/* Copyright (C) 2004 Jean-Marc Valin
   File medfilter.c
   Median filter


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

#include "medfilter.h"
#include "misc.h"

MedianFilter *median_filter_new(int N)
{
   MedianFilter *f = (MedianFilter*)speex_alloc(sizeof(MedianFilter));
   f->N = N;
   f->ids = (int*)speex_alloc(sizeof(int)*N);
   f->val = (float*)speex_alloc(sizeof(float)*N);
   f->filled = 0;
   return f;
}

void median_filter_update(MedianFilter *f, float val)
{
   int i=0;
   int insert = 0;
   while (insert<f->filled && f->val[insert] < val)
   {
      insert++;
   }
   if (f->filled == f->N)
   {
      int remove;
      for (remove=0;remove<f->N;remove++)
         if (f->ids[remove] == 0)
            break;
      if (insert>remove)
         insert--;
      if (insert > remove)
      {
         for (i=remove;i<insert;i++)
         {
            f->val[i] = f->val[i+1];
            f->ids[i] = f->ids[i+1];
         }
      } else if (insert < remove)
      {
         for (i=remove;i>insert;i--)
         {
            f->val[i] = f->val[i-1];
            f->ids[i] = f->ids[i-1];
         }
      }
      for (i=0;i<f->filled;i++)
         f->ids[i]--;
   } else {
      for (i=f->filled;i>insert;i--)
      {
         f->val[i] = f->val[i-1];
         f->ids[i] = f->ids[i-1];
      }
      f->filled++;
   }
   f->val[insert]=val;
   f->ids[insert]=f->filled-1;
}

float median_filter_get(MedianFilter *f)
{
   return f->val[f->filled>>1];
}

