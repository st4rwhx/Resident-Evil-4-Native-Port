/* SN ProDG libgcc: _exit traps into the debugger with an illegal instruction. */
void _exit(int code)
{
    __asm__(".long 0");
}
