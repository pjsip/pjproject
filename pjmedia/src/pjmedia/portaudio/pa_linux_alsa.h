#ifndef PA_LINUX_ALSA_H
#define PA_LINUX_ALSA_H

/*
 * $Id: pa_linux_alsa.h,v 1.1.2.12 2004/09/25 14:15:25 aknudsen Exp $
 * PortAudio Portable Real-Time Audio Library
 * ALSA-specific extensions
 *
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/** @file
 * ALSA-specific PortAudio API extension header file.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PaAlsaStreamInfo
{
    unsigned long size;
    PaHostApiTypeId hostApiType;
    unsigned long version;

    const char *deviceString;
}
PaAlsaStreamInfo;

void PaAlsa_InitializeStreamInfo( PaAlsaStreamInfo *info );

void PaAlsa_EnableRealtimeScheduling( PaStream *s, int enable );

void PaAlsa_EnableWatchdog( PaStream *s, int enable );

#ifdef __cplusplus
}
#endif

#endif
