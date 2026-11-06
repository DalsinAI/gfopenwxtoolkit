/*
 * wxtk_module_rt.c: the wx module's own start and end, linked into
 * LIBS:openwxtoolkit/wx.module. The module has no C library of its own
 * (its calls go to the program's), so this is the part of libnix's start
 * code it needs: register the module's EH frames with the program's
 * unwinder, so exceptions pass through wx; then run its C++ constructors
 * (libnix's __initcpp: last to first); and the other way at the end.
 * The list heads come first in the module (mod_tables.s, wxtk-gen.py).
 * Copyright (c) 2026 AmiCradle Team
 */
typedef void (*wxtk_fn)(void);
extern const wxtk_fn __CTOR_LIST__[], __DTOR_LIST__[];
extern const long _EH_FRAME_BEGINS__[], _EH_FRAME_OBJECTS__[];
extern void __register_frame_info(const void *begin, void *object);
extern void *__deregister_frame_info(const void *begin);

void wxtk_module_init(void)
{
    const long *b = _EH_FRAME_BEGINS__ + 1, *o = _EH_FRAME_OBJECTS__ + 1;
    const wxtk_fn *p0 = __CTOR_LIST__ + 1, *p;
    while (*b) __register_frame_info((const void *)*b++, (void *)*o++);
    for (p = p0; *p; p++) {
    }
    while (p > p0) (*--p)();
}

void wxtk_module_exit(void)
{
    const wxtk_fn *p = __DTOR_LIST__ + 1;
    const long *b = _EH_FRAME_BEGINS__ + 1;
    while (*p) (*p++)();
    while (*b) __deregister_frame_info((const void *)*b++);
}
