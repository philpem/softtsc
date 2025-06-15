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

#include <glib.h>
#include <string.h>
#include <math.h>

#include "channel.h"

#define SYNT 0x3B28     // 0011101100101000

const static char* const morsetable[] = {
  "A.-", "B-...", "C-.-.", "D-..", "E.", "F..-.", "G--.", "H....", "I..",
  "J.---", "K-.-", "L.-..", "M--", "N-.", "O---", "P.--.", "Q--.-", "R.-.",
  "S...", "T-", "U..-", "V...-", "W.--", "X-..-", "Y-.--", "Z--..",
  "0-----", "1.----", "2..---", "3...--", "4....-", "5.....", "6-....",
  "7--...", "8---..", "9----.", NULL
};

static void modem_tx(guint64* cw, void* userdata)
{
  MPT1327Channel* ch = userdata;
  guint64 cwtmp = ch->tx_callback(ch->userdata);
  if (cwtmp==1)
    *cw = 0xAAAAAAAAAAAA0000LL | SYNT; // Traffic channel sync word
  else if (cwtmp>1)
    *cw = mpt1327_channel_fcs_add(cwtmp);
}

static void modem_rx(guint32 bit, void* userdata)
{
  MPT1327Channel* ch = userdata;
  static int x;

  // Store new bit
  ch->rx_cw <<= 1;
  ch->rx_cw |= bit;

  // TODO: improve this. We need carrier detect for starters.
  //       probably nice to know which SYNC/SYNT was used too
  if (mpt1327_channel_fcs(ch->rx_cw>>16)==(ch->rx_cw&0xFFFF))
  {
    // Strip fcs from received data
    ch->rx_callback(ch->userdata, ch->rx_cw>>16);
  }

}

static void sound_rx(const mskmodem_sound_t* buf, 
                     gint32 samples, void* userdata)
{
  MPT1327Channel* ch = userdata; 

  if (ch->enable_bridge)
  {
    int cbremain = ch->cbsnd_size - ch->cbsnd_wr;
    if (cbremain >= samples)
      memcpy(ch->cbsnd+ch->cbsnd_wr, buf, samples*sizeof(*buf));
    else {
      memcpy(ch->cbsnd+ch->cbsnd_wr, buf, cbremain*sizeof(*buf));
      memcpy(ch->cbsnd, buf, (samples-cbremain)*sizeof(*buf));
    }
    ch->cbsnd_wr = (ch->cbsnd_wr + samples) % ch->cbsnd_size;
    ch->cbsnd_ready += samples;
  }

}

static void sound_tx(mskmodem_sound_t* buf, gint32 samples, void* userdata)
{
  MPT1327Channel* ch = userdata; 
  int i, bufavail;
  int p = 0;

  // Sound buffer (rx->tx)
  if (ch->cbsnd_ready >= samples || (ch->cbsnd_ready && ! ch->enable_bridge))
  {
    int cbremain = ch->cbsnd_size - ch->cbsnd_rd;
    p = (ch->cbsnd_ready >= samples) ? samples : ch->cbsnd_ready;

    if (cbremain >= p)
      memcpy(buf, ch->cbsnd+ch->cbsnd_rd, p*sizeof(*buf));
    else {
      memcpy(buf, ch->cbsnd+ch->cbsnd_rd, cbremain*sizeof(*buf));
      memcpy(buf+cbremain, ch->cbsnd, (p-cbremain)*sizeof(*buf));
    }

    ch->cbsnd_rd = (ch->cbsnd_rd + p) % ch->cbsnd_size;
    ch->cbsnd_ready -= p;
            
  }

  // Silence any unprocessed buffer
  while (p<samples) {
    buf[p] = 0;
    p++;
  }

  g_mutex_lock(&ch->mutex); //TODO: don't lock so long

  // Mix in tones
  if (ch->cbtone_ready) {
    MPT1327Tone* t = &ch->cbtone[ch->cbtone_rd];
    if (t->fcomp) {
      t->fcomp(t->userdata);
      t->fcomp = NULL;
    }
    for (i=0; i<samples && t->duration>0; i++) {
      mskmodem_sound_t v = buf[i] + 0.6 * MSKMODEM_SOUND_FULLSCALE *  
                           sin(2.0*G_PI*t->duration*t->freq/48000.0);
      buf[i] = tanhf(v);
      t->duration -= 1;
      if (t->duration==0) {
        ch->cbtone_rd = (ch->cbtone_rd + 1) % ch->cbtone_size;
        ch->cbtone_ready -= 1;
        t = &ch->cbtone[ch->cbtone_rd];
      }
    }
  }

  g_mutex_unlock(&ch->mutex);

}

