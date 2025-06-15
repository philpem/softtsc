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

"""This is the MPT1327 codeword generation and decoding module"""

# Synchronisation words
PREAMBLE = 0xAAAA # 1010101010101010
SYNC = 0xC4D7     # 1100010011010111
SYNT = 0x3B28     # 0011101100101000

# Special idents
ALLI    = 8191 # System wide ident
TSCI    = 8190 # Ident of TSC
IPFIXI  = 8189 # Interprefix ident
SDMI    = 8188 # Short data message ident
DIVERTI = 8187 # Divert ident
INCI    = 8186 # Include ident
REGI    = 8185 # Registration ident
DNI     = 8103 # Data network gateway ident
PABXI   = 8102 # PABX gateway ident
PSTNGI  = 8101 # General PSTN gateway ident
DUMMYI  = 0    # Dummy ident

# Aloha number tables (s7.3.3)
alhtolength2bit = {0:0,1:1,2:3,3:6}
alhtolength4bit = {0:0,1:1,2:2,3:3,4:4,5:5,6:6,7:7,8:8,9:9,10:10,11:12,12:15,
                   13:19,14:25,15:32}

##############################################################################
## Helper functions
##############################################################################

def mpt1327_fcs_py(cw):
  """Calculates MPT1327 Frame Check Sequence"""

  ck = 0
  parity = 0

  # Calculate checksum
  for n in range(48):
    b = cw >> (47-n) & 1
    parity ^= b
    if (b ^ (ck >> 15)) & 1:
      ck ^= 0x6815
    ck <<= 1

  ck = (ck ^ 0x0002) & 0xFFFE

  # Calculate parity
  for n in range(16):
    parity ^= ck >> (15-n) & 1

  return ck | parity;

# Use the C implementation of fcs if available because it's much faster
try:
  from libmpt1327modem import fcs as mpt1327_fcs
except ImportError:
  import warnings
  warnings.warn("Using slower implementation of fcs... native unavailable.",
                ImportWarning)
  mpt1327_fcs = mpt1327_fcs_py

def identstr(i):
  idents = {
    ALLI:   "ALLI",
    TSCI:   "TSCI",
    IPFIXI: "IPFIXI",
    SDMI:   "SDMI",   
    DIVERTI:"DIVERTI",
    INCI:   "INCI",
    REGI:   "REGI",
    DNI:    "DNI",
    PABXI:  "PABXI",
    PSTNGI: "PSTNGI",
    DUMMYI: "DUMMYI"
  }
  if i in idents:
    return "%04d(%s)" % (i, idents[i])
  return "%04d" % i

##############################################################################
## Codeword objects
##############################################################################

# Generic messages
def C000(typ, func):
  pkt = 0x200001 << 26 # Fixed bits for CAT 000 messages
  pkt |= (typ & 0x3) << 21
  pkt |= (func & 0x7) << 18
  return pkt

def C000_10(pfix, ident1, func):
  pkt = C000(0x2, func)
  pkt |= (pfix & 0x7F) << 40
  pkt |= (ident1 & 0x1FFF) << 27
  return pkt

def C000_11(params, parameters, func):
  pkt = C000(0x3, func)
  pkt |= (params & 0xFFFFF) << 27
  pkt |= (parameters & 0x3FFFF)
  return pkt

def C000_BCAST(sysdef, sys):
  pkt = C000(0x3, 4)
  pkt |= (sysdef & 0x1F) << 42
  pkt |= (sys & 0x7FFF) << 27
  return pkt

# MPT1327 codewords
class CCSC:
  """Control channel system codeword"""
  def __init__(self, *v):
    self.syscode = v[0]

  def cw(self):

    # Calculate CCS of CCSC (See appendix 3 of spec)
    ccs = 0xAAAAC4D40000 | (self.syscode << 1)
    f = mpt1327_fcs(ccs)
    if (f&1)==0:
      f = (1<<16) | mpt1327_fcs(ccs|1)
    f = (f >> 1) ^ 1
  
    # Generate final codeword
    cw = (self.syscode<<32) | (f << 16) | PREAMBLE
    return cw

  def __str__(self):
    return "CCSC[syscode=0x%x]" % self.syscode

