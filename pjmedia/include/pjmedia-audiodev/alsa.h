/* 
 * Copyright (C) 2024 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_AUDIODEV_ALSA_H__
#define __PJMEDIA_AUDIODEV_ALSA_H__

/**
 * @file pjmedia-audiodev/alsa.h
 * @brief ALSA Audio Device.
 */

#include <pjmedia/audiodev.h>

/**
 * @defgroup PJMED_AUDDEV_ALSA ALSA Audio Device
 * @ingroup audio_device_api
 * @brief ALSA specific Audio Device API
 * @{
 *
 * This section describes specific functions for ALSA audio devices.
 * Application can use @ref PJMEDIA_AUDIODEV_API API to manipulate
 * the ALSA audio device.
 *
 */

PJ_BEGIN_DECL


/**
 * Manually set ALSA devices. This function will remove all devices registered
 * in the factory and register the specified devices.
 *
 * Note that by default the library will automatically try to enumerate and
 * register the ALSA devices during factory initialization. Application can
 * override the registered devices using this function.
 *
 * If application wish to let the library do the device enumeration again,
 * just call this function with zero device, i.e: \a count is set to zero.
 *
 * @param af        The ALSA factory, or NULL to use the default.
 * @param count     The number of ALSA device names.
 * @param names     The ALSA device names to be registered.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_aud_alsa_set_devices(pjmedia_aud_dev_factory *af,
                                                  unsigned count,
                                                  const char* names[]);



PJ_END_DECL


/**
 * @}
 */

#endif  /* __PJMEDIA_AUDIODEV_ALSA_H__ */

