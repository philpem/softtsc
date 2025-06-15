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

#ifndef MSKMODEM_H
#define MSKMODEM_H

#include "sound.h"

struct MSKModemContext_s;
typedef struct MSKModemContext_s MSKModemContext;

typedef void(*MSKModemTxFn)(guint64* cw, void* userdata);
typedef void(*MSKModemRxFn)(guint32 bit, void* userdata);

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
);

void
mskmodem_free
(
  MSKModemContext** ppCtx
);

int
mskmodem_run
(
  MSKModemContext* ctx
);

int
mskmodem_stop
(
  MSKModemContext* ctx
);

#endif /* MSKMODEM_H */