class DCSC:
  """Data channel system codeword"""
  def __init__(self, *v):
    self.syscode = v[0]

  def cw(self):

    # Calculate CCS of DCSC (See appendix 3 of spec. Adapted for SYNT)
    ccs = 0xAAAA3B2A0000 | (self.syscode << 1)
    f = mpt1327_fcs(ccs)
    if (f&1)==1:
      f = (1<<16) | mpt1327_fcs(ccs|1)
    f = (f >> 1) ^ 1
  
    # Generate final codeword
    cw = (self.syscode<<32) | (f << 16) | PREAMBLE
    return cw
  
  def __str__(self):
    return "DCSC[syscode=0x%x]" % self.syscode

class GTC:
  """Go to traffic channel message"""
  def __init__(self, *v):
    self.pfix, self.ident1, self.d, self.chan, self.ident2, self.n = v

  def cw(self):
    pkt = 1 << 47
    pkt |= (self.pfix & 0x7F) << 40
    pkt |= (self.ident1 & 0x1FFF) << 27
    pkt |= (self.d & 0x1) << 25
    pkt |= (self.chan & 0x3FF) << 15
    pkt |= (self.ident2 & 0x1FFF) << 2
    pkt |= (self.n & 0x3)
    return pkt

  def __str__(self):
    return "GTC[pfix=0x%02x ident1=%s d=%d chan=%d ident2=%s n=%d(=%d)]" %\
      (self.pfix, identstr(self.ident1), self.d, self.chan, \
       identstr(self.ident2), self.n, alhtolength2bit[self.n])

class ALH_Base:
  """Aloha"""
  def __init__(self, *v):
    self.pfix, self.ident1, self.chan4, self.wt, self.rsvd, self.m, self.n = v

  def cw(self):
    pkt = C000(0x0, self.func)
    pkt |= (self.pfix & 0x7F) << 40
    pkt |= (self.ident1 & 0x1FFF) << 27
    pkt |= (self.chan4 & 0xF) << 14
    pkt |= (self.wt & 0x7) << 11
    pkt |= (self.rsvd & 0x3) << 9
    pkt |= (self.m & 0x1f) << 4
    pkt |= (self.n & 0xf)   
    return pkt

  alhstr = {
    0:"ALH",    # Any single codeword message invited.
    1:"ALHS",   # Messages invited, except RQD.
    2:"ALHD",   # Messages invited, except RQS.
    3:"ALHE",   # Emergency requests (RQE) only invited.
    4:"ALHR",   # Registration (RQR) or emergency requests (RQE) invited.
    5:"ALHX",   # Messages invited, except RQR.
    6:"ALHF",   # Fall back mode (system dependent)
    7:"ALH(RSVD)" # Reserved
  }

  def __str__(self):
    return "%s[pfix=0x%02x ident1=%s chan4=%d wt=%d rsvd=%04x m=%d n=%d(=%d)]"\
        % (self.alhstr[self.func], self.pfix, identstr(self.ident1), \
           self.chan4, self.wt, self.rsvd, self.m, self.n, 
           alhtolength4bit[self.n])

class ALH(ALH_Base):
  """Aloha - Any single codeword message invited."""
  func = 0

class ALHS(ALH_Base):
  """Aloha - Messages invited, except RQD."""
  func = 1

class ALHD(ALH_Base):
  """Aloha - Messages invited, except RQS."""
  func = 2

class ALHE(ALH_Base):
  """Aloha - Emergency requests (RQE) only invited."""
  func = 3

class ALHR(ALH_Base):
  """Aloha - Registration (RQR) or emergency requests (RQE) invited."""
  func = 4

class ALHX(ALH_Base):
  """Aloha - Messages invited, except RQR."""
  func = 5

class ALHF(ALH_Base):
  """Aloha - Fall back mode (system dependent)"""
  func = 6

