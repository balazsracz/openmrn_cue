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
 * \file HostProtocol.hxx
 *
 * Encapsulates the host protocol over an OpenLCB bus.
 *
 * @author Balazs Racz
 * @date 3 May 2015
 */

#ifndef _BRACZ_CUSTOM_HOSTPROTOCOL_HXX_
#define _BRACZ_CUSTOM_HOSTPROTOCOL_HXX_

#include "nmranet/DatagramHandlerDefault.hxx"

namespace bracz_custom {

struct HostProtocolDefs {
  enum {
    DATAGRAM_ID = 0xF1,
  };
};

/** A datagram handler that allows transmitting the host protocol packets over
    OpenLCB bus with datagrams. */
class HostClientHandler : public nmranet::DefaultDatagramHandler {
 public:
  HostClientHandler(nmranet::DatagramService* dg_s, nmranet::Node* node)
      : DefaultDatagramHandler(dg_s), node_(node) {
    dg_service()->registry()->insert(node_, HostProtocolDefs::DATAGRAM_ID,
                                     this);
  }

  ~HostClientHandler() {
    dg_service()->registry()->erase(node_, HostProtocolDefs::DATAGRAM_ID, this);
  }


protected:
  Action entry() override;
  Action ok_response_sent() override;

  Action dg_client_ready();
  Action response_buf_ready();
  Action response_send_complete();

 private:
  nmranet::Node* node_;
  nmranet::DatagramClient* dg_client_{nullptr};
  nmranet::DatagramPayload response_payload_;
  BarrierNotifiable n_;
};

}  // namespce bracz_custom

#endif  // _BRACZ_CUSTOM_HOSTPROTOCOL_HXX_
