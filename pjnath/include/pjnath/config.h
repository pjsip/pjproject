/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#ifndef __PJNATH_CONFIG_H__
#define __PJNATH_CONFIG_H__


/**
 * @file config.h
 * @brief Compile time settings
 */

/**
 * @defgroup PJNATH_CONFIG Configuration
 * @ingroup PJNATH
 * @{
 */

/* **************************************************************************
 * STUN CLIENT CONFIGURATION
 */

/**
 * Maximum number of attributes in the STUN packet (for the old STUN
 * library).
 *
 * Default: 16
 */
#ifndef PJSTUN_MAX_ATTR
#   define PJSTUN_MAX_ATTR			    16
#endif


/**
 * Maximum number of attributes in the STUN packet (for the new STUN
 * library).
 *
 * Default: 16
 */
#ifndef PJ_STUN_MAX_ATTR
#   define PJ_STUN_MAX_ATTR			    16
#endif


/**
 * @}
 */

#endif	/* __PJNATH_CONFIG_H__ */

