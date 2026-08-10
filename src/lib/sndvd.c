/* SN Systems ProDG libsn: DVD drive emulation over the host file server (sndvd.c). A DABR data
 * breakpoint on the DI control register turns every DVD command the game issues into a DSI
 * exception; the handler executes the command against the host file (PCreadAsyncInit) and
 * raises the completion interrupt by hand. GCC 2.95 -O2 -G 0 -fno-common. */

typedef unsigned char u8;
typedef unsigned int u32;

typedef struct OSContext {
	u32 gpr[32];
	u32 cr;
	u32 lr;
	u32 ctr;
	u32 xer;
	double fpr[32];
	u32 fpscr_pad;
	u32 fpscr;
	u32 srr0;
	u32 srr1;
} OSContext;

extern void OSReport(const char *msg, ...);
extern void OSLoadContext(OSContext *ctx);
extern void *__OSSetExceptionHandler(int exc, void *handler);
extern int PCreadAsyncInit(int fd, u32 addr, u32 len, void (*cb)(int), u32 ofs, int flag);

/* the original object's .data is 8-byte aligned (one 4-byte pad word after g_hDVD) */
asm(".section .data\n\t.balign 8\n\t.section .text");

/* DI (disc interface) registers */
#define DI_REG(n) (*(volatile u32 *)(0xCC006000 + (n) * 4))
#define DI_PHYS 0x0C006000
#define UNCACHED(a) ((volatile u32 *)((a) | 0xC0000000))
#define DI_DICR_ADDR 0xCC00601C

#define DVD_ERR_READ 0x00031100
#define DVD_ERR_SEEK 0x00052100
#define DVD_ERR_CMD 0x00052000

int g_hDVD = 0;

int g_nDvdAudioCfg;
u8 g_aBuffer[32] __attribute__((aligned(32)));
static int g_nDvdError;
static int g_nDvdCurrOffset;
int g_nDvdReadLength;
int g_nBaseOffset;
static u8 g_nEmuState;
u8 g_nLidState;
static void *g_OldDsiExcHandler;

void DisDvdBP(void)
{
	u32 dabr = 0;
	asm volatile("lwz 3, %0\n\tmtspr 1013, 3\n\tisync" : : "m"(dabr));
}

void EnaDvdBP(void)
{
	u32 dabr = DI_DICR_ADDR | 2;
	asm volatile("lwz 3, %0\n\tmtspr 1013, 3\n\tisync" : : "m"(dabr));
}

static void ForceDvdTcIrq(u32 addr)
{
	DisDvdBP();
	DI_REG(2) = 0x12000000;
	DI_REG(3) = 0;
	DI_REG(4) = 0x20;
	DI_REG(6) = 0x20;
	DI_REG(5) = addr;
	DI_REG(7) = 3;
	EnaDvdBP();
}

void ForceDvdDeIrq(void)
{
	DisDvdBP();
	DI_REG(2) = 0;
	DI_REG(7) = 1;
	EnaDvdBP();
}

static void DvdCallback(int len)
{
	len = (len + 31) & ~31;
	if (len == g_nDvdReadLength) {
		g_nDvdError = 0;
		g_nDvdCurrOffset += len;
		ForceDvdTcIrq((u32)g_aBuffer);
	} else {
		g_nDvdCurrOffset = -1;
		g_nDvdError = DVD_ERR_READ;
		ForceDvdDeIrq();
	}
}

int CheckSeekOffset(int ofs)
{
	if (ofs > 0x5705FFFF) {
		return DVD_ERR_SEEK;
	}
	return 0;
}

void DSIHandler(int exc, OSContext *ctx, u32 dsisr, u32 dar);
extern void DSIExcHandler(void);

/* DSI exception entry: DABR hits (DSISR bit 9) save the context and go to DSIHandler, other DSIs
 * go to the previous handler (or the debugger's DSIentry when it is installed). */
asm("	.type DSIExcHandler,@function\n"
    "DSIExcHandler:\n"
    "	mfdsisr 5\n"
    "	rlwinm. 5,5,10,31,31\n"
    "	beq NotDvdDsi\n"
    "	stw 0,0(4)\n"
    "	stw 1,4(4)\n"
    "	stw 2,8(4)\n"
    "	stmw 6,24(4)\n"
    "	mfspr 0,913\n"
    "	stw 0,424(4)\n"
    "	mfspr 0,914\n"
    "	stw 0,428(4)\n"
    "	mfspr 0,915\n"
    "	stw 0,432(4)\n"
    "	mfspr 0,916\n"
    "	stw 0,436(4)\n"
    "	mfspr 0,917\n"
    "	stw 0,440(4)\n"
    "	mfspr 0,918\n"
    "	stw 0,444(4)\n"
    "	mfspr 0,919\n"
    "	stw 0,448(4)\n"
    "	mfdsisr 5\n"
    "	mfdar 6\n"
    "	b DSIHandler\n"
    "NotDvdDsi:\n"
    "	lis 5,SN_DSI@h\n"
    "	ori 5,5,SN_DSI@l\n"
    "	lwz 5,0(5)\n"
    "	cmpwi 5,0\n"
    "	bne 1f\n"
    "	lis 5,g_OldDsiExcHandler@h\n"
    "	ori 5,5,g_OldDsiExcHandler@l\n"
    "	lwz 5,0(5)\n"
    "	mtlr 5\n"
    "	blr\n"
    "1:\n"
    "	lwz 3,128(4)\n"
    "	mtcrf 255,3\n"
    "	lwz 3,132(4)\n"
    "	mtlr 3\n"
    "	lwz 3,136(4)\n"
    "	mtctr 3\n"
    "	lwz 3,140(4)\n"
    "	mtxer 3\n"
    "	lwz 3,408(4)\n"
    "	mtsrr0 3\n"
    "	lwz 3,412(4)\n"
    "	mtsrr1 3\n"
    "	lwz 3,12(4)\n"
    "	lwz 5,20(4)\n"
    "	lwz 4,16(4)\n"
    "	b DSIentry\n"
    "	li 3,0\n"
    "	li 4,0\n"
    "	li 5,0\n"
    "	li 6,0\n"
    "	bl DSIHandler\n"
    "	.size DSIExcHandler,.-DSIExcHandler\n");

