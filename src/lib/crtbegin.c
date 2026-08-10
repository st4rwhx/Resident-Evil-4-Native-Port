/* SN Systems ProDG crt: the constructor/destructor list heads (gcc crtstuff.c), one -1 sentinel
 * each at the start of .ctor / .dtor; __main walks the lists from crtend's zero terminator down. */

typedef void (*func_ptr)(void);

func_ptr __CTOR_LIST__[1] __attribute__((section(".ctor"))) = { (func_ptr) -1 };
func_ptr __DTOR_LIST__[1] __attribute__((section(".dtor"))) = { (func_ptr) -1 };
