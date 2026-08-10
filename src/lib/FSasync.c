/* SN Systems ProDG libsn: asynchronous host file-server reads over EXI channel 2 (FSasync.c).
 * GCC 2.95 -O2 -G 0. The transfer state is kept in volatile globals shared with the EXI2 TC
 * interrupt handler. */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

extern int OSDisableInterrupts(void);
extern int OSRestoreInterrupts(int level);
extern void *__OSSetInterruptHandler(short interrupt, void *handler);
extern u32 __OSUnmaskInterrupts(u32 mask);

extern int SNQueryData(void);
extern int SNRead(void *buf, int len);
extern int SNWrite(const void *buf, int len);
extern void SNDVDRead_init(void);
extern void SNDVDReadAsync_next(void *buf, u32 len);
extern void SNDVDReadSync_next(void *buf, u32 len);
extern void SNDVDWrite_init(u32 len);
extern void SNDVDWriteNoDMA_next(const void *buf, u32 len);

typedef void (*FSCBFunc)(int result);

/* EXI channel 2 registers and the PI interrupt cause register */
#define PI_INTSR (*(volatile u32 *)0xCC003000)
#define EXI2_CSR (*(volatile u32 *)0xCC006828)
#define EXI2_CR (*(volatile u32 *)0xCC006834)

/* one 1 KiB DMA block per EXI transfer, at most 0x1FC00 bytes per file-server request */
#define FS_BLOCK_SIZE 0x400
#define FS_MAX_REQUEST 0x1FC00

/* host file-server request header (little-endian host, hence the byte-swapped length) */
struct FSReadHeader {
	u8 cmd;
	u8 pad;
	u16 len;
	u32 zero;
	u8 subcmd;
	u8 pad2[3];
	int handle;
	int offset;
	u32 size;
};

/* file-server reply: two 16-bit words, then the result word */
struct FSResult {
	u16 w0;
	u16 w1;
	u32 w2;
	int err;
} __attribute__((aligned(32)));

volatile int g_nRWasyncPhase = 0;
static volatile FSCBFunc g_FSCBFunc = 0;
volatile int g_nEXI2TCCnt = 0;
volatile int g_nDbgFsAsyncCnt = 0;

static int g_hHandle;
static u8 *g_pBuffer;
volatile int g_nBlockCnt;
volatile int g_nRemainderCnt;
volatile u32 g_nTotalBytesRemaining;
volatile int g_bDoFSACK;
volatile int g_nFSLastError;
struct FSResult g_FsResult;

void PCrwSyncFSACK(void);
int CompleteAsync(void);

int PCwriteAsyncInit(void)
{
	return -1;
}

static void DoFSReadHeader(int handle, void *buf, u32 len, int offset)
{
	struct FSReadHeader hdr;
	u16 hlen = 0x10;
	u8 *p = (u8 *)&hdr;
	int i;

	for (i = 0; i < sizeof(hdr); i++) {
		*p++ = 0;
	}
	hdr.cmd = 9;
	hdr.len = hlen << 8;
	if (g_bDoFSACK) {
		hdr.subcmd = 9;
	} else {
		hdr.subcmd = 11;
	}
	hdr.handle = handle;
	hdr.offset = offset;
	hdr.size = len;
	SNDVDWrite_init(sizeof(hdr));
	SNDVDWriteNoDMA_next(&hdr, sizeof(hdr));
}

void InitReadCounts(void)
{
	u32 n;

	if (g_nTotalBytesRemaining > FS_MAX_REQUEST) {
		n = FS_MAX_REQUEST;
	} else {
		n = g_nTotalBytesRemaining;
	}
	g_nTotalBytesRemaining -= n;
	g_nRWasyncPhase = 2;
	g_nBlockCnt = n >> 10;
	g_nRemainderCnt = n & (FS_BLOCK_SIZE - 1);
}

void PCreadAsyncNext(void)
{
	u8 *buf = g_pBuffer;
	u32 len;

	PI_INTSR = 0x1000;
	InitReadCounts();
	if (g_nBlockCnt) {
		len = FS_BLOCK_SIZE;
		g_pBuffer += FS_BLOCK_SIZE;
		g_nBlockCnt--;
	} else {
		len = g_nRemainderCnt;
		g_nRemainderCnt = 0;
	}
	SNDVDRead_init();
	SNDVDReadAsync_next(buf, len);
}

int PCreadAsyncInit(int handle, void *buf, u32 len, FSCBFunc cb, int offset, int doAck)
{
	if ((u32)buf & 0x1F) {
		return -1;
	}
	if (len & 0x1F) {
		return -1;
	}
	CompleteAsync();
	g_bDoFSACK = doAck;
	g_FSCBFunc = cb;
	DoFSReadHeader(handle, buf, len, offset);
	g_nTotalBytesRemaining = len;
	g_hHandle = handle;
	g_pBuffer = buf;
	g_nRWasyncPhase = 1;
	return 0;
}

