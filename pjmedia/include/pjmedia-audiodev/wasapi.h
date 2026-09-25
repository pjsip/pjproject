/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
 * Contributed by Andreas Ahland
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
#ifndef __PJMEDIA_AUDIODEV_WASAPI_H__
#define __PJMEDIA_AUDIODEV_WASAPI_H__

/**
 * @file pjmedia-audiodev/wasapi.h
 * @brief WASAPI Audio Device.
 */

#include <pjmedia/audiodev.h>

/**
 * @defgroup PJMED_AUDDEV_WASAPI WASAPI Audio Device
 * @ingroup audio_device_api
 * @brief WASAPI specific Audio Device API
 * @{
 *
 * This section describes specific functions for the Windows desktop WASAPI
 * audio device, see #PJMEDIA_AUDIO_DEV_HAS_WASAPI, which is disabled by
 * default there.
 *
 * Note that this is available on the Windows desktop only. UWP and Windows
 * Phone 8 have PJMEDIA_AUDIO_DEV_HAS_WASAPI enabled but are served by a
 * different implementation, which does not provide this API.
 */

PJ_BEGIN_DECL


/* Declared only where wasapi_dev_win.cpp provides it, so that using it on a
 * target served by wasapi_dev.cpp is a compile error rather than a link one.
 * Keep in step with the guard at the top of that file.
 */
#if defined(PJMEDIA_AUDIO_DEV_HAS_WASAPI) && \
    PJMEDIA_AUDIO_DEV_HAS_WASAPI != 0 && \
    defined(PJ_WIN32) && PJ_WIN32 != 0 && \
    !(defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE != 0) && \
    !(defined(PJ_WIN32_UWP) && PJ_WIN32_UWP != 0) && \
    !(defined(PJ_WIN32_WINPHONE8) && PJ_WIN32_WINPHONE8 != 0)

/**
 * Get the WASAPI endpoint ID of a device of this backend, so that an
 * application can open the same endpoint itself, e.g: with an IAudioClient
 * of its own next to the streams created through the audio device API. The
 * generic API does not expose the endpoint, and the device name cannot serve
 * as a substitute: it is converted to ANSI and truncated to
 * #PJMEDIA_AUD_DEV_INFO_NAME_LEN, so two endpoints may well share it.
 *
 * When both a default capture and a default render endpoint exist, the
 * first device of the backend stands for them and has no fixed ID of its
 * own. Otherwise there is no such entry and every device is a fixed
 * endpoint, so check \a is_default rather than the index. For the default
 * entry, \a buf is left empty and \a is_default is set to PJ_TRUE, and the
 * application is expected to call
 * IMMDeviceEnumerator::GetDefaultAudioEndpoint() instead. Pass eConsole as
 * the role, which is the one this backend uses: eCommunications may well
 * resolve to another endpoint than the one the pjmedia stream is on.
 *
 * Note also that a stream binds the default endpoints when it is created and
 * stays on them, so if the Windows default changes while a stream is running,
 * GetDefaultAudioEndpoint() no longer reports the endpoint that stream uses.
 *
 * The ID is returned as an array of pj_uint16_t rather than of WCHAR, so
 * that this header does not have to include windows.h. It is a null
 * terminated wide string and can be passed to
 * IMMDeviceEnumerator::GetDevice() as is.
 *
 * @param id            The device index. #PJMEDIA_AUD_DEFAULT_CAPTURE_DEV
 *                      and #PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV are accepted
 *                      and resolved as the rest of the API resolves them,
 *                      which may well land on another backend, in which case
 *                      PJMEDIA_EAUD_INVDEV is returned.
 * @param capture       PJ_TRUE for the capture endpoint, PJ_FALSE for the
 *                      render endpoint.
 * @param buf           Buffer receiving the null terminated endpoint ID.
 * @param len           Size of \a buf in characters.
 * @param is_default    Set to PJ_TRUE when the device stands for the
 *                      default endpoints.
 *
 * @return              PJ_SUCCESS on success, PJMEDIA_EAUD_INVDEV when the
 *                      index does not belong to this backend or the device
 *                      has no endpoint in the requested direction, or
 *                      PJ_ETOOSMALL when the ID does not fit in \a buf.
 */
PJ_DECL(pj_status_t) pjmedia_wasapi_get_endpoint(pjmedia_aud_dev_index id,
                                                 pj_bool_t capture,
                                                 pj_uint16_t *buf,
                                                 unsigned len,
                                                 pj_bool_t *is_default);

#endif  /* Windows desktop WASAPI */


PJ_END_DECL


/**
 * @}
 */

#endif  /* __PJMEDIA_AUDIODEV_WASAPI_H__ */
