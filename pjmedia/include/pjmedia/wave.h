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
#ifndef __PJMEDIA_WAVE_H__
#define __PJMEDIA_WAVE_H__


/**
 * @file wave.h
 * @brief WAVE file manipulation.
 */
#include <pjmedia/types.h>


PJ_BEGIN_DECL

#if PJ_IS_BIG_ENDIAN
#   define PJMEDIA_RIFF_TAG	('R'<<24|'I'<<16|'F'<<8|'F')
#   define PJMEDIA_WAVE_TAG	('W'<<24|'A'<<16|'V'<<8|'E')
#   define PJMEDIA_FMT_TAG	('f'<<24|'m'<<16|'t'<<8|' ')
#else
#   define PJMEDIA_RIFF_TAG	('F'<<24|'F'<<16|'I'<<8|'R')
#   define PJMEDIA_WAVE_TAG	('E'<<24|'V'<<16|'A'<<8|'W')
#   define PJMEDIA_FMT_TAG	(' '<<24|'t'<<16|'m'<<8|'f')
#endif


/**
 * This file describes the simpler/canonical version of a WAVE file.
 * It does not support the full RIFF format specification.
 */
struct pjmedia_wave_hdr
{
    struct {
	pj_uint32_t riff;
	pj_uint32_t file_len;
	pj_uint32_t wave;
    } riff_hdr;

    struct {
	pj_uint32_t fmt;
	pj_uint32_t len;
	pj_uint16_t fmt_tag;
	pj_uint16_t nchan;
	pj_uint32_t sample_rate;
	pj_uint32_t bytes_per_sec;
	pj_uint16_t block_align;
	pj_uint16_t bits_per_sample;
    } fmt_hdr;

    struct {
	pj_uint32_t data;
	pj_uint32_t len;
    } data_hdr;
};

/**
 * @see pjmedia_wave_hdr
 */
typedef struct pjmedia_wave_hdr pjmedia_wave_hdr;


PJ_END_DECL

#endif	/* __PJMEDIA_WAVE_H__ */
