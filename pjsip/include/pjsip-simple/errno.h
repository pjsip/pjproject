/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#ifndef __PJSIP_SIMPLE_ERRNO_H__
#define __PJSIP_SIMPLE_ERRNO_H__


#include <pjsip/sip_errno.h>

/**
 * @hideinitializer
 * No event package with the specified name.
 */
#define PJSIP_SIMPLE_ENOPKG	    -1
/**
 * @hideinitializer
 * Event package already exists.
 */
#define PJSIP_SIMPLE_EPKGEXISTS	    -1


/**
 * @hideinitializer
 * Expecting SUBSCRIBE request
 */
#define PJSIP_SIMPLE_ENOTSUBSCRIBE  -1
/**
 * @hideinitializer
 * No presence associated with subscription
 */
#define PJSIP_SIMPLE_ENOPRESENCE    -1
/**
 * @hideinitializer
 * No presence info in server subscription
 */
#define PJSIP_SIMPLE_ENOPRESENCEINFO -1
/**
 * @hideinitializer
 * Bad Content-Type
 */
#define PJSIP_SIMPLE_EBADCONTENT    -1
/**
 * @hideinitializer
 * Bad PIDF Message
 */
#define PJSIP_SIMPLE_EBADPIDF	    -1
/**
 * @hideinitializer
 * Bad XPIDF Message
 */
#define PJSIP_SIMPLE_EBADXPIDF	    -1



#endif	/* __PJSIP_SIMPLE_ERRNO_H__ */

