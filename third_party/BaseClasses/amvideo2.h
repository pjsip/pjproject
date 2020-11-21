/*
  Some necessary macros are missing from mingw version. They are here.
  See https://github.com/ofTheo/videoInput/blob/master/videoInputSrcAndDemos/libs/DShow/Include/amvideo.h
*/
//------------------------------------------------------------------------------
// File: AMVideo.h
//
// Desc: Video related definitions and interfaces for ActiveMovie.
//
// Copyright (c) 1992 - 2001, Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#define TRUECOLOR(pbmi)  ((TRUECOLORINFO *)(((LPBYTE)&((pbmi)->bmiHeader)) \
					+ (pbmi)->bmiHeader.biSize))
#define COLORS(pbmi)	((RGBQUAD *)(((LPBYTE)&((pbmi)->bmiHeader)) 	\
					+ (pbmi)->bmiHeader.biSize))

#define SIZE_MASKS (iMASK_COLORS * sizeof(DWORD))
#define SIZE_PREHEADER (FIELD_OFFSET(VIDEOINFOHEADER,bmiHeader))

#define WIDTHBYTES(bits) ((DWORD)(((bits)+31) & (~31)) / 8)
#define DIBWIDTHBYTES(bi) (DWORD)WIDTHBYTES((DWORD)(bi).biWidth * (DWORD)(bi).biBitCount)
#define _DIBSIZE(bi) (DIBWIDTHBYTES(bi) * (DWORD)(bi).biHeight)
#define DIBSIZE(bi) ((bi).biHeight < 0 ? (-1)*(_DIBSIZE(bi)) : _DIBSIZE(bi))

#define PALETTISED(pbmi) ((pbmi)->bmiHeader.biBitCount <= iPALETTE)
