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
 * \file SignalServer.hxx
 *
 * Datagram server that allows arbitrary packets to be sent to the signalbus.
 *
 * @author Balazs Racz
 * @date 22 Feb 2015
 */

#include "custom/SignalServer.hxx"
#include "custom/SignalLoop.hxx"
#include "src/base.h"

namespace bracz_custom {

SignalServer::SignalServer(openlcb::DatagramService* if_dg, openlcb::Node* node,
                           SignalPacketBaseInterface* signalbus)
    : DefaultDatagramHandler(if_dg), node_(node), signalbus_(signalbus) {
  dg_service()->registry()->insert(node_, DATAGRAM_ID, this);
}

SignalServer::~SignalServer() {
  dg_service()->registry()->erase(node_, DATAGRAM_ID, this);
}

StateFlowBase::Action SignalServer::entry() {
  if (size() < 2)
    return respond_reject(openlcb::Defs::ERROR_INVALID_ARGS_MESSAGE_TOO_SHORT);
  uint8_t cmd = payload()[1];
  switch (cmd) {
    case CMD_SIGNALPACKET: {
      return allocate_and_call(signalbus_, STATE(send_signalpacket));
    }
    case CMD_SIGNALPACKET_WITH_ACK: {
      if (size() < 4) {
        return respond_reject(
            openlcb::Defs::ERROR_INVALID_ARGS_MESSAGE_TOO_SHORT);
      }
      return allocate_and_call(signalbus_, STATE(send_signalpacket_with_ack));
    }
    case CMD_SIGNAL_PAUSE: {
      Singleton<SignalLoopInterface>::instance()->disable_loop();
      return respond_ok(0);
    }
    case CMD_SIGNAL_RESUME: {
      Singleton<SignalLoopInterface>::instance()->enable_loop();
      return respond_ok(0);
    }
    default:
      return respond_reject(openlcb::Defs::ERROR_UNIMPLEMENTED_SUBCMD);
  }
}

StateFlowBase::Action SignalServer::send_signalpacket() {
  auto* b = get_allocation_result(signalbus_);
  b->data()->payload_.assign((char*)(payload() + 2), size() - 2);
  b->set_done(bn_.reset(this));
  signalbus_->send(b);
  return wait_and_call(STATE(wait_for_packet_ready));
}

StateFlowBase::Action SignalServer::send_signalpacket_with_ack() {
  BufferPtr<SignalPacket> b(get_allocation_result(signalbus_));
  busPacket_.reset(b->ref());
  b->data()->payload_.assign((char*)(payload() + 4), size() - 4);
  uint16_t timeout = openlcb::data_to_error(payload() + 2);
  b->data()->responseTimeoutMsec_ = timeout;
  b->data()->done_.reset(this);
  signalbus_->send(b.release());
  return wait_and_call(STATE(wait_for_packet_response));
}

StateFlowBase::Action SignalServer::wait_for_packet_response() {
  switch (busPacket_->data()->resultCode_) {
    case SignalPacket::RESULT_NOACK:
      return respond_reject(openlcb::Defs::ERROR_OPENLCB_TIMEOUT);
    case SignalPacket::RESULT_ACK:
      return respond_ok(0);
    default:
      return respond_reject(openlcb::Defs::ERROR_TEMPORARY);
  }
}

StateFlowBase::Action SignalServer::wait_for_packet_ready() {
  return respond_ok(0);
}

}  // namespace bracz_custom
