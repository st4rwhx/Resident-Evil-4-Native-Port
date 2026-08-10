/* SN Systems ProDG libsn crt0: the data half of the start-up object (the code is the `.init`
 * entry, lib/__start.s). Two 32-byte message buffers, the libsn version words and the
 * `__mod2i` link fiddle. GCC 2.95 -O2 -G 0 -fno-common, .data 8-byte aligned. */

extern int __mod2i(int, int);

/* the original object's .data is 8-byte aligned */
asm(".section .data\n\t.balign 8\n\t.section .text");

/* two 32-byte strings in one block (the debug symbol covers both) */
struct SnMessages {
	char wait[32];
	char version[32];
};

struct SnMessages lbl_80253A20 = {
	"Waiting for SN Debugger...\n",
	"<< libsn version %d >>\n",
};

int __SN_Libsn_version_60 = 0;  /* the version is in the name; the word is a link marker */
int __SN_Libsn_version = 60;

/* keeps __mod2i linked in */
static void *LinkFiddle[2] = { (void *) __mod2i, 0 };
