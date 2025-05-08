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

#pragma once

#include "openlcb/DatagramHandlerDefault.hxx"
#include "custom/SignalPacket.hxx"

namespace bracz_custom {

class SignalServer : public openlcb::DefaultDatagramHandler {
 public:
  enum { DATAGRAM_ID = 0x2F };

  SignalServer(openlcb::DatagramService* if_dg, openlcb::Node* node,
               SignalPacketBaseInterface* signalbus);

  ~SignalServer();

 private:
  Action entry() OVERRIDE;

  Action send_signalpacket();
  Action wait_for_packet_ready();
  Action send_signalpacket_with_ack();
  Action wait_for_packet_response();

  /// Local OpenLCB node to export the datagram service onto.
  openlcb::Node* node_;
  /// Target bus implementation. Packets are sent here.
  SignalPacketBaseInterface* signalbus_;
  /// Holds the packet reference until packet processing is complete.
  BufferPtr<SignalPacket> busPacket_;
  BarrierNotifiable bn_;
};

}  // namespace bracz_custom
