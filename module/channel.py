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

"""Implementation of MPT1327 channel controller"""

import sys
import logging
from libmpt1327modem import MPT1327Modem
import mpt1327 as mpt
from queue import Queue

class TRAF:
  """Traffic SYNC codeword"""
  def cw(self):
    return 1 # Special meaning
  def __str__(self):
    return "TRAFFIC"

class TXItem:
  """TX queue item"""
  def __init__(self, cw, txcompletionitem=None, rxcompletionitem=None):
    self.cw = cw
    self.txcomplete = txcompletionitem
    self.rxcomplete = rxcompletionitem

class TXCompletionItem:
  """TX completion item"""
  def __init__(self, cfunc, data=None):
    self.cfunc = cfunc
    self.data = data
  def complete(self, ch):
    return self.cfunc(self.data, ch)

class RXCompletionItem:
  """Solicited RX completion item"""
  def __init__(self, size, cfunc, data=None):
    self.size = size
    self.remaining = size
    self.cfunc = cfunc
    self.data = data
    self.cws = []
    self.slottimeout = 4 # 64-bit slots

  def addcw(self, ch, cw):
    self.cws += [cw]
    self.remaining -= 1
    self.slottimeout = 4 # Reset timeout
    if self.remaining>0:
      return 0 # More codewords needed
    else:
      self.cfunc(self.data, 0, ch, self.cws)
      return 1

  def tick(self, ch):
    self.slottimeout -= 1
    if self.slottimeout==0:
      ch.rxcomplete = None # We timed out of our slot so no point waiting
      self.cfunc(self.data, 1, ch, None)      

