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

#ifndef SOUND_H
#define SOUND_H

#define MSKMODEM_SOUND_FULLSCALE 1.0f
typedef float mskmodem_sound_t;

struct MSKModemSoundContext_s;
typedef struct MSKModemSoundContext_s MSKModemSoundContext;

typedef void(*MSKModemSoundRxFn) (const mskmodem_sound_t* buf,
                                  gint32 samplecount,
                                  void* context);

typedef void(*MSKModemSoundTxFn) (mskmodem_sound_t* buf,
                                  gint32 samplecount,
                                  void* context);

int
mskmodem_sound_init (
  MSKModemSoundContext** pCtx,
  const char* channelId,
  MSKModemSoundRxFn rx_f,
  MSKModemSoundTxFn tx_f,
  void* context
);

void
mskmodem_sound_free (
  MSKModemSoundContext** ctx
);

int
mskmodem_sound_run (
  MSKModemSoundContext* ctx
);

int
mskmodem_sound_stop (
  MSKModemSoundContext* ctx
);

#endif /* SOUND_H */

