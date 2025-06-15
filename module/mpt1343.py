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

import sys

class SYSCODE:

  labstr = {
    0: "Reserved",
    1: "All categories permitted",
    2: "Categories A and B only permitted",
    3: "Categories C and D only permitted",
    4: "Category A only permitted",
    5: "Category B only permitted",
    6: "Category C only permitted",
    7: "Category D only permitted"
  }
  
  natstr = {
    0: "National network 1",
    1: "National network 2",
    2: "Reserved",
    3: "Reserved"
  }

  @staticmethod
  def decode(syscode):
    if syscode & 0x4000: # National
      return SYSCODE_National(
          (syscode >> 12) & 0x3,
          (syscode >> 3) & 0x1FF,
          syscode & 0x7)
    else:
      return SYSCODE_Regional(
          (syscode >> 7) & 0x7F,
          (syscode >> 3) & 0xF,
          syscode & 0x7)

class SYSCODE_National(SYSCODE):

  def __init__(self, *v):
    self.net, self.ndd, self.lab = v

  def encode(self):
    syscode = 0x4000
    syscode |= (self.net & 0x3) << 12
    syscode |= (self.ndd & 0x1FF) << 3
    syscode |= self.lab & 0x7
    return syscode

  def __str__(self):
    syscode = self.encode()
    return "0x%x [National, net=%d(%s), ndd=%d, lab=%d(%s)]" % (
        syscode, self.net, SYSCODE.natstr[self.net], self.ndd, 
        self.lab, SYSCODE.labstr[self.lab])

class SYSCODE_Regional(SYSCODE):

  def __init__(self, *v):
    self.opid, self.ndd, self.lab = v

  def encode(self):
    syscode = (self.opid & 0x7F) << 7 
    syscode |= (self.ndd & 0xF) << 3
    syscode |= self.lab & 0x7
    return syscode

  def __str__(self):
    syscode = self.encode()
    return "0x%x [Regional, opid=%d, ndd=%d, lab=%d(%s)]" % (
        syscode, self.opid, self.ndd, self.lab, SYSCODE.labstr[self.lab])

if __name__=="__main__":
  if len(sys.argv)==2:
    print( SYSCODE.decode(int(sys.argv[1], 16)) )
  else:
    print("MPT1342 SYSCODE maker (MPT1343 9.3.4.2.2)")
    n = int(raw_input("1=National, 0=Regional :"))

    if n==1: # National
      net = int(raw_input("Net[0-3]               :"))
      ndd = int(raw_input("NDD [0-%d]             :" % ( (2**(9))-1) ))
      lab = int(raw_input("Lab[0-7]               :"))
      syscode = SYSCODE_National(net, ndd, lab)
    else: # Regional
      opid = int(raw_input("Operator Id[0-127]     :"))
      ndd = int(raw_input("NDD [0-%d]             :" % ( (2**(4))-1) ))
      lab = int(raw_input("Lab[0-7]               :"))
      syscode = SYSCODE_Regional(opid, ndd, lab)

    print("\n%s" % syscode)

