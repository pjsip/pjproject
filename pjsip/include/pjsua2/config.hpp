/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
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
 * @file pjsua2/config.hpp
 * @brief PJSUA2 Base Agent Operation
 */
#include <pjsua-lib/pjsua.h>

/**
 * @defgroup PJSUA2_CFG_Compile Compile time settings
 * @ingroup PJSUA2_DS
 * @{
 */

/**
 * Specify if the Error exception info should contain operation and source
 * file information.
 */
#ifndef PJSUA2_ERROR_HAS_EXTRA_INFO
#   define PJSUA2_ERROR_HAS_EXTRA_INFO          1
#endif

/**
 * Maximum buffer length to print SDP content for SdpSession. Set this to 0
 * if the printed SDP is not needed.
 */
#ifndef PJSUA2_MAX_SDP_BUF_LEN
#   define PJSUA2_MAX_SDP_BUF_LEN               1024
#endif

/**
 * Ticket #2189 described some lists of objects which is not thread safe.
 * The ticket deprecated some APIs which uses those lists and introduce new one
 * to replace them. This settings will disable the deprecated API all together.
 * See also https://github.com/pjsip/pjproject/issues/2232
 */
#ifndef DEPRECATED_FOR_TICKET_2232
#   define DEPRECATED_FOR_TICKET_2232           1
#endif

/**
 * C++11 deprecated dynamic exception specification, but SWIG needs it.
 */
#ifndef SWIG
#   define PJSUA2_THROW(x)
#else
#   define PJSUA2_THROW(x) throw(x)
#endif

/**
 * @}  PJSUA2_CFG
 */

#endif  /* __PJSUA2_CONFIG_HPP__ */
