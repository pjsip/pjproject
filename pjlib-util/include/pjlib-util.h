/* $Id */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJLIB_UTIL_H__
#define __PJLIB_UTIL_H__

#include <pjlib-util/dns.h>
#include <pjlib-util/errno.h>
#include <pjlib-util/getopt.h>
#include <pjlib-util/md5.h>
#include <pjlib-util/scanner.h>
#include <pjlib-util/stun.h>
#include <pjlib-util/xml.h>


PJ_BEGIN_DECL

/**
 * Initialize PJLIB UTIL (defined in errno.c)
 *
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjlib_util_init(void);


PJ_END_DECL


#endif	/* __PJLIB_UTIL_H__ */
