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
#include "custom/MCPCanFrameFormat.hxx"


using nmranet::Defs;
using nmranet::DatagramClient;

namespace bracz_custom {

static nmranet::NodeHandle g_host_address{0, 0};
static Notifiable* g_host_address_nn = nullptr;

void TEST_clear_host_address() {
  g_host_address = {0, 0};
  g_host_address_nn = nullptr;
}

HostClient::~HostClient() {}

StateFlowBase::Action HostClient::HostClientHandler::entry() {
  if (size() < 2) {
    return respond_reject(Defs::ERROR_INVALID_ARGS_MESSAGE_TOO_SHORT);
  }
  response_payload_.clear();
  uint8_t cmd = payload()[1];
  switch (cmd) {
    case CMD_PING: {
      response_payload_.reserve(3);
      response_payload_.push_back(HostProtocolDefs::SERVER_DATAGRAM_ID);
      response_payload_.push_back(CMD_PONG);
      response_payload_.push_back(payload()[2] + 1);
      return respond_ok(DatagramClient::REPLY_PENDING);
    }
    case CMD_SYNC: {
      g_host_address = message()->data()->src;
      Notifiable* n = nullptr;
      std::swap(n, g_host_address_nn);
      if (n) {
        n->notify();
      }
      return respond_ok(0);
      break;
    }
    case CMD_CAN_PKT: {
      return allocate_and_call(parent_->can_hub1(), STATE(translate_inbound_can));
      /*handle_can_packet_from_host(payload() + 2, size() - 2);
        return respond_ok(0);*/
      break;
    }
  }  // switch
  return respond_reject(Defs::ERROR_UNIMPLEMENTED_SUBCMD);
}

StateFlowBase::Action HostClient::HostClientHandler::translate_inbound_can() {
  auto* b = get_allocation_result(parent_->can_hub1());
  struct can_frame* f = b->data()->mutable_frame();
  memset(f, 0, sizeof(*f));
  b->data()->skipMember_ = HostClient::instance()->can1_bridge_port();
  //message()->data()->payload[0] = size();
  mcp_to_frame(payload() + 2, f);
  parent_->can_hub1()->send(b);
  return respond_ok(0);
}

StateFlowBase::Action HostClient::HostClientHandler::ok_response_sent() {
  if (!response_payload_.size()) {
    return release_and_exit();
  }
  return allocate_and_call(STATE(dg_client_ready),
                           dg_service()->client_allocator());
}

StateFlowBase::Action HostClient::HostClientHandler::dg_client_ready() {
  dg_client_ = full_allocation_result(dg_service()->client_allocator());
  return allocate_and_call(
      dg_service()->interface()->addressed_message_write_flow(),
      STATE(response_buf_ready));
}

StateFlowBase::Action HostClient::HostClientHandler::response_buf_ready() {
  auto* b = get_allocation_result(
      dg_service()->interface()->addressed_message_write_flow());
  b->data()->reset(Defs::MTI_DATAGRAM, message()->data()->dst->node_id(),
                   message()->data()->src, std::move(response_payload_));
  release();
  b->set_done(n_.reset(this));
  dg_client_->write_datagram(b);
  return wait_and_call(STATE(response_send_complete));
}

StateFlowBase::Action HostClient::HostClientHandler::response_send_complete() {
  dg_service()->client_allocator()->typed_insert(dg_client_);
  dg_client_ = nullptr;
  response_payload_.clear();
  return exit();
}

StateFlowBase::Action HostClient::HostPacketBridge::format_send() {
  auto* b = get_allocation_result(parent()->send_client());
  const struct can_frame& f = message()->data()->frame();
  uint32_t id_masked_eff = GET_CAN_FRAME_ID_EFF(f) & ~1;
  if (!IS_CAN_FRAME_EFF(f) || id_masked_eff == 0x0c000380 ||
      (id_masked_eff == 0x08000900 && f.can_dlc == 3 && f.data[0] == 4)) {
    // Rudimentary filter to remove noise from keepalive and dcc packets.
    return release_and_exit();
  }

  b->data()->resize(f.can_dlc + 5 + 2);
  frame_to_mcp(f, (uint8_t*) &b->data()->data()[2]);
  b->data()->at(0) = HostProtocolDefs::SERVER_DATAGRAM_ID;
  b->data()->at(1) = CMD_CAN_PKT;
  parent()->send_client()->send(b);
  return release_and_exit();
}

HostClient::HostClientSend::HostClientSend(HostClient* parent) : HubPort(parent) {} 
HostClient::HostClientSend::~HostClientSend() {}

void HostClient::log_output(char* buf, int size) {
  if (size <= 0) return;
  auto* b = send_client()->alloc();
  b->data()->reserve(size + 2);
  b->data()->push_back(HostProtocolDefs::SERVER_DATAGRAM_ID);
  b->data()->push_back(CMD_VCOM1);
  b->data()->append(buf, size);
  send_client()->send(b);
}

StateFlowBase::Action HostClient::HostClientSend::entry() {
  // If the log master has not checked in yet, we wait.
  if (g_host_address == nmranet::NodeHandle{0, 0}) {
    g_host_address_nn = this;
    return wait();
  }
  return allocate_and_call(STATE(dg_client_ready),
                           dg_service()->client_allocator());
}

StateFlowBase::Action HostClient::HostClientSend::dg_client_ready() {
  dg_client_ = full_allocation_result(dg_service()->client_allocator());
  return allocate_and_call(
      dg_service()->interface()->addressed_message_write_flow(),
      STATE(response_buf_ready));
}

StateFlowBase::Action HostClient::HostClientSend::response_buf_ready() {
  auto* b = get_allocation_result(
      dg_service()->interface()->addressed_message_write_flow());
  b->data()->reset(Defs::MTI_DATAGRAM, parent()->node()->node_id(),
                   g_host_address,
                   std::move(static_cast<string&>(*message()->data())));
  release();
  b->set_done(n_.reset(this));
  dg_client_->write_datagram(b);
  return wait_and_call(STATE(response_send_complete));
}

StateFlowBase::Action HostClient::HostClientSend::response_send_complete() {
  dg_service()->client_allocator()->typed_insert(dg_client_);
  dg_client_ = nullptr;
  return exit();
}

}  // namespce bracz_custom
