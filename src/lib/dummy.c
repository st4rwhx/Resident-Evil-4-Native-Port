/* SN Systems ProDG libsn: stdio system-call stubs (dummy.c). fd 1/2 go to the UART, everything else
 * to the host file server (PC* calls). GCC 2.95 -O2. */

extern void InitializeUART(int baud);
extern int WriteUARTN(const void *buf, unsigned int len);
extern int PCwrite(int fd, const void *buf, unsigned int len);
extern int PCclose(int fd);
extern int PClseek(int fd, int ofs, int whence);
extern int PCread(int fd, void *buf, unsigned int len);

/* the original object's .data is 8-byte aligned (the DOL places it at ...9D0 after a 4-aligned end) */
asm(".section .data\n\t.balign 8\n\t.section .text");

struct stat_min {
	int st_dev;
	int st_mode;
};

int __sn_serialp(const void *buf, unsigned int len)
{
	static int first = 1;

	if (first) {
		first = 0;
		InitializeUART(0);
	}
	WriteUARTN(buf, len);
	return len;
}

static int sn_stdio_pad = 0; /* unnamed 4-byte .data word after first.183 */

int _write(int fd, const void *buf, unsigned int len)
{
	if (fd == 1 || fd == 2) {
		return __sn_serialp(buf, len);
	}
	if (fd == 0) {
		return -1;
	}
	return PCwrite(fd, buf, len);
}

int write(int fd, const void *buf, unsigned int len)
{
	return _write(fd, buf, len);
}

int close(int fd)
{
	return PCclose(fd);
}

int fstat(int fd, struct stat_min *st)
{
	st->st_mode = 0x2000;
	return 0;
}

int lseek(int fd, int ofs, int whence)
{
	return PClseek(fd, ofs, whence);
}

int read(int fd, void *buf, unsigned int len)
{
	return PCread(fd, buf, len);
}
