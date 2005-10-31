#include <pj/config.h>
#include <pj/compat/setjmp.h>

int __sigjmp_save(sigjmp_buf env, int savemask)
{
    return 0;
}

extern int __sigsetjmp(pj_jmp_buf env, int savemask);
extern void __longjmp(pj_jmp_buf env, int val) __attribute__((noreturn));

PJ_DEF(int) pj_setjmp(pj_jmp_buf env)
{
    return __sigsetjmp(env, 0);
}

PJ_DEF(void) pj_longjmp(pj_jmp_buf env, int val)
{
    __longjmp(env, val);
}

