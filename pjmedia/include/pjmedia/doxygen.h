/* $Id$ */
/* 
 * Copyright (C)2003-2007 Benny Prijono <benny@prijono.org>
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
#ifndef __PJMEDIA_DOXYGEN_H__
#define __PJMEDIA_DOXYGEN_H__

/**
 * @file doxygen.h
 * @brief Doxygen's mainpage.
 */

/**
 * @defgroup PJMEDIA PJMEDIA Library
 */

/*////////////////////////////////////////////////////////////////////////// */
/*
	INTRODUCTION PAGE
 */

/**
 * @mainpage PJMEDIA and PJMEDIA-CODEC
 *
 * \n
 * @section intro_sec PJMEDIA
 *
 * PJMEDIA is a rather complete media stack, distributed under Open Source/GPL
 * terms, and featuring small footprint and good extensibility and portability.
 *
 * Please click the <A HREF="modules.htm"><b>Modules</b></A> link on top
 * of this page to get the complete features currently present in PJMEDIA.
 *
 * Also please read the documentation about @ref PJMEDIA_PORT_CONCEPT,
 * which is a major concept that is used for implementing many objects in 
 * the library.
 *
 * \n
 * @section pjmedia_codec_sec PJMEDIA-CODEC
 *
 * PJMEDIA-CODEC is a static library containing various codec implementations,
 * wrapped into PJMEDIA codec framework. The static library is designed as
 * such so that only codecs that are explicitly initialized are linked with 
 * the application, therefore keeping the application size in control.
 *
 * Please see @ref pjmedia_codec_page on how to use the codec in 
 * PJMEDIA-CODEC.
 *
 * \n
 * @section main_page_get_start_sec Getting Started
 *
 * For those who likes to just get start coding, the @ref getting_started_pjmedia
 * may be a good place to start.
 *
 * The @ref page_pjmedia_samples page describes some examples that are available
 * in the source tree.
 *
 *
 * \n
 * @section pjmedia_lic Copying and Acknowledgements
 *
 * PJMEDIA and PJMEDIA-CODEC contains various parts obtained from other
 * places, and each of these would have their own licensing terms.
 * Please see @ref lic_stuffs page for details.
 *
 */

/**
 * @page pjmed_keywords_page Features Index
 * @section pjmed_keywords Features Index
 *
 * <b>PJMEDIA features</b>, in no particular order (click to go to the relevant
 * documentation): 
 * @ref lic_stuffs "Open Source media stack", 
 * @ref PJMEDIA_CLOCK, 
 * @ref PJMEDIA_CODEC,
 * @ref enc_dec_codec,
 * @ref plc_codec, 
 * @ref PJMEDIA_CONF, 
 * @ref PJMED_G711 "G711/G.711 (PCMA/PCMU) codec with PLC",
 * @ref PJMED_GSM "GSM codec with PLC", 
 * @ref PJMED_L16 "linear codecs (multiple clockrate, stereo support, etc)",
 * @ref PJMED_SPEEX "Speex codec (narrowband, wideband, ultra-wideband)",
 * @ref PJMED_JBUF "portable, adaptive jitter buffer with PLC support",
 * @ref PJMEDIA_MASTER_PORT, 
 * @ref PJMEDIA_NULL_PORT,
 * @ref PJMED_PLC, 
 * @ref PJMEDIA_PORT_CONCEPT, 
 * @ref PJMEDIA_PORT_CLOCK,
 * @ref PJMEDIA_RESAMPLE "high quality resampling/sampling rate conversion",
 * @ref PJMEDIA_RESAMPLE_PORT, 
 * @ref PJMED_RTCP "small footprint, portable RTCP with media quality statistics",
 * @ref PJMED_RTP "very small footprint, modular, DSP ready RTP implementation", 
 * @ref PJMEDIA_SDP "modular, small footprint, open source SDP implementation", 
 * @ref PJMEDIA_SDP_NEG "modular SDP negotiation/negotiator abstraction",
 * @ref PJMED_SES "media session abstraction",
 * @ref PJMEDIA_SILENCEDET,
 * @ref PJMED_SND "portable audio/sound hardware/device abstraction for Linux, Unix, Windows, DirectSound, WinCE, Windows Mobile, MacOS X, etc.", 
 * @ref PJMED_SND_PORT,
 * @ref PJMEDIA_SPLITCOMB, 
 * @ref PJMED_STRM "remote stream", 
 * @ref PJMEDIA_TRANSPORT_H "custom media transport abstraction",
 * @ref PJMEDIA_TRANSPORT_UDP, 
 * @ref PJMEDIA_FILE_PLAY "WAV/WAVE file playback", 
 * @ref PJMEDIA_FILE_REC "WAV/WAVE file recording/capture",
 * @ref PJMEDIA_WAVE "portable WAV/WAVE header manipulation"
 */


