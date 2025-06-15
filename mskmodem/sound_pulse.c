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

#include <string.h>
#include <pulse/pulseaudio.h>
#include <glib.h>

#include "sound.h"

struct MSKModemSoundContext_s {
  pa_mainloop* paloop;
  pa_context* pactx;
  pa_stream* pastream_in;
  pa_stream* pastream_out;
  MSKModemSoundRxFn rx_f;
  MSKModemSoundTxFn tx_f;
  void* userdata;
};

static void sound_rx(pa_stream* p, size_t nbytes, void* context)
{
  MSKModemSoundContext* ctx = context;
  const void* buf;

  pa_stream_peek(p, &buf, &nbytes);
  ctx->rx_f( (mskmodem_sound_t*)buf,
             nbytes/sizeof(mskmodem_sound_t), 
             ctx->userdata );
  pa_stream_drop(p);
}

static void sound_tx(pa_stream* p, size_t nbytes, void* context)
{
  MSKModemSoundContext* ctx = context;
  void* buf;

  nbytes = 9600; // About 100ms
  pa_stream_begin_write(p, &buf, &nbytes);
  ctx->tx_f( (mskmodem_sound_t*)buf,
             nbytes/sizeof(mskmodem_sound_t),
             ctx->userdata );
  pa_stream_write(p, buf, nbytes, NULL, 0, PA_SEEK_RELATIVE);
}

static void context_state_callback(pa_context* c, void* userdata)
{
  int i;
  MSKModemSoundContext* ctx = userdata;
  const pa_sample_spec ss = {
    .format = PA_SAMPLE_FLOAT32LE,
    .rate = 48000,
    .channels = 1
  };
  const pa_buffer_attr sbp = {
    .maxlength = -1,
    .tlength = 2568, // 32-bits. (This is used for timing. Our timing slot is
                     // based on 64-bits (1/2 slot) so we need 1/4 slot=32bits)
    .prebuf = -1,
    .minreq = -1,
    .fragsize = 1284 // 16-bits -  low latency rx
  };

  switch (pa_context_get_state(c)) {

  case PA_CONTEXT_READY:

    // Connect input stream
    ctx->pastream_in = pa_stream_new(ctx->pactx, "Rx", &ss, NULL);
    g_assert(ctx->pastream_in);
    pa_stream_set_read_callback(ctx->pastream_in, sound_rx, ctx);
    pa_stream_connect_record(ctx->pastream_in, NULL, &sbp,
                             PA_STREAM_ADJUST_LATENCY);

    // Connect output stream
    ctx->pastream_out = pa_stream_new(ctx->pactx, "Tx", &ss, NULL);
    g_assert(ctx->pastream_out);
    pa_stream_set_write_callback(ctx->pastream_out, sound_tx, ctx);
    pa_stream_connect_playback(ctx->pastream_out, NULL,
                               &sbp, PA_STREAM_ADJUST_LATENCY, NULL, NULL);


    break;

  case PA_CONTEXT_FAILED:
    g_message("Connection to PulseAudio failed");
    break;

  default:
    break;
  }
}

int
mskmodem_sound_init (
  MSKModemSoundContext** ppCtx,
  MSKModemSoundRxFn rx_f,
  MSKModemSoundTxFn tx_f,
  void* context
)
{
  MSKModemSoundContext* ctx = g_new0(MSKModemSoundContext, 1);
  ctx->userdata = context;
  ctx->rx_f = rx_f;
  ctx->tx_f = tx_f;

  // Set up Pulse audio
  ctx->paloop = pa_mainloop_new();
  g_assert(ctx->paloop);
  ctx->pactx = pa_context_new(pa_mainloop_get_api(ctx->paloop), 
                              "MSK Modem");
  g_assert(ctx->pactx);
  pa_context_set_state_callback(ctx->pactx, context_state_callback, ctx);
  pa_context_connect(ctx->pactx, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);

  *ppCtx = ctx;

  return 0;
}

void
mskmodem_sound_free (
  MSKModemSoundContext** ppCtx
)
{
  if (ppCtx && *ppCtx)
  {
    MSKModemSoundContext* ctx = *ppCtx;

    if (ctx->pastream_in)
      pa_stream_unref(ctx->pastream_in);
    if (ctx->pastream_out)
      pa_stream_unref(ctx->pastream_out);

    if (ctx->pactx)
      pa_context_unref(ctx->pactx);
    if (ctx->paloop)
      pa_mainloop_free(ctx->paloop);

    g_free(ctx);
    *ppCtx = NULL;
  }
}

int
mskmodem_sound_run (
  MSKModemSoundContext* ctx
)
{
  int retval;
  pa_mainloop_run(ctx->paloop, &retval);
  return retval;
}

int
mskmodem_sound_stop (
  MSKModemSoundContext* ctx
)
{
  pa_mainloop_quit(ctx->paloop, 0);
  return 0;
}


