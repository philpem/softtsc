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

#ifndef CHANNEL_H
#define CHANNEL_H 

#include <mskmodem.h>

typedef void (*mpt1327_channel_recv_fn)(void* userdata, guint64 cw);
typedef guint64 (*mpt1327_channel_txcv_fn)(void* userdata);
typedef guint64 (*mpt1327_channel_completion_fn)(void* userdata);

typedef struct MPT1327Tone_s
{
  gint16 freq;
  gint32 duration;
  mpt1327_channel_completion_fn fcomp;
  void* userdata;
} MPT1327Tone;

typedef struct MPT1327Channel_s
{
  // Modem thread
  MSKModemContext* modem;
  
  // Codeword transmission
  mpt1327_channel_txcv_fn tx_callback;

  // Codeword reception
  guint64 rx_cw;
  mpt1327_channel_recv_fn rx_callback;

  // Sound bridge
  gboolean enable_bridge;
  float* cbsnd;
  int cbsnd_size;   // Buffer size
  int cbsnd_ready;  // Ready count
  int cbsnd_wr;     // Write index
  int cbsnd_rd;     // Read index

  // Tone synthesiser
  MPT1327Tone* cbtone;
  int cbtone_size;   // Buffer size
  int cbtone_ready;  // Ready count
  int cbtone_wr;     // Write index
  int cbtone_rd;     // Read index

  // Misc
  void* userdata;
  GMutex mutex;

} MPT1327Channel;

int mpt1327_channel_init(MPT1327Channel** ppCh, const char* channelId,
                         mpt1327_channel_recv_fn recvfn,
                         mpt1327_channel_txcv_fn txcvfn,
                         void* context);
void mpt1327_channel_free(MPT1327Channel** ppCh);
int mpt1327_channel_stop(MPT1327Channel* ch);
int mpt1327_channel_start(MPT1327Channel* ch);
guint16 mpt1327_channel_fcs(guint64 cw);
guint64 mpt1327_channel_fcs_add(guint64 cw);
void mpt1327_channel_queue_tone(
    MPT1327Channel* ch,
    gint16 freq,
    gint32 duration,
    mpt1327_channel_completion_fn fcomp,
    void* userdata
);
void mpt1327_channel_queue_morse(
    MPT1327Channel* ch,
    const char* morse,
    mpt1327_channel_completion_fn fcomp,
    void* userdata
);
void mpt1327_channel_bridge(
    MPT1327Channel* ch,
    int bridge
);

#endif /* CHANNEL_H */

