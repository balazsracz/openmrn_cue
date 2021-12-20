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

#include "custom/TrackInterface.hxx"

#include "custom/HostLogging.hxx"
#include "utils/constants.hxx"

namespace bracz_custom {

StateFlowBase::Action TrackIfSend::entry() {
  return allocate_and_call(device_, STATE(fill_packet));
}

StateFlowBase::Action TrackIfSend::fill_packet() {
  auto* b = get_allocation_result(device_);
  b->set_done(message()->new_child());
  auto* f = b->data()->mutable_frame();
  CLR_CAN_FRAME_EFF(*f);
  SET_CAN_FRAME_ID(*f, CAN_ID_TRACKPROCESSOR);
  dcc::Packet* packet = message()->data();
  f->data[0] = packet->header_raw_data;
  f->can_dlc = packet->dlc + 1;
  HASSERT(f->can_dlc <= 8);
  memcpy(f->data + 1, packet->payload, packet->dlc);
  send_host_log_event(HostLogEvent::TRACK_SENT);
  device_->send(b);
  return release_and_exit();
}

enum {
  CS_CAN_FILTER = CanMessageData::CAN_STD_FRAME_FILTER | CAN_ID_COMMANDSTATION,
  CS_CAN_MASK = CanMessageData::CAN_STD_FRAME_MASK | 0x7FF,
};

/* Specifies how many packets we can have maxiumum pending to send to the track
 * processor. This value should be typically the same as the track processor's
 * internal packet buffer size. It shouldn't be too large or else the latency
 * of urgent packets will be high. */
DECLARE_CONST(track_processor_packet_buffer_count);

TrackIfReceive::TrackIfReceive(CanIf* interface, dcc::TrackIf* packet_q)
    : IncomingFrameFlow(interface->service()),
      pool_(sizeof(Buffer<dcc::Packet>),
            config_track_processor_packet_buffer_count()),
      interface_(interface),
      packetQueue_(packet_q) {
  interface_->frame_dispatcher()->register_handler(this, CS_CAN_FILTER,
                                                  CS_CAN_MASK);
}

TrackIfReceive::~TrackIfReceive() {
  interface_->frame_dispatcher()->unregister_handler(this, CS_CAN_FILTER,
                                                    CS_CAN_MASK);
}

StateFlowBase::Action TrackIfReceive::entry() {
  auto* f = &message()->data()->frame();
  if (!f->can_dlc) return release_and_exit();
  uint8_t cmd = f->data[0];
  switch(cmd) {
    case TRACKCMD_KEEPALIVE: {
      return call_immediately(STATE(handle_keepalive));
    }
    default:
      return release_and_exit();
  }
}

StateFlowBase::Action TrackIfReceive::handle_keepalive() {
  auto* f = &message()->data()->frame();
  bool is_alive = true;
  if (f->can_dlc > 2) {
    is_alive = f->data[2];
    if (is_alive) {
      send_host_log_event(HostLogEvent::TRACK_ALIVE);
    }
  }
  if (f->can_dlc > 1) {
    int free_packet_count = f->data[1];
    while (is_alive && free_packet_count--) {
      Buffer<dcc::Packet>* b;
      pool_.alloc(&b, nullptr);
      // If the pool is exhausted, do not send any more packets even if the
      // track processor says there are free buffers.
      if (!b) break;
      packetQueue_->send(b);
    }
  }
  release();
  return allocate_and_call(interface_->frame_write_flow(), STATE(respond_keepalive));
}

StateFlowBase::Action TrackIfReceive::respond_keepalive() {
  auto* b = get_allocation_result(interface_->frame_write_flow());
  auto* f = b->data()->mutable_frame();
  SET_CAN_FRAME_ID(*f, CAN_ID_TRACKPROCESSOR);
  CLR_CAN_FRAME_EFF(*f);
  f->can_dlc = 1;
  f->data[0] = TRACKCMD_KEEPALIVE;
  interface_->frame_write_flow()->send(b);
  return exit();
}

}  // namespace bracz_custom