class ACK_Base:
  """Acknowledgement"""
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.qual, self.n = v

  @classmethod
  def decode(cls, cw):
    o = cls(
      (cw>>40) & 0x7F,  # Pfix
      (cw>>27) & 0x1FFF,# Ident1
      (cw>>5) & 0x1FFF, # Ident2
      (cw>>4) & 0x1,    # Qual
      cw & 0xF)         # n
    return o

  def cw(self):
    pkt = C000(0x1, self.func)
    pkt |= (self.pfix & 0x7F) << 40
    pkt |= (self.ident1 & 0x1FFF) << 27
    pkt |= (self.ident2 & 0x1FFF) << 5
    pkt |= (self.qual & 0x1) << 4
    pkt |= (self.n & 0xf)
    return pkt

  ackstr = {
    0:"ACK", # General acknowledgement
    1:"ACKI",# Intermediate acknowledgement
    2:"ACKQ",# Call queued
    3:"ACKX",# Message rejected
    4:"ACKV",# Called unit unavailable
    5:"ACKE",# Acknowledge emergency call
    6:"ACKT",# Try on different address
    7:"ACKB" # Acknowledge, call-back or negative acknowledgement
  }

  def __str__(self):
    return "%s[pfix=0x%02x ident1=%s ident2=%s qual=%d n=%d]" %\
      (ACK.ackstr[self.func], self.pfix, identstr(self.ident1), \
       identstr(self.ident2), self.qual, self.n)

class ACK(ACK_Base):
  """General acknowledgement"""
  func = 0

class ACKI(ACK_Base):
  """Intermediate acknowledgement"""
  func = 1

class ACKQ(ACK_Base):
  """Call queued"""
  func = 2

class ACKX(ACK_Base):
  """Message rejected"""
  func = 3

class ACKV(ACK_Base):
  """Called unit unavailable"""
  func = 4

class ACKE(ACK_Base):
  """Acknowledge emergency call"""
  func = 5

class ACKT(ACK_Base):
  """Try on different address"""
  func = 6

class ACKB(ACK_Base):
  """Acknowledge, call-back or negative acknowledgement"""
  func = 7

class AHY: 
  """General availability check"""
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.d, self.point, \
      self.check, self.e, self.ad = v

  def cw(self):
    pkt = C000_10(self.pfix, self.ident1, 0)
    pkt |= (self.ident2 & 0x1FFF) << 5
    pkt |= (self.d & 0x1) << 4
    pkt |= (self.point & 0x1) << 3
    pkt |= (self.check & 0x1) << 2
    pkt |= (self.e & 0x1) << 1
    pkt |= (self.ad & 0x1)
    return pkt

  def __str__(self):
    return "AHY[pfix=0x%02x ident1=%s ident2=%0s d=%d point=%d check=%d e=%d ad=%d]" %\
      (self.pfix, identstr(self.ident1), identstr(self.ident2), self.d, 
       self.point, self.check, self.e, self.ad)

class AHYX:
  """Cancel alert/Waiting state message"""
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.point = v

  def cw(self):
    pkt = C000_10(self.pfix, self.ident1, 2)
    pkt |= (self.ident2 & 0x1FFF) << 5
    pkt |= (self.point & 0x1F)
    return pkt

  def __str__(self):
    return "AHYX[pfix=0x%02x ident1=%s ident2=%s point=%d]" %\
      (self.pfix, identstr(self.ident1), identstr(self.ident2), self.point)

class AHYP: # Not in MPT1343/11.5.3
  """Called unit presence monitoring"""
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.rsvd = v

  def cw(self):
    pkt = C000_10(self.pfix, self.ident1, 5)
    pkt |= (self.ident2 & 0x1FFF) << 5
    pkt |= (self.rsvd & 0x1F)
    return pkt

  def __str__(self):
    return "AHYP[pfix=0x%02x ident1=%s ident2=%s rsvd=%d]" %\
      (self.pfix, identstr(self.ident1), identstr(self.ident2), self.rsvd)

class AHYQ:
  """Status ahoy message"""
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.status = v

  def cw(self):
    pkt = C000_10(self.pfix, self.ident1, 6)
    pkt |= (self.ident2 & 0x1FFF) << 5
    pkt |= (self.status & 0x1F)
    return pkt

  def __str__(self):
    return "AHYQ[pfix=0x%02x ident1=%s ident2=%s status=%d]" %\
      (self.pfix, identstr(self.ident1), identstr(self.ident2), self.status)

class AHYC:
  """Short data invitation message"""
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.slots, self.desc = v

  def cw(self):
    pkt = C000_10(self.pfix, self.ident1, 7)
    pkt |= (self.ident2 & 0x1FFF) << 5
    pkt |= (self.slots & 0x03) << 3
    pkt |= (self.desc & 0x07)
    return pkt

  def __str__(self):
    return "AHYC[pfix=0x%02x ident1=%s ident2=%s slots=%d desc=%d]" %\
      (self.pfix, identstr(self.ident1), identstr(self.ident2), self.slots, \
       self.desc)

