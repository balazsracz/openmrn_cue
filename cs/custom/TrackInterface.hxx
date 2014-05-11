/** \copyright
 * Copyright (c) 2014, Balazs Racz
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
 * \file TrackInterface.hxx
 *
 * Code that sends outgoing packets to the track interface board via CANbus.
 *
 * @author Balazs Racz
 * @date 11 May 2014
 */

#ifndef _CUSTOM_TRACKINTERFACE_HXX_
#define _CUSTOM_TRACKINTERFACE_HXX_

#include "dcc/Packet.hxx"
#include "utils/PipeFlow.hxx"
#include "utils/CanIf.hxx"

namespace bracz_custom {

enum {
  CAN_ID_TRACKPROCESSOR = 0b11100000001,  // 0x701
  CAN_ID_COMMANDSTATION = 0b11100000000,  // 0x700
};

/** This state flow will take every incoming packet and send it out on a CANbus
 * interface to the "track processor slave" address. That will go in a standard
 * packet so as not to disturb OpenLCB communication. */
class TrackIfSend : public StateFlow<Buffer<dcc::Packet>, QList<1> > {
 public:
  TrackIfSend(CanHubFlow* can_hub)
      : StateFlow<Buffer<dcc::Packet>, QList<1> >(can_hub.service()),
        device_(can_hub) {}

 private:
  Action entry() OVERRIDE;
  Action fill_packet();

  CanHub* device_;
};

class TrackIfReceive : public


}  // namespace bracz_custom

#endif  // _CUSTOM_TRACKINTERFACE_HXX_
