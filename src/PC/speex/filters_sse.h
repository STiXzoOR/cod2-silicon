/* Copyright (C) 2002 Jean-Marc Valin
   File: filters.c
   Various analysis/synthesis filters

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

/*
 * The original Speex SSE path used GCC inline assembly. Keep the header usable
 * when _USE_SSE is enabled, but implement the same recurrence in plain C.
 */
void filter_mem2(float *x, float *num, float *den, float *y, int N, int ord, float *mem)
{
   int i, j;

   for (i = 0; i < N; i++)
   {
      float xi = x[i];
      float yi;

      y[i] = num[0] * xi + mem[0];
      yi = y[i];

      for (j = 0; j < ord - 1; j++)
      {
         mem[j] = mem[j + 1] + num[j + 1] * xi - den[j + 1] * yi;
      }
      mem[ord - 1] = num[ord] * xi - den[ord] * yi;
   }
}

void iir_mem2(float *x, float *den, float *y, int N, int ord, float *mem)
{
   int i, j;

   for (i = 0; i < N; i++)
   {
      y[i] = x[i] + mem[0];

      for (j = 0; j < ord - 1; j++)
      {
         mem[j] = mem[j + 1] - den[j + 1] * y[i];
      }
      mem[ord - 1] = -den[ord] * y[i];
   }
}
