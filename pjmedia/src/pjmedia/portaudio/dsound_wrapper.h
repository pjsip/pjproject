#ifndef __DSOUND_WRAPPER_H
#define __DSOUND_WRAPPER_H
/*
 * $Id: dsound_wrapper.h,v 1.1.1.1.2.8 2005/01/16 20:48:37 rossbencina Exp $
 * Simplified DirectSound interface.
 *
 * Author: Phil Burk & Robert Marsanyi
 *
 * For PortAudio Portable Real-Time Audio Library
 * For more information see: http://www.softsynth.com/portaudio/
 * DirectSound Implementation
 * Copyright (c) 1999-2000 Phil Burk & Robert Marsanyi
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

/* on Borland compilers, WIN32 doesn't seem to be defined by default, which
    breaks DSound.h. Adding the define here fixes the problem. - rossb. */
#ifdef __BORLANDC__
#if !defined(WIN32)
#define WIN32
#endif
#endif

/*
  We are only using DX3 in here, no need to polute the namespace - davidv
*/
#define DIRECTSOUND_VERSION 0x0300

#include <DSound.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


typedef struct
{
    HINSTANCE hInstance_;
    
    HRESULT (WINAPI *DirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
    HRESULT (WINAPI *DirectSoundEnumerateW)(LPDSENUMCALLBACKW, LPVOID);
    HRESULT (WINAPI *DirectSoundEnumerateA)(LPDSENUMCALLBACKA, LPVOID);

    HRESULT (WINAPI *DirectSoundCaptureCreate)(LPGUID, LPDIRECTSOUNDCAPTURE *, LPUNKNOWN);
    HRESULT (WINAPI *DirectSoundCaptureEnumerateW)(LPDSENUMCALLBACKW, LPVOID);
    HRESULT (WINAPI *DirectSoundCaptureEnumerateA)(LPDSENUMCALLBACKA, LPVOID);
}DSoundEntryPoints;

extern DSoundEntryPoints dswDSoundEntryPoints;

void DSW_InitializeDSoundEntryPoints(void);
void DSW_TerminateDSoundEntryPoints(void);

#define DSW_NUM_POSITIONS     (4)
#define DSW_NUM_EVENTS        (5)
#define DSW_TERMINATION_EVENT     (DSW_NUM_POSITIONS)

typedef struct
{
/* Output */
    LPDIRECTSOUND        dsw_pDirectSound;
    LPDIRECTSOUNDBUFFER  dsw_OutputBuffer;
    DWORD                dsw_WriteOffset;     /* last write position */
    INT                  dsw_OutputSize;
    INT                  dsw_BytesPerOutputFrame;
    /* Try to detect play buffer underflows. */
    LARGE_INTEGER        dsw_CounterTicksPerBuffer; /* counter ticks it should take to play a full buffer */
    LARGE_INTEGER        dsw_LastPlayTime;
    UINT                 dsw_LastPlayCursor;
    UINT                 dsw_OutputUnderflows;
    BOOL                 dsw_OutputRunning;
    /* use double which lets us can play for several thousand years with enough precision */
    double               dsw_FramesWritten;
    double               dsw_FramesPlayed;
/* Input */
    INT                  dsw_BytesPerInputFrame;
    LPDIRECTSOUNDCAPTURE dsw_pDirectSoundCapture;
    LPDIRECTSOUNDCAPTUREBUFFER   dsw_InputBuffer;
    UINT                 dsw_ReadOffset;      /* last read position */
    UINT                 dsw_InputSize;
} DSoundWrapper;

HRESULT DSW_Init( DSoundWrapper *dsw );
void DSW_Term( DSoundWrapper *dsw );
HRESULT DSW_InitOutputBuffer( DSoundWrapper *dsw, unsigned long nFrameRate,
                              WORD nChannels, int bufSize );
HRESULT DSW_StartOutput( DSoundWrapper *dsw );
HRESULT DSW_StopOutput( DSoundWrapper *dsw );
DWORD   DSW_GetOutputStatus( DSoundWrapper *dsw );
HRESULT DSW_WriteBlock( DSoundWrapper *dsw, char *buf, long numBytes );
HRESULT DSW_ZeroEmptySpace( DSoundWrapper *dsw );
HRESULT DSW_QueryOutputSpace( DSoundWrapper *dsw, long *bytesEmpty );
HRESULT DSW_Enumerate( DSoundWrapper *dsw );

HRESULT DSW_InitInputBuffer( DSoundWrapper *dsw, unsigned long nFrameRate,
                             WORD nChannels, int bufSize );
HRESULT DSW_StartInput( DSoundWrapper *dsw );
HRESULT DSW_StopInput( DSoundWrapper *dsw );
HRESULT DSW_ReadBlock( DSoundWrapper *dsw, char *buf, long numBytes );
HRESULT DSW_QueryInputFilled( DSoundWrapper *dsw, long *bytesFilled );
HRESULT DSW_QueryOutputFilled( DSoundWrapper *dsw, long *bytesFilled );

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif  /* __DSOUND_WRAPPER_H */
