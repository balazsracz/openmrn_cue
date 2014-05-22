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
#include "dcc/UpdateLoop.hxx"
#include "utils/BufferQueue.hxx"
#include "utils/Hub.hxx"
#include "utils/CanIf.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/DefaultNode.hxx"

/// @TODO(balazs.racz) this is not nice.
extern nmranet::DefaultNode g_node;

namespace bracz_custom {

enum {
  CAN_ID_TRACKPROCESSOR = 0b11100000001,  // 0x701
  CAN_ID_COMMANDSTATION = 0b11100000000,  // 0x700
};

enum {
  TRACKCMD_KEEPALIVE = 1,
  TRACKCMD_LOG = 3,
  TRACKCMD_ACK = 5,
  TRACKCMD_NOACK = 7,
  TRACKCMD_POWERON = 9,
  TRACKCMD_POWEROFF = 11,
  TRACKCMD_RESET = 13,
  TRACKCMD_DEADBAND = 15,
  TRACKCMD_BREAK = 17,
  TRACKCMD_DISABLE = 19,
  TRACKCMD_ENABLE = 21,
};

class TrackPowerOnOffBit : public nmranet::BitEventInterface {
 public:
  TrackPowerOnOffBit(uint64_t event_on, uint64_t event_off, PacketFlowInterface* track)
      : BitEventInterface(event_on, event_off), track_(track), state_(false) {}

  virtual bool GetCurrentState() { return state_; }
  virtual void SetState(bool new_value) {
    auto* b = track_->alloc();
    b->data()->dlc = 0;
    b->data()->set_cmd(new_value ? TRACKCMD_POWERON : TRACKCMD_POWEROFF);
    track_->send(b);
    state_ = new_value;
  }

  virtual nmranet::AsyncNode* node()
  {
    return &g_node;
  }

 private:
  PacketFlowInterface* track_;
  /// @TODO(balazs.racz): this state should be updated from the alive bit in
  /// the keepalive packets.
  bool state_;
};

/** This state flow will take every incoming packet and send it out on a CANbus
 * interface to the "track processor slave" address. That will go in a standard
 * packet so as not to disturb OpenLCB communication. */
class TrackIfSend : public StateFlow<Buffer<dcc::Packet>, QList<1> > {
 public:
  TrackIfSend(CanHubFlow* can_hub)
      : StateFlow<Buffer<dcc::Packet>, QList<1> >(can_hub->service()),
        device_(can_hub) {}

 private:
  Action entry() OVERRIDE;
  Action fill_packet();

  CanHubFlow* device_;
};

class TrackIfReceive : public IncomingFrameFlow {
 public:
  /** Creates a port to listen on the feedback from the track processor.
   * @param interface is the CAN bus port to listen on
   * @param packet_q is a flow that will get an empty packet whenever the
   * track processor is ready to receive the next outgoing packet. */
  TrackIfReceive(CanIf* interface, PacketFlowInterface* packet_q);
  ~TrackIfReceive();

  Action entry() OVERRIDE;
  Action handle_keepalive();
  Action respond_keepalive();

 private:
  /** This pool will be the source of outgoing packet buffers. */
  FixedPool pool_;
  /** @TODO(balazs.racz) replace this with a service keeping all objects. */
  CanIf* interface_;
  PacketFlowInterface* packetQueue_;
};

}  // namespace bracz_custom

#endif  // _CUSTOM_TRACKINTERFACE_HXX_
