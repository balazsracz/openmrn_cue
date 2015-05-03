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
 * \file HostProtocol.cxx
 *
 * Encapsulates the host protocol over an OpenLCB bus.
 *
 * @author Balazs Racz
 * @date 3 May 2015
 */

#include "custom/HostProtocol.hxx"

#include "custom/HostPacketCanPort.hxx"
#include "src/usb_proto.h"

using nmranet::Defs;
using nmranet::DatagramClient;

namespace bracz_custom {

StateFlowBase::Action HostClientHandler::entry() {
  if (size() < 2) {
    return respond_reject(Defs::ERROR_INVALID_ARGS_MESSAGE_TOO_SHORT);
  }
  response_payload_.clear();
  uint8_t cmd = payload()[1];
  switch(cmd) {
  case CMD_PING: {
    response_payload_.reserve(3);
    response_payload_.push_back(HostProtocolDefs::DATAGRAM_ID);
    response_payload_.push_back(CMD_PONG);
    response_payload_.push_back(payload()[2] + 1);
    return respond_ok(DatagramClient::REPLY_PENDING);
  }
  case CMD_SYNC: {
    break;
  }
  case CMD_CAN_PKT: {
    handle_can_packet_from_host(payload() + 2, size() - 2);
    break;
  }
  } // switch
  return respond_reject(Defs::ERROR_UNIMPLEMENTED_SUBCMD);
}

StateFlowBase::Action HostClientHandler::ok_response_sent() {
  if (!response_payload_.size()) {
    return release_and_exit();
  }
  return allocate_and_call(STATE(dg_client_ready), dg_service()->client_allocator());
}

StateFlowBase::Action HostClientHandler::dg_client_ready() {
  dg_client_ = full_allocation_result(dg_service()->client_allocator());
  return allocate_and_call(dg_service()->interface()->addressed_message_write_flow(), STATE(response_buf_ready));
}

StateFlowBase::Action HostClientHandler::response_buf_ready() {
  auto *b = get_allocation_result(dg_service()->interface()->addressed_message_write_flow());
  b->data()->reset(Defs::MTI_DATAGRAM, message()->data()->dst->node_id(), message()->data()->src, std::move(response_payload_));
  release();
  b->set_done(n_.reset(this));
  dg_client_->write_datagram(b);
  return wait_and_call(STATE(response_send_complete));
}

StateFlowBase::Action HostClientHandler::response_send_complete() {
  dg_service()->client_allocator()->typed_insert(dg_client_);
  dg_client_ = nullptr;
  response_payload_.clear();
  return exit();
}

}  // namespce bracz_custom