void mpt1327_channel_queue_tone(
  MPT1327Channel* ch,
  gint16 freq, 
  gint32 duration,
  mpt1327_channel_completion_fn fcomp,
  void* userdata
)
{
  g_mutex_lock(&ch->mutex);

  // If full just bomb TODO: improve this, it will leak completions!
  if (ch->cbtone_ready >= ch->cbtone_size)
    return;

  // Put tone into buffer
  ch->cbtone[ch->cbtone_wr].freq = freq;
  ch->cbtone[ch->cbtone_wr].duration = duration;
  ch->cbtone[ch->cbtone_wr].fcomp = fcomp;
  ch->cbtone[ch->cbtone_wr].userdata = userdata;
  ch->cbtone_wr = (ch->cbtone_wr + 1) % ch->cbtone_size;
  ch->cbtone_ready += 1;

  g_mutex_unlock(&ch->mutex);

}  

void mpt1327_channel_queue_morse(
  MPT1327Channel* ch, 
  const char* str,
  mpt1327_channel_completion_fn fcomp,
  void* userdata
)
{

  const int c = 3200;

  const char* s = str;
  while (*s) {

    // Look up in table
    const char* const* p = morsetable;
    while (*p) {
      if (**p==*s) {
        const char* r = *p+1;
        while (*r) {
          if (*r=='.')
            mpt1327_channel_queue_tone(ch, 800, 1 * c, NULL, NULL);
          else
            mpt1327_channel_queue_tone(ch, 800, 3 * c, NULL, NULL);
          r++;
          // Signaling space (ITU-R M.1667-1 2009 2.2)
          mpt1327_channel_queue_tone(ch, 0, 1 * c, NULL, NULL);
        }
        break;
      }
      p++;
    }

    // Letter space is 3 dots (including signal space above)
    mpt1327_channel_queue_tone(ch, 0, 2 * c, NULL, NULL);

    // Word space is 7 dots (including letter space and signal space above)
    if (*s==' ')
      mpt1327_channel_queue_tone(ch, 0, 4 * c, NULL, NULL);

    s++;
  }
  
  // Queue completion callback (TODO: improve readability)
  if (fcomp)
    mpt1327_channel_queue_tone(ch, 0, 4 * c, fcomp, userdata);

}

void mpt1327_channel_bridge(
  MPT1327Channel* ch,
  int bridge       
)
{
  ch->enable_bridge = bridge;
}

int
mpt1327_channel_stop(
  MPT1327Channel* ch
)
{
  return mskmodem_stop(ch->modem);
}

int mpt1327_channel_start(MPT1327Channel* ch)
{
  return mskmodem_run(ch->modem);
}

guint16 mpt1327_channel_fcs(guint64 cw) {

  guint64 ck = 0;
  int parity = 0, n, b;

  // Calculate checksum
  for (n=0;n<48;n++) {
    b = cw >> (47-n) & 1;
    parity ^= b;
    if ((b ^ (ck >> 15)) & 1) 
      ck ^= 0x6815;
    ck <<= 1;
  }
  ck = (ck ^ 0x0002) & 0xFFFE;

  // Calculate parity
  for (n=0;n<16;n++)
    parity ^= ck >> (15-n) & 1;

  return ck | parity;
}

guint64 mpt1327_channel_fcs_add(guint64 cw) {
  guint16 m = mpt1327_channel_fcs(cw);
  return cw<<16 | m;
}

int 
mpt1327_channel_init( 
  MPT1327Channel** ppCh,
  const char* channelId,
  mpt1327_channel_recv_fn recvfn,
  mpt1327_channel_txcv_fn txcvfn,
  void* context
)
{
  
  MPT1327Channel* ch = g_new0(MPT1327Channel, 1);
  
  mskmodem_init(&ch->modem, channelId,
                modem_rx, modem_tx,
                sound_rx, sound_tx, ch);

  ch->userdata = context;
  ch->rx_callback = recvfn;
  ch->tx_callback = txcvfn;

  g_mutex_init(&ch->mutex);

  // Sound bridge circular buffer
  ch->cbsnd_size = 10240; //TODO: arbitrary value: calculate properly
  ch->cbsnd = g_new(mskmodem_sound_t, ch->cbsnd_size);

  // Tones queue
  ch->cbtone_size = 512;
  ch->cbtone = g_new(MPT1327Tone, ch->cbtone_size);

  *ppCh = ch;

  return 0;

}

void mpt1327_channel_free(MPT1327Channel** ppCh)
{
  if (ppCh && *ppCh)
  {
    MPT1327Channel* ch = *ppCh;
    mpt1327_channel_stop(ch);
    mskmodem_free(&ch->modem);
    g_free(ch->cbsnd);
    g_free(ch->cbtone);
    g_free(ch);
    *ppCh = NULL;
  }
}  

