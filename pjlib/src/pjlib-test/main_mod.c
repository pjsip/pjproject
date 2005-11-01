/* $Header: /pjproject-0.3/pjlib/src/pjlib-test/main_mod.c 2     10/29/05 11:51a Bennylp $
 */
/* 
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/main_mod.c $
 * 
 * 2     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 1     10/05/05 5:12p Bennylp
 * Created.
 *
 */
#include "test.h"
#include <linux/module.h>
#include <linux/kernel.h>

int init_module(void)
{
    printk(KERN_INFO "PJLIB test module loaded. Starting tests...\n");
    
    test_main();

    /* Prevent module from loading. We've finished test anyway.. */
    return 1;
}

void cleanup_module(void)
{
    printk(KERN_INFO "PJLIB test module unloading...\n");
}

MODULE_LICENSE("GPL");

