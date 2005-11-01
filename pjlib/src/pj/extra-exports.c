/* $Id$
 *
 */
/*
 * This file contains code to export extra symbols from Linux kernel.
 * It should be copied to Linux kernel source tree and added to
 * Linux kernel combilation.
 *
 * This file is part of PJLIB project.
 */
#include <linux/module.h>
#include <linux/syscalls.h>

EXPORT_SYMBOL(sys_select);

EXPORT_SYMBOL(sys_epoll_create);
EXPORT_SYMBOL(sys_epoll_ctl);
EXPORT_SYMBOL(sys_epoll_wait);

EXPORT_SYMBOL(sys_socket);
EXPORT_SYMBOL(sys_bind);
EXPORT_SYMBOL(sys_getpeername);
EXPORT_SYMBOL(sys_getsockname);
EXPORT_SYMBOL(sys_sendto);
EXPORT_SYMBOL(sys_recvfrom);
EXPORT_SYMBOL(sys_getsockopt);
EXPORT_SYMBOL(sys_setsockopt);
EXPORT_SYMBOL(sys_listen);
EXPORT_SYMBOL(sys_shutdown);
EXPORT_SYMBOL(sys_connect);
EXPORT_SYMBOL(sys_accept);

