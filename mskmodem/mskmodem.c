/* SoftTSC - Software MPT1327 Trunking System Controller
* Copyright (C) 2013-2014 Paul Banks (http://paulbanks.org)
* 
* This file is part of SoftTSC
*
* SoftTSC is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* SoftTSC is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with SoftTSC.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <math.h>

#include "mskmodem.h"

static const float fir900to2100[] = {
  0.0003829,0.0000483,-0.0003554,-0.0009058,
  -0.0016643,-0.0026639,-0.0038995,-0.0053223,-0.0068404,-0.0083243,-0.0096175,
  -0.0105523,-0.0109671,-0.0107252,-0.0097326,-0.0079517,-0.0054107,-0.0022065,
  0.0014986,0.0054883,0.0095081,0.0132861,0.0165563,0.0190822,0.0206773,
  0.0212227,0.0206773,0.0190822,0.0165563,0.0132861,0.0095081,0.0054883,
  0.0014986,-0.0022065,-0.0054107,-0.0079517,-0.0097326,-0.0107252,-0.0109671,
  -0.0105523,-0.0096175,-0.0083243,-0.0068404,-0.0053223,-0.0038995,-0.0026639,
  -0.0016643,-0.0009058,-0.0003554,0.0000483,0.0003829
};

static const float fir600[] = {
  0.0015393,0.0017254,0.0020791,0.0026251,
  0.0033837,0.0043697,0.0055914,0.0070505,0.0087412,0.0106507,0.0127585,
  0.0150375,0.017454,0.0199689,0.022538,0.0251141,0.0276471,0.0300864,
  0.0323815,0.0344841,0.0363486,0.0379345,0.0392064,0.040136,0.0407023,
  0.0408925,0.0407023,0.040136,0.0392064,0.0379345,0.0363486,0.0344841,
  0.0323815,0.0300864,0.0276471,0.0251141,0.022538,0.0199689,0.017454,
  0.0150375,0.0127585,0.0106507,0.0087412,0.0070505,0.0055914,0.0043697,
  0.0033837,0.0026251,0.0020791,0.0017254,0.0015393
};

static float convolvesum(const float* sig, const float* coeff,
                         int len, int sigstart)
{

  int n, p = sigstart;
  float s = 0;
  for (n=0; n<len; n++) {
    s += sig[p] * coeff[n];
    p = (p==0) ? len-1 : p-1;
  }

  return s;

}

struct MSKModemContext_s {

  MSKModemSoundContext* sctx;

  // Modulator variables
  guint64 current;
  guint64 bitmask;
  int phase;
  float padj;
  float fs;

  // Coh Demodulator variables
  int curr_sample;
  float* corr_i0, *corr_q0, *corr_i1, *corr_q1;
  float sum_i0, sum_q0, sum_i1, sum_q1;
  int corr;
  int pll;
  int nuf_ones;

  // Incoh Demodulator variables
  int filterpos;
  float* initfilter;
  float last;
  int discpos;
  int* discqueue;
  float* discfilter;
  int mst;
  int slast;
  int pll_count;

  // Callbacks to user code
  MSKModemRxFn rx_f; // Modem rx
  MSKModemTxFn tx_f; // Modem tx
  MSKModemSoundRxFn rx_sound_f; // Sound rx
  MSKModemSoundTxFn tx_sound_f; // Sound rx
  void* userdata; // Context data for callbacks

};

static void modem_tx(mskmodem_sound_t* buf, int samples, void* userdata)
{
  MSKModemContext* u = userdata;
  int i;

  u->tx_sound_f(buf, samples, u->userdata);

  for (i=0; i<samples; i++) {

    if (++u->phase > 40) {

      u->phase = 1;

      // If first bit get new codeword
      if (u->bitmask==0) {
        u->bitmask = 0x8000000000000000LL;
        u->current = 0;
        u->tx_f(&u->current, u->userdata);
      }

      // Adjust phase for continuity
      u->padj = u->padj + u->fs;
      u->padj -= floor(u->padj);

      // Get bit
      if (u->current & u->bitmask)
        u->fs = 1.25f - 0.25f;
      else
        u->fs = 1.25f + 0.25f;

      // Set next bit
      u->bitmask >>= 1;

    }

    if (u->current != 0) {
      buf[i] = sin(2.0*G_PI*(u->fs * (u->phase/40.0) + u->padj));
    }

  }

}

static void modem_rx(const mskmodem_sound_t* s, int samples, void* userdata)
{
  MSKModemContext* u = userdata;
  int i=0, b=0, t=0, snrz=0;
  float v = 0.0f;

  int pll_early=0, pll_late=0, pll_reset=0;

  u->rx_sound_f(s, samples, u->userdata);

  for (i=0; i<samples-1; i++) {

    // Initial filter
    u->initfilter[u->filterpos] = s[i];
    v = convolvesum(u->initfilter, fir900to2100, 50, u->filterpos);

    // Zero crossing detector
    if ( (u->last < 0 && v >=0) || (u->last >=0 && v < 0) )
      u->mst = 40/3;
    u->last = v;

    // Monostable
    b = 0;
    if (u->mst > 0) {
      u->mst -= 1;
      b = 1;
    }

    // Discriminator
    u->discqueue[u->discpos] = b;
    if ((t = u->discpos - (40/3)) < 0)
      t += 15;
    b &= u->discqueue[t];
    if ((t = u->discpos - (40/6)) < 0)
      t += 15;
    b &= u->discqueue[t];
    b = 1 - b;
    if (++u->discpos >= 15)
      u->discpos = 0;

    // Low pass output of discriminator
    u->discfilter[u->filterpos] = b;
    v = convolvesum(u->discfilter, fir600, 50, u->filterpos);

    // Bit detector
    if (v>0.5f)
      b = 1;
    else
      b = 0;

    // PLL sync
    snrz = 0;
    if (b != u->slast) {
      u->slast = b;
      snrz = 1;
    }

    // PLL early/late gate
    pll_reset = 0;
    if (u->pll_count < 40/2-1 && snrz)
      pll_early = 1;
    else if (u->pll_count > 40/2+1 && snrz)
      pll_late = 1;

    // PLL reference adjust
    if (u->pll_count == 40-1-2 && pll_early && !pll_late)
      pll_reset = 1;
    if (u->pll_count == 40-1 && !pll_early && !pll_late)
      pll_reset = 1;
    if (u->pll_count == 40-1 && pll_early && pll_late)
      pll_reset = 1;
    if (u->pll_count == 40+1+2)
      pll_reset = 1;

    // PLL reference generator
    if (u->pll_count > 40/2)
      u->pll = 0;
    else {
      if (u->pll==0)
        u->rx_f(b, u->userdata);
      u->pll = 1;
    }

    // PLL reference adjust
    if (pll_reset) {
      u->pll_count = 0;
      pll_early = 0;
      pll_late = 0;
    } else
      u->pll_count += 1;

    // Update filterpos for next samples
    if (++u->filterpos >= 50) u->filterpos = 0;

  }

}

int
mskmodem_init
(
  MSKModemContext** ppCtx,
  const char* channelId,
  MSKModemRxFn rx_f,
  MSKModemTxFn tx_f,
  MSKModemSoundRxFn rx_sound_f,
  MSKModemSoundTxFn tx_sound_f,
  void* userdata
)
{

  MSKModemContext* ctx = g_new0(MSKModemContext, 1);
  *ppCtx = ctx;

  ctx->tx_f = tx_f;
  ctx->rx_f = rx_f;
  ctx->tx_sound_f = tx_sound_f;
  ctx->rx_sound_f = rx_sound_f;
  ctx->userdata = userdata;

  ctx->corr_i0 = g_new0(float, 40);
  ctx->corr_q0 = g_new0(float, 40);
  ctx->corr_i1 = g_new0(float, 40);
  ctx->corr_q1 = g_new0(float, 40);
  ctx->pll = 40;

  ctx->initfilter = g_new0(float, 50);
  ctx->discqueue  = g_new0(int, 50);
  ctx->discfilter = g_new0(float, 50);

  mskmodem_sound_init(&ctx->sctx, channelId, modem_rx, modem_tx, ctx); 

  return 0;
}

void
mskmodem_free
(
  MSKModemContext** ppCtx
)
{
  if (ppCtx && *ppCtx)
  {
    MSKModemContext* ctx = *ppCtx;
    mskmodem_sound_free(&ctx->sctx);
    g_free(ctx->corr_i0);
    g_free(ctx->corr_q0);
    g_free(ctx->corr_i1);
    g_free(ctx->corr_q1);
    
    g_free(ctx->initfilter);
    g_free(ctx->discqueue);
    g_free(ctx->discfilter);

    g_free(ctx);
    *ppCtx = NULL;
  }
}

int
mskmodem_run
(
  MSKModemContext* ctx
)
{
  mskmodem_sound_run(ctx->sctx);
}

int
mskmodem_stop
(
  MSKModemContext* ctx
)
{
  mskmodem_sound_stop(ctx->sctx);
}

