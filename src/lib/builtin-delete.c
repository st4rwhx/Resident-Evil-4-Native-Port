/* SN Systems ProDG libstdc++ stub: the operator new/delete fallbacks (`__builtin_new`,
 * `__builtin_vec_new`, `__builtin_delete`, `__builtin_vec_delete`) that print a library warning when
 * the program defines no operator of its own. The DOL keeps only their strings (0x168 of .rodata:
 * the "bad_alloc" type name and the four warnings, word-aligned); the bodies were dead-stripped, so
 * they are static here and strip_unused drops them the same way. */

int printf(const char*, ...);
void abort(void);

static const char* bad_alloc_name(void)
{
	return "bad_alloc";
}

static void* builtin_new(unsigned int size)
{
	printf("\n*** Library warning ***\nYou must define an operator new\nsee Pro-DG documentation\n");
	abort();
	return 0;
}

static void* builtin_vec_new(unsigned int size)
{
	printf("\n*** Library warning ***\nYou must define an operator new[]\nsee Pro-DG documentation\n");
	abort();
	return 0;
}

static void builtin_delete(void* p)
{
	printf("\n*** Library warning ***\nYou must define an operator delete\nsee Pro-DG documentation\n");
	abort();
}

static void builtin_vec_delete(void* p)
{
	printf("\n*** Library warning ***\nYou must define an operator delete[]\nsee Pro-DG documentation\n");
	abort();
}
