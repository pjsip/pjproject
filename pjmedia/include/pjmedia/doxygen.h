/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
 * Please click the <A HREF="modules.htm"><b>Table of Contents</b></A> link on top
 * of this page to get the complete features currently present in PJMEDIA.
 *
 * Also please read the documentation about @ref PJMEDIA_PORT
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
  @page page_pjmedia_samples PJMEDIA and PJMEDIA-CODEC Examples

  @section pjmedia_samples_sec PJMEDIA and PJMEDIA-CODEC Examples

  Please find below some PJMEDIA related examples that may help in giving
  some more info:

  - @ref page_pjmedia_samples_level_c\n
    This is a good place to start learning about @ref PJMEDIA_PORT,
    as it shows that @ref PJMEDIA_PORT are only "passive" objects
    with <tt>get_frame()</tt> and <tt>put_frame()</tt> interface, and
    someone has to call these to retrieve/store media frames.

  - @ref page_pjmedia_samples_playfile_c\n
    This example shows that when application connects a media port (in this
    case a @ref PJMEDIA_FILE_PLAY) to @ref PJMED_SND_PORT, media will flow
    automatically since the @ref PJMED_SND_PORT provides @ref PJMEDIA_PORT_CLOCK.

  - @ref page_pjmedia_samples_recfile_c\n
    Demonstrates how to capture audio from microphone to WAV file.

  - @ref page_pjmedia_samples_playsine_c\n
    Demonstrates how to create a custom @ref PJMEDIA_PORT (in this
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