/**
 * @page pjmedia_codec_page Using PJMEDIA-CODEC
 *
 * Before application can use a codec, it needs to initialize and register
 * the codec to the codec manager. This is accomplished with using
 * constructs like the following:
 *
 \code
    #include <pjmedia.h>
    #include <pjmedia-codec.h>

    init_codecs( pjmedia_endpt *med_ept )
    {
	// Register G.711 codecs
	pjmedia_codec_g711_init(med_ept);

	// Register GSM codec.
	pjmedia_codec_gsm_init(med_ept);

	// Register Speex codecs.
	// With the default flag, this will register three codecs:
	// speex/8000, speex/16000, and speex/32000
	pjmedia_codec_speex_init(med_ept, 0, 0, 0);
    }
 \endcode
 *
 * After the codec is registered, application may create the encoder/decoder
 * instance, by using the API as documented in @ref PJMEDIA_CODEC.
 */



/**
 * @page lic_stuffs Copying and Acknowledgements
 * @section lic_stuff Copying and Acknowledgements
 * @subsection pjmedia_about_subsec About PJMEDIA
 *
 * PJMEDIA is distributed under GPL terms (other licensing schemes may be 
 * arranged):
 \verbatim
    Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 \endverbatim
 *
 *
 * @section other_acks Acknowlegments
 * @subsection portaudio_subsec PortAudio
 *
 * PortAudio is supported as one of the sound device backend, and
 * is used by default on Linux/Unix and MacOS X target.
 * 
 * Please visit <A HREF="http://www.portaudio.com">http://www.portaudio.com</A>
 * for more info.
 *
 * PortAudio is distributed with the following condition.
 \verbatim
    Based on the Open Source API proposed by Ross Bencina
    Copyright (c) 1999-2000 Phil Burk

    Permission is hereby granted, free of charge, to any person obtaining
    a copy of this software and associated documentation files
    (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge,
    publish, distribute, sublicense, and/or sell copies of the Software,
    and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.
 \endverbatim
 *
 *
 * @subsection resample_ack Resample
 *
 * PJMEDIA uses <tt>resample-1.8.tar.gz</tt> from 
 * <A HREF="http://www-ccrma.stanford.edu/~jos/resample/">
 * Digital Audio Resampling Home Page</A>. This library is distibuted
 * on LGPL terms.
 *
 * Some excerpts from the original source codes:
 \verbatim
    HISTORY

    The first version of this software was written by Julius O. Smith III
    <jos@ccrma.stanford.edu> at CCRMA <http://www-ccrma.stanford.edu> in
    1981.  It was called SRCONV and was written in SAIL for PDP-10
    compatible machines.  The algorithm was first published in

    Smith, Julius O. and Phil Gossett. ``A Flexible Sampling-Rate
    Conversion Method,'' Proceedings (2): 19.4.1-19.4.4, IEEE Conference
    on Acoustics, Speech, and Signal Processing, San Diego, March 1984.

    An expanded tutorial based on this paper is available at the Digital
    Audio Resampling Home Page given above.

    Circa 1988, the SRCONV program was translated from SAIL to C by
    Christopher Lee Fraley working with Roger Dannenberg at CMU.

    Since then, the C version has been maintained by jos.

    Sndlib support was added 6/99 by John Gibson <jgg9c@virginia.edu>.

    The resample program is free software distributed in accordance
    with the Lesser GNU Public License (LGPL).  There is NO warranty; not
    even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 \endverbatim
 *
 * @subsection jb_ack Adaptive Jitter Buffer
 *
 * The PJMEDIA jitter buffer is based on implementation kindly donated
 * by <A HREF="http://www.switchlab.com">Switchlab, Ltd.</A>, and is
 * distributed under PJMEDIA licensing terms.
 *
 *
 * @subsection silence_det_ack Adaptive Silence Detector
 *
 * The adaptive silence detector was based on silence detector 
 * implementation in <A HREF="http://www.openh323.org">Open H323</A> 
 * project. I couldn't find the source code anymore, but generally
 * Open H323 files are distributed under MPL terms and has the
 * following excerpts:
 \verbatim
    Open H323 Library

    Copyright (c) 1998-2000 Equivalence Pty. Ltd.

    The contents of this file are subject to the Mozilla Public License
    Version 1.0 (the "License"); you may not use this file except in
    compliance with the License. You may obtain a copy of the License at
    http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS IS"
    basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
    the License for the specific language governing rights and limitations
    under the License.

    The Original Code is Open H323 Library.

    The Initial Developer of the Original Code is Equivalence Pty. Ltd.

    Portions of this code were written with the assisance of funding from
    Vovida Networks, Inc. http://www.vovida.com.
 \endverbatim

 * @subsection gsm_ack GSM Codec
 *
 * PJMEDIA uses GSM 
 * <A HREF="http://kbs.cs.tu-berlin.de/~jutta/toast.html">GSM 06.10</A>
 * version 1.0 at patchlevel 12. It has the following Copyright notice:
 *
 \verbatim
    Copyright 1992, 1993, 1994 by Jutta Degener and Carsten Bormann,
    Technische Universitaet Berlin

    Any use of this software is permitted provided that this notice is not
    removed and that neither the authors nor the Technische Universitaet Berlin
    are deemed to have made any representations as to the suitability of this
    software for any purpose nor are held responsible for any defects of
    this software.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.

    As a matter of courtesy, the authors request to be informed about uses
    this software has found, about bugs in this software, and about any
    improvements that may be of general interest.

    Berlin, 28.11.1994
    Jutta Degener
    Carsten Bormann
 \endverbatim
 *
 *
 * @subsection speex_codec_ack Speex Codec
 *
 * PJMEDIA uses Speex codec uses version 1.1.12 from <A HREF="http://www.speex.org">
 * www.speex.org</A>. The Speex library comes with the following Copying
 * notice:
 \verbatim
    Copyright 2002-2005 
	    Xiph.org Foundation
	    Jean-Marc Valin
	    David Rowe
	    EpicGames
	    Analog Devices

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    - Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

    - Neither the name of the Xiph.org Foundation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 \endverbatim
 *
 *
 * @subsection g711_codec_ack G.711 Codec
 *
 * The G.711 codec algorithm came from Sun Microsystems, Inc, and it's
 * got the following excerpts:
 *
 \verbatim
    This source code is a product of Sun Microsystems, Inc. and is provided
    for unrestricted use.  Users may copy or modify this source code without
    charge.

    SUN SOURCE CODE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING
    THE WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
    PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.

    Sun source code is provided with no support and without any obligation on
    the part of Sun Microsystems, Inc. to assist in its use, correction,
    modification or enhancement.

    SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
    INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS SOFTWARE
    OR ANY PART THEREOF.

    In no event will Sun Microsystems, Inc. be liable for any lost revenue
    or profits or other special, indirect and consequential damages, even if
    Sun has been advised of the possibility of such damages.

    Sun Microsystems, Inc.
    2550 Garcia Avenue
    Mountain View, California  94043
 \endverbatim
 *
 */