class MARK:
  """Control channel marker"""
  def __init__(self, *v):
    self.sys = v[0]
    self.chan4 = 0
    raise NotImplementedError("MARK is not implemented yet!")

  def cw(self):
    a = 0
    b = 0
    pkt = C000(0x3, 0)
    pkt |= (self.chan4 & 0x1F) << 43
    pkt |= (a & 0x1) << 42
    pkt |= (self.sys & 0x7FFF) << 27
    pkt |= (b & 0x3FFFF)
    return pkt

class MAINT:
  """Call maintenance message"""
  func = 1
  def __init__(self, *v):
    self.pfix, self.ident1, self.chan, self.oper, self.rsvd = v

  @staticmethod
  def decode(cw):
    o = MAINT(
      (cw>>40) & 0x7F,
      (cw>>27) & 0x1FFF,
      (cw>>8) & 0x3FF,
      (cw>>5) & 0x7,
      cw & 0x1F)
    return o

  def cw(self):
    pkt = C000(0x3, 1)
    pkt |= (self.pfix & 0x7F) << 40
    pkt |= (self.ident1 & 0x1FFF) << 27
    pkt |= (self.chan & 0x3FF) << 8
    pkt |= (self.oper & 0x7) << 5
    pkt |= (self.rsvd & 0x1F)
    return pkt

  operstr = {
    0: "Presel On",
    1: "Presel Off",
    2: "Periodic",
    3: "Disconnect",
    4: "Spare",
    5: "Reserved",
    6: "Clear down",
    7: "Disable TX"
  }

  def __str__(self):
    return "MAINT[pfix=0x%02x ident1=%s chan=%d oper=%d (%s) rsvd=%d]" %\
      (self.pfix, identstr(self.ident1), self.chan, self.oper, \
       MAINT.operstr[self.oper], self.rsvd)

class CLEAR:
  """Clear down traffic channel"""
  def __init__(self, *v):
    self.chan, self.cont, self.rsvd, self.spare = v

  def cw(self):
    pkt = C000(0x3, 2)
    pkt |= (self.chan & 0x3FF) << 37
    pkt |= (self.cont & 0x3FF) << 27
    pkt |= (self.rsvd & 0x7) << 14
    pkt |= (self.spare & 0x3) << 12
    pkt |= 0xAAA # 12 bits of bit reversals
    return pkt

  def __str__(self):
    return "CLEAR[chan=%d cont=%d rsvd=%d spare=%d]" %\
      (self.chan, self.cont, self.rsvd, self.spare)

class MOVE:
  def __init__(self, *v):
    self.pfix, self.ident1, self.cont, self.m, self.rsvd, self.spare = v

  def cw(self):
    pkt = C000(0x3, 3)
    pkt |= (self.pfix & 0x7F) << 40
    pkt |= (self.ident1 & 0x1FFF) << 27
    pkt |= (self.cont & 0x3FF) << 8
    pkt |= (self.m & 0x1F) << 3
    pkt |= (self.rsvd & 0x3) << 1
    pkt |= (self.spare & 0x1) << 1
    return pkt

  def __str__(self):
    return "MOVE[...]"

class BCAST_ADDCONTROL:
  def __init__(self, *v):
    self.sys, self.chan, self.spare, self.rsvd = v

  def cw(self):
    pkt = C000_BCAST(0x0, self.sys)
    pkt |= (self.chan & 0x3FF) << 8
    pkt |= (self.spare & 0x3) << 6
    pkt |= (self.rsvd & 0x3F)
    return pkt

  def __str__(self):
    return "BCAST_ADDCONTROL[...]"

class BCAST_DELCONTROL:
  def __init__(self, *v):
    self.sys, self.chan, self.spare, self.rsvd = v

  def cw(self):
    pkt = C000_BCAST(0x0, self.sys)
    pkt |= (self.chan & 0x3FF) << 8
    pkt |= (self.spare & 0x3) << 6
    pkt |= (self.rsvd & 0x3F)
    return pkt
  
  def __str__(self):
    return "BCAST_DELCONTROL[...]"

