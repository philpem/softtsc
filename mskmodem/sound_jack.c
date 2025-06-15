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
#include <glib.h>
#include <jack/jack.h>

#include "sound.h"

struct MSKModemSoundContext_s {
  jack_client_t* client; 
  jack_port_t* outport;
  jack_port_t* inport;

  MSKModemSoundRxFn rx_f;
  MSKModemSoundTxFn tx_f;
  void* userdata;

  int isStarted;
};

int
process (jack_nframes_t nframes, void *arg)
{
  MSKModemSoundContext* ctx = arg;
  jack_default_audio_sample_t *out;
  jack_default_audio_sample_t *in;

  in = jack_port_get_buffer(ctx->inport, nframes);
  out = jack_port_get_buffer(ctx->outport, nframes);

  ctx->rx_f(in, nframes, ctx->userdata);
  ctx->tx_f(out, nframes, ctx->userdata);

  return 0;

}

int
mskmodem_sound_init (
  MSKModemSoundContext** ppCtx,
  const char* channelId,
  MSKModemSoundRxFn rx_f,
  MSKModemSoundTxFn tx_f,
  void* context
)
{
  jack_options_t options = JackNullOption;
  jack_status_t status;
  
  MSKModemSoundContext* ctx = g_new0(MSKModemSoundContext, 1);
  ctx->userdata = context;
  ctx->rx_f = rx_f;
  ctx->tx_f = tx_f;

  // Set up jack audio
  ctx->client = jack_client_open(channelId, options, &status, NULL);
  if (!ctx->client)
  {
    g_error("NO jack");
    return 1;
  }

  jack_set_process_callback(ctx->client, process, ctx);

  ctx->outport = jack_port_register (ctx->client, "Tx",
                                     JACK_DEFAULT_AUDIO_TYPE,
				     JackPortIsOutput, 0);

  ctx->inport = jack_port_register (ctx->client, "Rx",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsInput, 0);
  if (!ctx->outport || !ctx->inport)
  {
    g_error("No ports");
    return 1;
  }

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

    jack_client_close(ctx->client);

    g_free(ctx);
    *ppCtx = NULL;
  }
}

int
mskmodem_sound_run (
  MSKModemSoundContext* ctx
)
{

  const char **ports;
  
  // Are we running?
  if (ctx->isStarted)
    return 0;

  // Ready to start processing audio!
  if (jack_activate (ctx->client)) {
    g_error("cannot activate client");
    return 1;
  }

  // Connect outputs ports (JackPortIsInput - perspective of Jack)
  ports = jack_get_ports (ctx->client, NULL, NULL, 
                          JackPortIsPhysical|JackPortIsInput);
  if (ports == NULL) {
    g_error("no physical playback ports");
    return 1;
  }
  if (jack_connect (ctx->client, jack_port_name (ctx->outport), ports[0])) {
    g_error("cannot connect output ports");
    jack_free(ports);
    return 1;
  }
  jack_free(ports);

  // Connect input ports
  ports = jack_get_ports (ctx->client, NULL, NULL, 
                          JackPortIsPhysical|JackPortIsOutput);
  if (ports == NULL) {
    g_error("no physical input ports");
    return 1;
  }
  if (jack_connect (ctx->client, ports[0], jack_port_name (ctx->inport) )) {
    g_error("cannot connect input ports");
    jack_free(ports);
    return 1;
  }
  jack_free(ports);

  ctx->isStarted = 1;

  return 0;
}

int
mskmodem_sound_stop (
  MSKModemSoundContext* ctx
)
{
  // Are we running?
  if (!ctx->isStarted)
    return 0;

  // Ready to start processing audio!
  if (jack_deactivate (ctx->client)) {
    g_error("cannot deactivate client");
    return 1;
  }
  
  ctx->isStarted = 0;

  return 0;
}