/**
 @page getting_started_pjmedia Getting Started with PJMEDIA
 
 @section getstart_init_setup_build Setting-up the Build System

 @subsection subsec_build_pjmedia Building PJMEDIA and PJMEDIA-CODEC

 The PJMEDIA and PJMEDIA-CODEC libraries are normally bundled in PJPROJECT
 source tarball, and they are located in <tt><b>pjmedia</b></tt> sub-directory
 tree.

 Please follow the instructions in <tt><b>INSTALL.txt</b></tt> in the root
 PJPROJECT directory to build all projects, including PJMEDIA and PJMEDIA-CODEC.

 @subsection subsec_config_build Setting Up the Build Environment

 In your project, you will need to configure the following.
 - Add <tt><b>$pjproject/pjmedia/include</b></tt> in the search path for
   include files.
 - Add <tt><b>$pjproject/pjmedia/lib</b></tt> in the search path for
   library files.
 - Add PJMEDIA and PJMEDIA static libraries in the link command.

 @subsection subsec_inc_pjmedia Include PJMEDIA and PJMEDIA-CODEC in Source Files

 To include all features from PJMEDIA and PJMEDIA-CODEC, use the following:

 \code
   #include <pjlib.h>
   #include <pjmedia.h>
   #include <pjmedia-codec.h>
 \endcode

 Alternatively, you may include only specific parts of the library (for example
 to speed up compilation by just a fraction), for example:

 \code
   #include <pjmedia/conference.h>
   #include <pjmedia/jbuf.h>
   #include <pjmedia-codec/speex.h>
 \endcode

  Note that you need to give <b>"pjmedia/"</b> and <b>"pjmedia-codec/"</b> 
  prefix to include specific files.


 @section getstart_using Using PJMEDIA

  I wish I could explain more, but for now, please have a look at the 
  @ref page_pjmedia_samples page on some examples.
 */