class BCAST_MAINT:
  def __init__(self, *v):
    self.sys, self.per, self.ival, self.pon, self.id, self.rsvd, self.spare = v

  def cw(self):
    pkt = C000_BCAST(0x2, self.sys)
    pkt |= (self.per & 0x1) << 17
    pkt |= (self.ival & 0x1F) << 12
    pkt |= (self.pon & 0x1) << 11
    pkt |= (self.id & 0x1) << 10
    pkt |= (self.rsvd & 0x3) << 8
    pkt |= (self.spare & 0xFF)
    return pkt

  def __str__(self):
    return "BCAST_MAINT[...]"

class BCAST_REG:
  def __init__(self, *v):
    self.sys, self.rsvd, self.spare = v

  def cw(self):
    pkt = C000_BCAST(0x3, self.sys)
    pkt |= (self.rsvd & 0xF) << 14
    pkt |= (self.spare & 0x3FFF)
    return pkt
  
  def __str__(self):
    return "BCAST_REG[...]"

class RQS:
  """Request Simple call"""
  func = 0
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.dt, self.level, self.ext, \
      self.flag1, self.flag2 = v

  @staticmethod
  def decode(cw):
    o = RQS(
      (cw>>40) & 0x7F,
      (cw>>27) & 0x1FFF,
      (cw>>5) & 0x1FFF,
      (cw>>4) & 0x1,
      (cw>>3) & 0x1,
      (cw>>2) & 0x1,
      (cw>>1) & 0x1,
      cw & 0x1
    )
    return o

  def __str__(self):
    return "RQS[pfix=0x%02x ident1=%s ident2=%s dt=%d level=%d ext=%d flag1=%d flag2=%d]" %\
        (self.pfix, identstr(self.ident1), identstr(self.ident2), self.dt, \
         self.level, self.ext, self.flag1, self.flag2)

class RQX:
  """Request call cancel / abort transaction"""
  func = 2
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.rsvd = v

  @staticmethod
  def decode(cw):
    o = RQX(
      (cw>>40) & 0x7F,
      (cw>>27) & 0x1FFF,
      (cw>>5) & 0x1FFF,
      cw & 0x1F)
    return o
  
  def __str__(self):
    return "RQX[pfix=0x%02x ident1=%s ident2=%s rsvd=%d]" %\
        (self.pfix, identstr(self.ident1), identstr(self.ident2), self.rsvd) 
  
class RQT:
  """Request call diversion"""
  func = 3
  def __init__(self, *v):
    self.pfix, self.ident1 = v

  @staticmethod
  def decode(cw):
    o = RQT(
      (cw>>40) & 0x7F,
      (cw>>27) & 0x1FFF)
    return o

class RQE:
  """Request emergency call"""
  func = 4
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.d, self.rsvd, self.ext, \
      self.flag1, self.flag2 = v

  @staticmethod
  def decode(cw):
    o = RQE(
      (cw>>40) & 0x7F,
      (cw>>27) & 0x1FFF,
      (cw>>5) & 0x1FFF,
      (cw>>4) & 0x1,
      (cw>>3) & 0x1,
      (cw>>2) & 0x1,
      (cw>>1) & 0x1,
      cw & 0x1)
    return o

  def __str__(self):
    return "RQE[pfix=0x%02x ident1=%s ident2=%s d=%d rsvd=%d ext=%d flag1=%d flag2=%d]" %\
        (self.pfix, identstr(self.ident1), identstr(self.ident2), self.d, 
         self.rsvd, self.ext, self.flag1, self.flag2)

class RQR:
  """Request to register"""
  func = 5
  def __init__(self, *v):
    self.pfix, self.ident1, self.info, self.rsvd = v

  @staticmethod
  def decode(cw):
    o = RQR(
      (cw>>40) & 0x7F,
      (cw>>27) & 0x1FFF,
      (cw>>3) & 0x7FFF,
      cw & 0x7)
    return o

  def __str__(self):
    return "RQR[pfix=0x%02x ident1=%s info=%d rsvd=%d]" %\
        (self.pfix, identstr(self.ident1), self.info, self.rsvd)