void DSIHandler(int exc, OSContext *ctx, u32 dsisr, u32 dar)
{
	u32 insn;
	u32 ea;
	u32 di[10];
	int pos;
	int cmd;
	int i;

	insn = *(u32 *)ctx->srr0;
	ea = insn & 0xFFFF;
	if (insn & 0x8000) {
		ea |= 0xFFFF0000;
	}
	ea += ctx->gpr[(insn >> 16) & 0x1F];
	if (dar == DI_DICR_ADDR) {
		for (i = 0; i < 10; i++) {
			di[i] = *UNCACHED(DI_PHYS + i * 4);
		}
		pos = g_nBaseOffset + (di[3] << 2);
		cmd = di[2] >> 16;
		switch (cmd) {
		case 0x1200:
			ForceDvdTcIrq(di[5]);
			break;
		case 0xA800:
			if (di[6] == 0) {
				ForceDvdTcIrq((u32)g_aBuffer);
			} else {
				g_nDvdError = CheckSeekOffset(pos);
				if (g_nDvdError == 0) {
					g_nDvdReadLength = di[6];
					if (PCreadAsyncInit(g_hDVD, di[5], di[6], DvdCallback, pos, 1) == -1) {
						g_nDvdError = DVD_ERR_READ;
					}
				}
			}
			if (g_nDvdError != 0) {
				ForceDvdDeIrq();
			}
			break;
		case 0xE000:
			DI_REG(8) = g_nDvdError;
			g_nDvdError &= 0xFF000000;
			ForceDvdTcIrq((u32)g_aBuffer);
			break;
		case 0xE100:
		case 0xE200:
		case 0xE401:
			if (g_nDvdAudioCfg != 0) {
				goto passthru;
			}
		case 0xAB00:
		case 0xE300:
			ForceDvdTcIrq((u32)g_aBuffer);
			break;
		default:
			OSReport("Unknown DVD cmd (%08X)\n", di[2]);
			__asm__(".long 1");
			g_nDvdError = DVD_ERR_CMD;
			ForceDvdDeIrq();
			/* COMPILER-DIFF: #6 (the original does not cross-jump this single-insn `bl` tail
			 * into the 0xA800 case's; the empty asm keeps the two copies apart) */
			asm volatile("");
			break;
		}
	} else {
passthru:
		DisDvdBP();
		*(u32 *)ea = ctx->gpr[(insn >> 21) & 0x1F];
		EnaDvdBP();
	}
	ctx->srr0 += 4;
	/* REGALLOC LEVER (OPEN): with 7 references `ctx` (7 refs / 153 insns) ranks below `pos`
	 * (3 refs / 29 insns) in global.c's allocno priority and gets r28; the original has ctx in
	 * r29 and pos in r28, i.e. one more reference of ctx. No natural source form found yet. */
	asm volatile("" : : "r"(ctx));
	OSLoadContext(ctx);
}

void SNDVDEmuInit(int h)
{
	if (h != -0x8000) {
		return;
	}
	g_hDVD = h;
}

void SNDVDEmuInitDSIHandler(void)
{
	if (g_hDVD != -0x8000) {
		return;
	}
	g_nDvdAudioCfg = 0;
	g_nDvdError = 0;
	g_nDvdCurrOffset = -1;
	g_nBaseOffset = 0;
	g_nLidState = 1;
	g_nEmuState = 1;
	EnaDvdBP();
	g_OldDsiExcHandler = __OSSetExceptionHandler(2, (void *)DSIExcHandler);
}

u32 SNDVDEmuControl(int cmd)
{
	u32 ret = 0;

	if (g_hDVD != -0x8000) {
		return 0;
	}
	switch (cmd) {
	case 0:
		g_nLidState = 0;
		break;
	case 1:
		g_nLidState = 1;
		break;
	case 2:
		g_nEmuState = 0;
		DisDvdBP();
		ret = 0x80000000;
		break;
	case 3:
		g_nEmuState = 1;
		EnaDvdBP();
		ret = 0x80000000;
		break;
	}
	return ret | (g_nEmuState << 8) | g_nLidState;
}