/**
  @page page_pjmedia_samples PJMEDIA and PJMEDIA-CODEC Examples

  @section pjmedia_samples_sec PJMEDIA and PJMEDIA-CODEC Examples

  Please find below some PJMEDIA related examples that may help in giving
  some more info:

  - @ref page_pjmedia_samples_level_c\n
    This is a good place to start learning about @ref PJMEDIA_PORT_CONCEPT,
    as it shows that @ref PJMEDIA_PORT_CONCEPT are only "passive" objects
    with <tt>get_frame()</tt> and <tt>put_frame()</tt> interface, and
    someone has to call these to retrieve/store media frames.

  - @ref page_pjmedia_samples_playfile_c\n
    This example shows that when application connects a media port (in this
    case a @ref PJMEDIA_FILE_PLAY) to @ref PJMED_SND_PORT, media will flow
    automatically since the @ref PJMED_SND_PORT provides @ref PJMEDIA_PORT_CLOCK.

  - @ref page_pjmedia_samples_recfile_c\n
    Demonstrates how to capture audio from microphone to WAV file.

  - @ref page_pjmedia_samples_playsine_c\n
    Demonstrates how to create a custom @ref PJMEDIA_PORT_CONCEPT (in this
    case a sine wave generator) and integrate it to PJMEDIA.

  - @ref page_pjmedia_samples_confsample_c\n
    This demonstrates how to use the @ref PJMEDIA_CONF. The sample program can 
    open multiple WAV files, and instruct the conference bridge to mix the
    signal before playing it to the sound device.

  - @ref page_pjmedia_samples_confbench_c\n
    I use this to benchmark/optimize the conference bridge algorithm, but
    readers may find the source useful.

  - @ref page_pjmedia_samples_resampleplay_c\n
    Demonstrates how to use @ref PJMEDIA_RESAMPLE_PORT to change the
    sampling rate of a media port (in this case, a @ref PJMEDIA_FILE_PLAY).

  - @ref page_pjmedia_samples_sndtest_c\n
    This program performs some tests to the sound device to get some
    quality parameters (such as sound jitter and clock drifts).\n
    Screenshots on WinXP: \image html sndtest.jpg "sndtest screenshot on WinXP"

  - @ref page_pjmedia_samples_streamutil_c\n
    This example mainly demonstrates how to stream media (in this case a
    @ref PJMEDIA_FILE_PLAY) to remote peer using RTP.

  - @ref page_pjmedia_samples_siprtp_c\n
    This is a useful program (integrated with PJSIP) to actively measure 
    the network quality/impairment parameters by making one or more SIP 
    calls (or receiving one or more SIP calls) and display the network
    impairment of each stream direction at the end of the call.
    The program is able to measure network quality parameters such as
    jitter, packet lost/reorder/duplicate, round trip time, etc.\n
    Note that the remote peer MUST support RTCP so that network quality
    of each direction can be calculated. Using siprtp for both endpoints
    is recommended.\n
    Screenshots on WinXP: \image html siprtp.jpg "siprtp screenshot on WinXP"

  - @ref page_pjmedia_samples_tonegen_c\n
    This is a simple program to generate a tone and write the samples to
    a raw PCM file. The main purpose of this file is to analyze the
    quality of the tones/sine wave generated by PJMEDIA tone/sine wave
    generator.

  - @ref page_pjmedia_samples_aectest_c\n
    Play a file to speaker, run AEC, and record the microphone input
    to see if echo is coming.
 */

/**
 * \page page_pjmedia_samples_siprtp_c Samples: Using SIP and Custom RTP/RTCP to Monitor Quality
 *
 * This source is an example to demonstrate using SIP and RTP/RTCP framework
 * to measure the network quality/impairment from the SIP call. This
 * program can be used to make calls or to receive calls from other
 * SIP endpoint (or other siprtp program), and to display the media
 * quality statistics at the end of the call.
 *
 * Note that the remote peer must support RTCP.
 *
 * The layout of the program has been designed so that custom reporting
 * can be generated instead of plain human readable text.
 *
 * The source code of the file is pjsip-apps/src/samples/siprtp.c
 *
 * Screenshots on WinXP: \image html siprtp.jpg
 *
 * \includelineno siprtp.c
 */

#endif /* __PJMEDIA_DOXYGEN_H__ */