class RQQ:
  """Request status transaction"""
  func = 6
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.status = v

  @staticmethod
  def decode(cw):
    o = RQQ(
      (cw>>40) & 0x7F,
      (cw>>27) & 0x1FFF,
      (cw>>5) & 0x1FFF,
      cw & 0x1F)
    return o

  def __str__(self):
    return "RQQ[pfix=0x%02x ident1=%s ident2=%s status=%d]" %\
        (self.pfix, identstr(self.ident1), identstr(self.ident2), self.status)

class RQC:
  """Request to send short data message"""
  func = 7
  def __init__(self, *v):
    self.pfix, self.ident1, self.ident2, self.slots, self.ext, \
      self.flag1, self.flag2 = v

  @staticmethod
  def decode(cw):
    o = RQC(
      (cw>>40) & 0x7F,
      (cw>>27) & 0x1FFF,
      (cw>>5) & 0x1FFF,
      (cw>>3) & 0x3,
      (cw>>2) & 0x1,
      (cw>>1) & 0x1,
      cw & 0x1
    )
    return o
  
  def __str__(self):
    return "RQC[pfix=0x%02x ident1=%s ident2=%s slots=%d ext=%d flag1=%d flag2=%d]" %\
        (self.pfix, identstr(self.ident1), identstr(self.ident2), self.slots,\
         self.ext, self.flag1, self.flag2)

class SAMIS:
  """Inbound Solicited Single Address Message"""
  def __init__(self, *v):
    self.desc, self.parameters1, self.parameters2, self.mfgcode, self.model, \
      self.chkbits, self.serial = v

  @staticmethod
  def decode(cw):
    parameters1 = (cw>>27) & 0xFFFFF
    parameters2 = cw & 0x3FFFF
    o = SAMIS(
      (cw >> 18) & 0x7,
      parameters1,
      parameters2,
      (parameters1>>12) & 0xFF,
      (parameters1>>8) & 0xF,
      (parameters1) & 0xFF,
      parameters2)
    return o

  def __str__(self):
    return "SAMIS[esn=%03d/%02d/%06d chkbits=0x%x]" % (self.mfgcode, 
        self.model, self.serial, self.chkbits);

##############################################################################
## RU to TSC decoder
##############################################################################

CAT000Classes = {
  1: { # ACK
    0:ACK, # General acknowledgement
    1:ACKI,# Intermediate acknowledgement
    2:ACKQ,# Call queued
    3:ACKX,# Message rejected
    4:ACKV,# Called unit unavailable
    5:ACKE,# Acknowledge emergency call
    6:ACKT,# Try on different address
    7:ACKB # Acknowledge, call-back or negative acknowledgement}
  },

  2: { # REQUEST
    0: RQS,
    2: RQX,
    3: RQT,
    4: RQE,
    5: RQR,
    6: RQQ,
    7: RQC
  },

  3: { # MISC
    1: MAINT
  }
}

def RUtoTSCDecode(cw):
  """Decodes cw's sent from Radio Units (RU) to the TSC"""
  if cw & 0x800000000000 and cw & 0x4000000: # Address CW & not GTC (S2.0)
    cat = (cw >> 23) & 0x7;

    if cat==0: # Cat 000
      type = (cw >> 21) & 0x3
      func = (cw >> 18) & 0x7
      return CAT000Classes[type][func].decode(cw)
        
    elif cat==1: # Cat 001
      desc = (cw >> 18) & 0x7
      if desc==0: # SAMIS (TODO: This is a little more nuanced in reality)
        return SAMIS.decode(cw)

  return None

##############################################################################
## Unit test functions
##############################################################################
def MPT1327_test():

  # Test FCS generator with samples acquired from real systems on the air
  assert(mpt1327_fcs(0x4A89740BAAAA)==SYNC) # Sys=4A89
  assert(mpt1327_fcs(0x80000401B80A)==0x949C)
  assert(mpt1327_fcs(0x8A544C720409)==0xF6EE)
  assert(mpt1327_fcs(0x80000401B800)==0xF531)
  assert(mpt1327_fcs(0x8E544C701000)==0xE4C9)

  # Test CCSC/DCSC generators
  for n in range(0x7FFF):
    assert(mpt1327_fcs(CCSC(n).cw())==SYNC)
    assert(mpt1327_fcs(DCSC(n).cw())==SYNT)

if __name__=="__main__":
  MPT1327_test()

