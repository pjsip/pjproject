/* $Id$
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

