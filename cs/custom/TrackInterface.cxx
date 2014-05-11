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

namespace bracz_custom {

StateFlowBase::Action entry() {
  return allocate_and_call(device_, STATE(fill_packet));
}

StateFlowBase::Action TrackIfSend::fill_packet() {
  auto* b = get_allocation_result(device_);
  b->set_done(message()->new_child());
  auto* f = b->mutable_frame();
  CLR_CAN_FRAME_EFF(*f);
  SET_CAN_FRAME_ID(*f, CAN_ID_TRACKPROCESSOR);
  dcc::Packet* packet = message()->data();
  f->data[0] = packet->header_raw_data;
  f->dlc = packet->dlc + 1;
  HASSERT(f->dlc <= 8);
  memcpy(f->data + 1, packet->payload, packet->dlc);
  device_->send(b);
}

}  // namespace bracz_custom
