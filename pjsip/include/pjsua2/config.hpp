/* $Id$ */
/*
 * Copyright (C) 2008-2012 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJSUA2_CONFIG_HPP__
#define __PJSUA2_CONFIG_HPP__

/**
 * @file pjsua2/ua.hpp
 * @brief PJSUA2 Base Agent Operation
 */
#include <pjsua-lib/pjsua.h>

/**
 * @defgroup PJSUA2_CFG_Compile Compile time settings
 * @ingroup PJSUA2_Ref
 * @{
 */

/**
 * Specify if the Error exception info should contain operation and source
 * file information.
 */
#ifndef PJSUA2_ERROR_HAS_EXTRA_INFO
#   define PJSUA2_ERROR_HAS_EXTRA_INFO		1
#endif


/**
 * @}  PJSUA2_CFG
 */

#endif	/* __PJSUA2_CONFIG_HPP__ */
