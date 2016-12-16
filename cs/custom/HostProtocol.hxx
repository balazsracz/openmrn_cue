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

#include "openlcb/DatagramHandlerDefault.hxx"
#include "utils/Hub.hxx"
#include "utils/Singleton.hxx"
#include "custom/HostLogging.hxx"

namespace bracz_custom {

struct HostProtocolDefs {
  enum {
    CLIENT_DATAGRAM_ID = 0xF1,
    SERVER_DATAGRAM_ID = 0xF2,
  };
};

class HostClientSend;

void TEST_clear_host_address();

class HostClient;

class HostClient : public Service, public Singleton<HostClient> {
 public:
  HostClient(openlcb::DatagramService* dg_service, openlcb::Node* node,
             CanHubFlow* can_hub1)
      : Service(dg_service->executor()),
        dg_service_(dg_service),
        node_(node),
        can_hub1_(can_hub1),
        client_handler_(this),
        bridge_port_(this),
        sender_(this) {}
  ~HostClient();

  openlcb::DatagramService* dg_service() { return dg_service_; }
  openlcb::Node* node() { return node_; }
  CanHubFlow* can_hub1() { return can_hub1_; }

  // These functions can be called from any thread.
  void send_host_log_event(HostLogEvent e) { log_output((char*)&e, 1); }
  void log_output(char* buf, int size);

  HubPort* send_client() { return &sender_; }
  CanHubPortInterface* can1_bridge_port() { return &bridge_port_; }

  /** A datagram handler that allows transmitting the host protocol packets over
      OpenLCB bus with datagrams. */
  class HostClientHandler : public openlcb::DefaultDatagramHandler {
   public:
    HostClientHandler(HostClient* parent)
        : DefaultDatagramHandler(parent->dg_service()), parent_(parent) {
      dg_service()->registry()->insert(parent_->node(), HostProtocolDefs::CLIENT_DATAGRAM_ID,
                                       this);
    }

    ~HostClientHandler() {
      dg_service()->registry()->erase(parent_->node(), HostProtocolDefs::CLIENT_DATAGRAM_ID,
                                      this);
    }

   protected:
    Action entry() override;
    Action ok_response_sent() override;

    Action translate_inbound_can();

    Action dg_client_ready();
    Action response_buf_ready();
    Action response_send_complete();

   private:
    HostClient* parent_;
    openlcb::DatagramClient* dg_client_{nullptr};
    openlcb::DatagramPayload response_payload_;
    BarrierNotifiable n_;
  };

  class HostPacketBridge : public CanHubPort {
  public:
    HostPacketBridge(HostClient* parent) : CanHubPort(parent) {
      device()->register_port(this);
    }

    ~HostPacketBridge() { device()->unregister_port(this); }

    HostClient* parent() { return static_cast<HostClient*>(service()); }

    CanHubFlow* device() { return parent()->can_hub1(); }

    Action entry() OVERRIDE {
      return allocate_and_call(parent()->send_client(), STATE(format_send));
    };

    Action format_send();
  };  // class hostpacketcanbridge

  class HostClientSend : public Singleton<HostClientSend>, public HubPort {
   public:
    HostClientSend(HostClient* parent);
    ~HostClientSend();

   protected:
    openlcb::DatagramService* dg_service() { return parent()->dg_service(); }
    HostClient* parent() { return static_cast<HostClient*>(service()); }

   private:
    Action entry() override;
    Action dg_client_ready();
    Action response_buf_ready();
    Action response_send_complete();

    openlcb::DatagramClient* dg_client_{nullptr};
    BarrierNotifiable n_;
  };  // class hostclientsend

 private:
  openlcb::DatagramService* dg_service_;
  openlcb::Node* node_;
  CanHubFlow* can_hub1_;
  HostClientHandler client_handler_;
  HostPacketBridge bridge_port_;
  HostClientSend sender_;

};  // HostClient

}  // namespce bracz_custom

#endif  // _BRACZ_CUSTOM_HOSTPROTOCOL_HXX_
