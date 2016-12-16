/** \copyright
 * Copyright (c) 2015, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file PacketDefs.hxx
 *
 * Protocol definitions for mobile station packets.
 *
 * @author Balazs Racz
 * @date 15 May 2015
 */

#include "src/usb_proto.h"

namespace mobilestation {

typedef openlcb::Payload Packet;

struct PacketDefs {
  static void SetupPacketHeader(Packet* pkt, uint16_t eid, uint16_t sid,
                                uint8_t len) {
    pkt->push_back(CMD_CAN_PKT);
    pkt->push_back(eid >> 8);
    pkt->push_back(eid & 0xff);
    pkt->push_back(sid >> 8);
    pkt->push_back(sid & 0xff);
    pkt->push_back(len);
  }

  static void SetupQueryPacket(Packet* pkt, uint16_t eid, uint16_t sid,
                               uint8_t d1, uint8_t d2) {
    SetupPacketHeader(pkt, eid, sid, 2);
    pkt->push_back(d1);
    pkt->push_back(d2);
  }

  static void SetupEStopQueryPacket(Packet* pkt) {
    SetupQueryPacket(pkt, 0x4008, 0x0901, 1, 0);
  }

  static uint16_t LokEid(int id) {
    return (((id << 2) + 1) << 8) + 1;
  }

  static void SetupLokSpeedQueryPacket(Packet* pkt, int id) {
    SetupQueryPacket(pkt, 0x4048, LokEid(id), 1, 0);
  }

  static void SetupLokFnQueryPacket(Packet* pkt, int lokid, int fnid) {
    SetupQueryPacket(pkt, 0x4048, LokEid(lokid), fnid, 0);
  }

};

}  // namespace mobilestaton