void ReadSyncNext(void)
{
	u8 *buf = g_pBuffer;
	u32 len;

	while (SNQueryData() == 0) {
	}
	PI_INTSR = 0x1000;
	InitReadCounts();
	if (g_nBlockCnt) {
		len = FS_BLOCK_SIZE;
		g_nBlockCnt--;
		g_pBuffer += FS_BLOCK_SIZE;
	} else {
		len = g_nRemainderCnt;
		g_nRemainderCnt = 0;
	}
	SNDVDRead_init();
	SNDVDReadSync_next(buf, len);
}

void CompletePCreadAsync(void)
{
	if (g_nRWasyncPhase == 1) {
		ReadSyncNext();
	}
	while (EXI2_CR & 1) {
	}
	for (;;) {
		g_nBlockCnt--;
		if (g_nBlockCnt == -1) {
			break;
		}
		SNDVDReadSync_next(g_pBuffer, FS_BLOCK_SIZE);
		g_pBuffer += FS_BLOCK_SIZE;
	}
	if (g_nRemainderCnt) {
		SNDVDReadSync_next(g_pBuffer, g_nRemainderCnt);
		g_nRemainderCnt = 0;
	}
	EXI2_CSR = 0;
	while (g_nTotalBytesRemaining) {
		ReadSyncNext();
		for (;;) {
			g_nBlockCnt--;
			if (g_nBlockCnt == -1) {
				break;
			}
			SNDVDReadSync_next(g_pBuffer, FS_BLOCK_SIZE);
			g_pBuffer += FS_BLOCK_SIZE;
		}
		if (g_nRemainderCnt) {
			SNDVDReadSync_next(g_pBuffer, g_nRemainderCnt);
			g_nRemainderCnt = 0;
		}
		EXI2_CSR = 0;
	}
	PCrwSyncFSACK();
}

void PCrwAsyncFSACK(void)
{
	struct FSResult *res = &g_FsResult;
	FSCBFunc cb;
	int n;

	while ((n = SNQueryData()) == 0) {
	}
	SNRead(res, n);
	res->w1 = 0;
	g_nFSLastError = res->err;
	SNWrite(res, 8);
	cb = g_FSCBFunc;
	g_nRWasyncPhase = 0;
	if (cb) {
		cb(g_nFSLastError);
		g_FSCBFunc = 0;
	}
	PI_INTSR = 0x1000;
}

void PCrwSyncFSACK(void)
{
	struct FSResult *res = &g_FsResult;

	if (g_bDoFSACK) {
		FSCBFunc cb;
		int n;

		while ((n = SNQueryData()) == 0) {
		}
		SNRead(res, n);
		res->w1 = 0;
		g_nFSLastError = res->err;
		SNWrite(res, 8);
		cb = g_FSCBFunc;
		g_nRWasyncPhase = 0;
		if (cb) {
			cb(g_nFSLastError);
			g_FSCBFunc = 0;
		}
	}
	g_FSCBFunc = 0;
	EXI2_CSR = 8;
	PI_INTSR = 0x1000;
	g_nRWasyncPhase = 0;
}

int CompleteAsync(void)
{
	if (g_nRWasyncPhase == 0) {
		return 0;
	}
	if (g_nRWasyncPhase == 5) {
		PCrwSyncFSACK();
	} else {
		CompletePCreadAsync();
	}
	return g_nFSLastError;
}

void PCrwAsyncNextPh(u32 phase)
{
	switch (phase) {
	case 1:
		PCreadAsyncNext();
		break;
	case 2:
	case 4:
		g_nDbgFsAsyncCnt++;
		break;
	case 3:
		break;
	case 5:
		PCrwAsyncFSACK();
		break;
	}
}

void EXI2TCHandler(short interrupt, void *context)
{
	u8 *buf = g_pBuffer;
	FSCBFunc cb;
	u32 len;

	g_nEXI2TCCnt++;
	EXI2_CSR = 0x88;
	if (g_nRWasyncPhase == 0) {
		return;
	}
	if (g_nBlockCnt) {
		if (g_nRWasyncPhase == 4) {
			g_pBuffer = buf + 0x200;
			len = 0x200;
		} else {
			g_pBuffer = buf + FS_BLOCK_SIZE;
			len = FS_BLOCK_SIZE;
		}
		g_nBlockCnt--;
	} else if (g_nRemainderCnt) {
		len = g_nRemainderCnt;
		g_nRemainderCnt = 0;
	} else {
		EXI2_CSR = 0;
		if (g_nTotalBytesRemaining) {
			if (g_nRWasyncPhase != 4) {
				g_nRWasyncPhase = 1;
			}
		} else if (g_bDoFSACK) {
			g_nRWasyncPhase = 5;
		} else {
			cb = g_FSCBFunc;
			g_nRWasyncPhase = 0;
			g_nFSLastError = 0;
			if (cb) {
				cb(0);
				g_FSCBFunc = 0;
			}
		}
		return;
	}
	SNDVDReadAsync_next(buf, len);
}

void SNInitEXI2TCHandler(void)
{
	int level = OSDisableInterrupts();

	__OSSetInterruptHandler(16, EXI2TCHandler);
	__OSUnmaskInterrupts(0x8000);
	EXI2_CSR |= 8;
	EXI2_CSR &= ~4;
	OSRestoreInterrupts(level);
}