class Channel:
  """MPT1327 channel controller"""

  def __init__(self, syscode, channelnumber, rxfunc, rxfuncdata, txstate=0):
    self.syscode = syscode
    self.channelnumber = channelnumber
    self.rxfunc = rxfunc
    self.rxfuncdata = rxfuncdata
    self.ahlcount = 0           # Aloha frame counter
    self.rxcomplete = None      # RX completion object for solicited message
    self.txcomplete = None      # TX completion object
    self.txqueue = Queue()      # Control channel transmission queue
    self.txtrafqueue = Queue()  # Traffic channel transmission queue
    self.txstate = txstate      # TX state machine state
    self.txreserved = 0         # Slot reservations
    self.txreserveditem = None  # RX completion object holder
    channelId = "TSC-Ch.%d" % channelnumber
    self.modem = MPT1327Modem(channelId, Channel._rxcv, Channel._txcv, self)
    self.logger = logging.getLogger(__name__)
    self.morse = None
    self.lastcw = None

  def _tick(self):
    if self.rxcomplete:
      self.rxcomplete.tick(self)

  def _txcvimpl(self):
    cw = None
    n = 0

    # Log last transmitted
    if self.lastcw:
      self.logger.debug("TX[%d]: %s", self.channelnumber, self.lastcw)
      self.lastcw = None

    # Transmission provides our ticker for time-outs
    self._tick()

    # Call transmission complete item
    if self.txcomplete:
      nextstate = self.txcomplete.complete(self)
      self.txcomplete = None
      if nextstate is not None:
        self.txstate = nextstate

    # Queue received item for action by receiver
    if self.txreserveditem:
      self.rxcomplete = self.txreserveditem
      self.txreserveditem = None

    if self.txstate==0: # STATE:0 - DECIDE OR BEGIN CODEWORD

      # Morse ident transmission - begins outside MPT1327 frame only
      if not self.morse==None and self.ahlcount==0:
        self.modem.morse(self.morse, Channel._morse_done, self)
        self.morse = None
        self.txstate = 4
      else:
        cw = mpt.CCSC(self.syscode)
        self.txstate = 1

    elif self.txstate==1: # STATE:1 - CONTROL CHANNEL CODEWORD
      self.txstate = 0
      
      # Frame handling
      if self.ahlcount==0:
        self.ahlcount = 5
        n = self.ahlcount
      self.ahlcount -= 1
      if self.ahlcount<0:
        self.ahlcount = 0

      # Withdrawn slots handling (7.2.5)
      if self.txreserved:
        self.txreserved -= 1
      if self.txreserved: # Multi slot...
          cw = mpt.AHY(0, mpt.DUMMYI, mpt.DUMMYI, 0, 0, 0, 0 ,0)

      # Queue handling
      elif not self.txqueue.empty():
        o = self.txqueue.get_nowait()
        cw = o.cw
        self.txcomplete= o.txcomplete
        self.txreserved = 0
        if o.rxcomplete:
          self.txreserved = o.rxcomplete.size
          self.txreserveditem = o.rxcomplete
      else:
        cw = mpt.ALH(0,0,self.channelnumber,6,0,0,n)

    elif self.txstate==2: # STATE:2 - TRAFFIC CHANNEL QUIESCENT STATE
      self.alhcount = 0
      if not self.txtrafqueue.empty(): 
        #cw = mpt.DCSC(self.syscode)
        cw = TRAF()
        self.txstate = 3

    elif self.txstate==3: # STATE:3 - TRAFFIC CHANNEL CODEWORD STATE
      self.txstate = 2
      o = self.txtrafqueue.get_nowait()
      cw = o.cw
      self.txcomplete = o.txcomplete
      self.txreserved = 0
      if o.rxcomplete:
        self.txreserved = o.rxcomplete.size
        self.txreserveditem = o.rxcomplete

    elif self.txstate==4: # STATE:4 - MORSE IDENT/ETC (TODO)
      pass

    # Encode codeword and return
    if cw:
      self.lastcw = cw
      return cw.cw()
    else:
      return 0

  def _rxcvimpl(self, cw):

    # If we're expecting a response in reserved slot(s)...
    if self.rxcomplete:
      self.logger.debug("RX[%d] RSVD: 0x%x", self.channelnumber, cw)
      if self.rxcomplete.addcw(self, cw):
        self.rxcomplete = None
      return 0

    # Otherwise this is a random access which *must* be an address codeword
    else:
      o = mpt.RUtoTSCDecode(cw)
      self.logger.debug("RX[%d]: 0x%x %s", self.channelnumber, cw, o)
      if (o):
        self.rxfunc(self.rxfuncdata, self, o)

  def _morse_done(self):
    self.txstate = 0

  def _txcv(self):
    try:
      return self._txcvimpl()
    except:
      self.logger.exception("TX[%d] Exception", self.channelnumber)

  def _rxcv(self, cw):
    try:
      return self._rxcvimpl(cw)
    except:
      self.logger.exception("RX[%d] Exception", self.channelnumber)

  def _Tx(self, queue, cw, txfunc, txdata, rxlen, rxfunc, rxdata):

    # TX completion object
    txcompl = None
    if txfunc:
      txcompl = TXCompletionItem(txfunc, txdata)

    # Solicited reply - reserve rxlen frames after transmission
    rxcompl = None
    if rxlen>0:
      rxcompl = RXCompletionItem(rxlen, rxfunc, rxdata)
   
    # Add to transmit queue
    queue.put(TXItem(cw, txcompl, rxcompl))

  def Start(self):
    self.modem.start()

  def Tx(self, cw, txfunc=None, txdata=None, rxlen=0, rxfunc=None, rxdata=None):
    self._Tx(self.txqueue, cw, txfunc, txdata, rxlen, rxfunc, rxdata)
  
  def TxTraf(self, cw, txfunc=None, txdata=None, rxlen=0, rxfunc=None, 
             rxdata=None):
    self._Tx(self.txtrafqueue, cw, txfunc, txdata, rxlen, rxfunc, rxdata)

if __name__=="__main__":

  def sammis(data, err, ch, cws):
    if err:
      print("*** TIMEOUT")
    else:
      print("*** SAMMIS!!!")

  def rxfunc(data, ch, o):
    pass

  def txcompl(data, ch):
    print("TX complete", data, ch)

  def gtccompl(data, ch):
    print("GTC complete.")
    return 2

  logging.basicConfig(filename="tsc-debug.log", 
                      filemode="w",
                      level=logging.DEBUG)

  ch = Channel(0x3201, 1, rxfunc, None)
  ch.Start()

  while True:
    a = input("Cmd:>")

    if a=="free":
      del(ch.modem)
    if a=="stop":
      print("Stopping...")
      ch.modem.stop()
    if a=="start":
      print("Starting...")
      ch.Start()

    if a=="g":
      ch.Tx(mpt.GTC(0, 3, 0, 1, mpt.PABXI, 0))
      ch.Tx(mpt.GTC(0, 3, 0, 1, mpt.PABXI, 0), gtccompl)

    if a=="?":
      ch.Tx(mpt.AHY(0, 3, mpt.TSCI, 1, 0, 0, 0, 0), txcompl, None)

    if a=="s":
      ch.Tx(mpt.AHYC(0, 3, mpt.TSCI, 1, 0), None, None, 1, sammis, None)

    if a=="c":
      ch.TxTraf(mpt.CLEAR(1, 0, 0, 0))
      ch.TxTraf(mpt.CLEAR(1, 0, 0, 0))

