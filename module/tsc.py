#!/bin/env python3
# SoftTSC - Software MPT1327 Trunking System Controller
# Copyright (C) 2013-2014 Paul Banks (http://paulbanks.org)
# 
# This file is part of SoftTSC
#
# SoftTSC is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# SoftTSC is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with SoftTSC.  If not, see <http://www.gnu.org/licenses/>.
#

"""Implementation of a basic MPT1327 TSC"""

import sys
import logging
from time import sleep
import mpt1327 as mpt
from channel import Channel

call = None

class Call:
  def __init__(self, *v):
    ch, self.ci = v
    print("Creating call %d to %d" % \
        (self.ci.ident2, self.ci.ident1))
    self.AHY(ch)

  def AHY(self, ch):
    ch.Tx(mpt.AHY(self.ci.pfix, 
                  self.ci.ident1, 
                  self.ci.ident2, 0, 0, 1, 0, 0), 
                  None, None,
                  1, Call.AHYUpdate, self)

  def AHYUpdate(self, timeout, ch, cws):
    if timeout:
      print("Called unit unavailable")
      ch.Tx(mpt.ACKV(self.ci.pfix, self.ci.ident1, self.ci.ident2, 0, 0))
    else:
      cw = mpt.RUtoTSCDecode(cws[0])
      if isinstance(cw, mpt.ACKI):
        print("Got reply, progressing call...")
        ch.Tx(mpt.GTC(self.ci.pfix, self.ci.ident1, 0, 1, self.ci.ident2, 0))
        ch.Tx(mpt.GTC(self.ci.pfix, self.ci.ident1, 0, 1, self.ci.ident2, 0),
              self.ChannelReady, None)
      else:
        print("Call::AHYUpdate: Unexpected message:", cw)

  def ChannelReady(self, data, ch):
    ch.txstate = 2


def CreateCall(ch, o):
  global call

  # You can't call yourself, we don't do data and we don't do simultaneous
  if o.ident1==o.ident2 or o.dt or call:
    ch.Tx(mpt.ACKX(o.pfix, o.ident1, o.ident2, 0, 0))
    return 0;

  # Create call
  call = Call(ch, o)

def RestartChannel(data, ch):
  print("Restart CC")
  ch.txstate = 0

def rxfunc(data, ch, o):
  global call

  if isinstance(o, mpt.RQS): # Simple call
    CreateCall(ch, o)
  elif isinstance(o, mpt.RQE): # Emergency call request
    ch.Tx(mpt.ACKX(o.pfix, o.ident1, o.ident2, 0, 0))
  elif isinstance(o, mpt.RQR): # Request to register
    print("Registration:", o)
    ch.Tx(mpt.ACK(o.pfix, mpt.TSCI, o.ident2, 0, 0))
  elif isinstance(o, mpt.RQC): # Short message
    print("Short message:", o)
    ch.Tx(mpt.ACKX(o.pfix, o.ident1, o.ident2, 0, 0))
  elif isinstance(o, mpt.MAINT): # Maintenance message
    if call:
      if o.oper==0: # Presel ON
        ch.modem.bridge(1)
      if o.oper==1: # Presel OFF
        ch.modem.bridge(0)
      if o.oper==3: # Disconnect
        print("DISCONNECT")
        call = None
        ch.TxTraf(mpt.CLEAR(ch.channelnumber, 0, 0, 0))
        ch.TxTraf(mpt.CLEAR(ch.channelnumber, 0, 0, 0))
        ch.TxTraf(mpt.CLEAR(ch.channelnumber, 0, 0, 0), RestartChannel, None)

  else:
    print("Unimplemented request", o)

def main():
  logging.basicConfig(filename="tsc-debug.log", 
                      filemode="w",
                      level=logging.DEBUG)
  ch = Channel(0x3201, 1, rxfunc, None)

  ch.Start()

  while True:
    a = input("Cmd (X to stop) >").lower()
    if a=="x":
      break

    if a.startswith("m"):
      ch.morse = a[1:].upper()

if __name__=="__main__":
  main()

