/* Copyright (C) 2002-2005 Jean-Marc Valin 
   File: misc.c
   Various utility routines for Speex

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
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "misc.h"

#ifdef USER_MISC
#include "user_misc.h"
#endif

#ifdef BFIN_ASM
#include "misc_bfin.h"
#endif

#ifndef RELEASE
void print_vec(float *vec, int len, char *name)
{
   int i;
   printf ("%s ", name);
   for (i=0;i<len;i++)
      printf (" %f", vec[i]);
   printf ("\n");
}
#endif

#ifdef FIXED_DEBUG
long long spx_mips=0;
#endif


spx_uint32_t be_int(spx_uint32_t i)
{
   spx_uint32_t ret=i;
#ifndef WORDS_BIGENDIAN
   ret =  i>>24;
   ret += (i>>8)&0x0000ff00;
   ret += (i<<8)&0x00ff0000;
   ret += (i<<24);
#endif
   return ret;
}

spx_uint32_t le_int(spx_uint32_t i)
{
   spx_uint32_t ret=i;
#ifdef WORDS_BIGENDIAN
   ret =  i>>24;
   ret += (i>>8)&0x0000ff00;
   ret += (i<<8)&0x00ff0000;
   ret += (i<<24);
#endif
   return ret;
}

#ifndef OVERRIDE_SPEEX_ALLOC
void *speex_alloc (int size)
{
   return calloc(size,1);
}
#endif

#ifndef OVERRIDE_SPEEX_ALLOC_SCRATCH
void *speex_alloc_scratch (int size)
{
   return calloc(size,1);
}
#endif

#ifndef OVERRIDE_SPEEX_REALLOC
void *speex_realloc (void *ptr, int size)
{
   return realloc(ptr, size);
}
#endif

#ifndef OVERRIDE_SPEEX_FREE
void speex_free (void *ptr)
{
   free(ptr);
}
#endif

#ifndef OVERRIDE_SPEEX_FREE_SCRATCH
void speex_free_scratch (void *ptr)
{
   free(ptr);
}
#endif

#ifndef OVERRIDE_SPEEX_MOVE
void *speex_move (void *dest, void *src, int n)
{
   return memmove(dest,src,n);
}
#endif

#ifndef OVERRIDE_SPEEX_ERROR
void speex_error(const char *str)
{
   fprintf (stderr, "Fatal (internal) error: %s\n", str);
   exit(1);
}
#endif

#ifndef OVERRIDE_SPEEX_WARNING
void speex_warning(const char *str)
{
#ifndef DISABLE_WARNINGS
   fprintf (stderr, "warning: %s\n", str);
#endif
}
#endif

#ifndef OVERRIDE_SPEEX_WARNING_INT
void speex_warning_int(const char *str, int val)
{
#ifndef DISABLE_WARNINGS
   fprintf (stderr, "warning: %s %d\n", str, val);
#endif
}
#endif

#ifndef OVERRIDE_SPEEX_NOTIFY
void speex_notify(const char *str)
{
#ifndef DISABLE_NOTIFICATIONS
   fprintf (stderr, "notification: %s\n", str);
#endif
}
#endif

#ifdef FIXED_POINT
spx_word16_t speex_rand(spx_word16_t std, spx_int32_t *seed)
{
   spx_word32_t res;
   *seed = 1664525 * *seed + 1013904223;
   res = MULT16_16(EXTRACT16(SHR32(*seed,16)),std);
   return EXTRACT16(PSHR32(SUB32(res, SHR32(res, 3)),14));
}
#else
spx_word16_t speex_rand(spx_word16_t std, spx_int32_t *seed)
{
   const unsigned int jflone = 0x3f800000;
   const unsigned int jflmsk = 0x007fffff;
   union {int i; float f;} ran;
   *seed = 1664525 * *seed + 1013904223;
   ran.i = jflone | (jflmsk & *seed);
   ran.f -= 1.5;
   return 3.4642*std*ran.f;
}
#endif

#ifndef OVERRIDE_SPEEX_PUTC
void _speex_putc(int ch, void *file)
{
   FILE *f = (FILE *)file;
   fprintf(f, "%c", ch);
}
#endif
