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
 * * \file TrainControlService.cxxtest
 *
 * Implementation of the protocol described in train_control.proto
 *
 * @author Balazs Racz
 * @date 5 May 2015
 */

#include "server/TrainControlService.hxx"

#include <set>
#include <functional>

#include "custom/HostProtocol.hxx"
#include "nmranet/Datagram.hxx"
#include "nmranet/DatagramHandlerDefault.hxx"
#include "src/usb_proto.h"

using nmranet::DatagramService;
using nmranet::DatagramClient;
using nmranet::DefaultDatagramHandler;
using nmranet::Node;
using nmranet::NodeHandle;
using nmranet::Payload;
using bracz_custom::HostProtocolDefs;

typedef Payload Packet;

namespace server {

TrainControlService::TrainControlService(ExecutorBase* e)
    : RpcService(e, create_handler_factory()) {}

TrainControlService::~TrainControlService() {}

class HostServer;
typedef StateFlow<Buffer<string>, QList<1> > PacketQueueFlow;

struct TrainControlService::Impl {
  DatagramService* dg_service() { return dg_service_; }
  Node* node() { return node_; }
  HostServer* datagram_handler() { return datagram_handler_; }
  PacketQueueFlow* host_queue() { return host_queue_; }

  NodeHandle client_dst_;
  DatagramService* dg_service_;
  Node* node_;
  HostServer* datagram_handler_;
  PacketQueueFlow* host_queue_;  // todo: this should be owned i think
};

class HostPacketHandlerInterface {
 public:
  virtual void packet_arrived(Buffer<string>* b) = 0;
};

/** Datagram handler that listens to incoming response datagrams from the
    MCU. */
class HostServer : public DefaultDatagramHandler {
 public:
  HostServer(TrainControlService* service)
      : DefaultDatagramHandler(service->impl()->dg_service()),
        service_(service) {
    dg_service()->registry()->insert(
        impl()->node(), HostProtocolDefs::SERVER_DATAGRAM_ID, this);
  }

  ~HostServer() {
    dg_service()->registry()->erase(impl()->node(),
                                    HostProtocolDefs::SERVER_DATAGRAM_ID, this);
  }

  TrainControlService::Impl* impl() { return service_->impl(); }

  void add_handler(HostPacketHandlerInterface* handler) {
    AtomicHolder l(&lock_);
    packet_handlers_.insert(handler);
  }

  void remove_handler(HostPacketHandlerInterface* handler) {
    AtomicHolder l(&lock_);
    packet_handlers_.erase(handler);
  }

 private:
  Action entry() OVERRIDE {
    Buffer<string>* b;
    pool()->alloc(&b);
b->data()->assign((const char*)(payload() + 1), size() - 1);
    std::set<HostPacketHandlerInterface*> packet_handlers;
    {
      AtomicHolder l(&lock_);
      packet_handlers = packet_handlers_;
    }
    for (auto* h : packet_handlers) {
      h->packet_arrived(b->ref());
    }
    b->unref();
    return respond_ok(0);
  }

  TrainControlService* service_;
  std::set<HostPacketHandlerInterface*> packet_handlers_;
  Atomic lock_;
};

typedef StateFlow<Buffer<string>, QList<1> > PacketQueueFlow;

/** This flow will serialize the datagrams to be sent to the host client,
 *  ensuring that there is only one datagram pending at any time. */
class HostPacketQueue : public PacketQueueFlow {
 public:
  HostPacketQueue(TrainControlService* service) : PacketQueueFlow(service) {}

  TrainControlService* service() {
    return static_cast<TrainControlService*>(PacketQueueFlow::service());
  }

 private:
  TrainControlService::Impl* impl() { return service()->impl(); }
  DatagramService* dg_service() { return impl()->dg_service(); }

  Action entry() OVERRIDE {
    return allocate_and_call(STATE(dg_client_ready),
                             dg_service()->client_allocator());
  }

  Action dg_client_ready() {
    dg_client_ = full_allocation_result(dg_service()->client_allocator());
    return allocate_and_call(
        dg_service()->interface()->addressed_message_write_flow(),
        STATE(buf_ready));
  }

  Action buf_ready() {
    auto* b = get_allocation_result(
        dg_service()->interface()->addressed_message_write_flow());
    b->data()->reset(nmranet::Defs::MTI_DATAGRAM, impl()->node()->node_id(),
                     impl()->client_dst_, Payload());
    b->data()->payload.reserve(1 + message()->data()->size());
    b->data()->payload.push_back(HostProtocolDefs::CLIENT_DATAGRAM_ID);
    b->data()->payload.append(*message()->data());
    release();
    b->set_done(n_.reset(this));
    dg_client_->write_datagram(b);
    return wait_and_call(STATE(send_complete));
  }

  Action send_complete() {
    if ((dg_client_->result() & DatagramClient::RESPONSE_CODE_MASK) !=
                                   DatagramClient::OPERATION_SUCCESS) {
      LOG(ERROR, "Failed to send datagram via host channel. Error 0x%x",
          dg_client_->result());
    }
    dg_service()->client_allocator()->typed_insert(dg_client_);
    dg_client_ = nullptr;
    return exit();
  }

  DatagramClient* dg_client_;
  BarrierNotifiable n_;
};

class PingResponseFn {
 public:
  static const int kAcceptResponseCode = CMD_PONG;
  static void FillResponse(const Packet& packet,
                           TrainControlResponse* response) {
    response->mutable_pong()->set_value(packet[1]);
  }
};

class ServerFlow : public RpcService::ImplFlowBase,
                   private HostPacketHandlerInterface {
 public:
  ServerFlow(TrainControlService* service, Buffer<TinyRpc>* b)
      : ImplFlowBase(service, b) {}

  TrainControlService* service() {
    return static_cast<TrainControlService*>(ImplFlowBase::service());
  }

 private:
  TrainControlService::Impl* impl() { return service()->impl(); }
  DatagramService* dg_service() { return impl()->dg_service(); }

  nmranet::If* interface() { return dg_service()->interface(); }

  void packet_arrived(Buffer<string>* b) OVERRIDE {
    if (packet_filter_(*b->data())) {
      response_packet_ = b;
      service()->impl()->datagram_handler()->remove_handler(this);
      this->notify();
    } else {
      b->unref();
    }
  }

  static bool PacketTypeFilter(uint8_t value, const string& p) {
    return p[0] == value;
  }

  // Class C has to have the following public members:
  //
  //  - staic int kAcceptResponseCode: incoming packet[0] will be checked
  //    against this.
  //
  //  - static void FillResponse(const Packet&, TrainControlResponse*);
  template <class C>
  void add_callback() {
    packet_filter_ =
      std::bind(&ServerFlow::PacketTypeFilter, C::kAcceptResponseCode, std::placeholders::_1);
    fill_response_ = &C::FillResponse;
    response_packet_ = nullptr;
    service()->impl()->datagram_handler()->add_handler(this);
  }

  Action response_arrived() {
    fill_response_(*response_packet_->data(),
                   message()->data()->response.mutable_response());
    return reply();
  }

  void send_packet(string&& payload) {
    auto* b = impl()->host_queue()->alloc();
    b->data()->assign(payload);
    impl()->host_queue()->send(b);
  }

  Action entry() OVERRIDE {
    const TrainControlRequest* request = &message()->data()->request.request();
    //TrainControlResponse* response =
    //    message()->data()->response.mutable_response();
    if (request->has_doping()) {
      add_callback<PingResponseFn>();

      string pkt;
      pkt.push_back(CMD_PING);
      pkt.push_back(request->doping().value() & 255);
      send_packet(std::move(pkt));
      LOG(INFO, "Sent ping packet value=%d", request->doping().value());
      return wait_and_call(STATE(response_arrived));
    } /*else if (request->has_dosendrawcanpacket()) {
      Packet* pkt = new Packet;
      const TrainControlRequest::DoSendRawCanPacket& args =
          request->dosendrawcanpacket();
      pkt->push_back(CMD_CAN_PKT);
      if (args.d_size() > 0) {
        for (int i = 0; i < args.d_size(); ++i) {
          pkt->push_back(args.d(i));
        }
      } else {
        *pkt += args.data();
      }
     LOG(INFO) << "Requested sending CAN packet.";
      if (args.wait()) {
        ADD_CANCALLBACK(ResponseCanPacketFn, *pkt, 0);
        can_io_->SendPacket(pkt);
      } else {
        can_io_->SendPacket(pkt);
        done->Run();
        return;
      }
    } else if (request->has_dosetspeed()) {
      Packet* pkt = new Packet;
      const TrainControlRequest::DoSetSpeed& args = request->dosetspeed();
      pkt->push_back(CMD_CAN_PKT);
      pkt->push_back(0x40);
      pkt->push_back(0x48);
      pkt->push_back((args.id() << 2) + 1);
      pkt->push_back(01);
      if (args.has_speed()) {
        pkt->push_back(3);  // Len;
        pkt->push_back(1);  // Speed;
        pkt->push_back(0);  // Speed;
        // Value.
        int value = args.speed() & 0x7f;
        if (args.dir() < 0) value += 0x80;
        pkt->push_back(value);  // Speed;
        LOG(INFO) << "Setting speed of train " << args.id() << " to " << value;
      } else {
        pkt->push_back(2);  // Len;
        pkt->push_back(1);  // Speed;
        pkt->push_back(0);
        LOG(INFO) << "Requested speed of train " << args.id() << ".";
      }
      ADD_CANCALLBACK(SpeedFn, *pkt, 2);
      can_io_->SendPacket(pkt);
    } else if (request->has_dosetaccessory()) {
      Packet* pkt = new Packet;
      const TrainControlRequest::DoSetAccessory& args =
          request->dosetaccessory();
      pkt->push_back(CMD_CAN_PKT);
      pkt->push_back(0x40);
      pkt->push_back(0x48);
      pkt->push_back((args.train_id() << 2) + 1);
      pkt->push_back(01);
      if (args.has_value()) {
        pkt->push_back(3);  // Len;
        pkt->push_back(args.accessory_id());
        pkt->push_back(0);  // unknown;
        pkt->push_back(args.value());
        LOG(INFO) << "Setting train " << args.train_id() << " accessory "
                  << args.accessory_id() << " to " << args.value();
      } else {
        pkt->push_back(2);  // Len;
        pkt->push_back(args.accessory_id());
        pkt->push_back(0);  // unknown;
        LOG(INFO) << "Getting train " << args.train_id() << " accessory "
                  << args.accessory_id();
      }
      ADD_CANCALLBACK(AccessoryFn, *pkt, 2);
      can_io_->SendPacket(pkt);
    } else if (request->has_dosetemergencystop()) {
      Packet* pkt = new Packet;
      const TrainControlRequest::DoSetEmergencyStop& args =
          request->dosetemergencystop();
      pkt->push_back(CMD_CAN_PKT);
      pkt->push_back(0x40);
      pkt->push_back(0x08);
      pkt->push_back(0x09);
      pkt->push_back(0x01);
      if (args.has_stop()) {
        pkt->push_back(3);  // Len
        pkt->push_back(1);
        pkt->push_back(0);  // unknown;
        pkt->push_back(args.stop() ? 1 : 0);
        LOG(INFO) << "Emergency " << (args.stop() ? "stop." : "start.");
        response->mutable_emergencystop()->set_stop(args.stop());
        done->Run();
      } else {
        pkt->push_back(2);  // Len;
        pkt->push_back(1);
        pkt->push_back(0);  // unknown;
        LOG(INFO) << "Getting emergency stop status.";
        ADD_CANCALLBACK(EmergencyStopFn, *pkt, 2);
      }
      can_io_->SendPacket(pkt);
    } else if (request->has_dogetlokdb()) {
      PopulateLokDb(response->mutable_lokdb());
      done->Run();
    } else if (request->has_dogetlokstate()) {
      LOG(WARNING) << request->DebugString();
      if (request->dogetlokstate().has_id()) {
        PopulateLokState(request->dogetlokstate().id(), response);
      } else {
        PopulateAllLokState(response);
      }
      done->Run();
    } else if (false) {
      // dosetlokstate?
      // doestoploco?
      // getorsetcv?
    } else if (request->has_dowaitforchange()) {
      const TrainControlRequest::DoWaitForChange& args =
          request->dowaitforchange();
      TimestampedState* st;
      TrainControlResponse::WaitForChangeResponse* r =
          response->mutable_waitforchangeresponse();
      if (args.has_id()) {
        r->set_id(args.id());
        st = GetLayoutState()->GetLok(args.id());
      } else {
        st = GetLayoutState();
      }
      if (!st) {
        LOG(WARNING) << "Requested wait for timestamp but nonexistant state.";
        rpc->set_status(RPC::APPLICATION_ERROR);
        rpc->set_error_detail("error: unknown state in wait for change.");
        done->Run();
        return;
      }
      st->AddListener(args.timestamp(),
                      NewCallback(&StateChangeCallback, st, r, done));
                      }*/

    message()->data()->response.set_failed(true);
    message()->data()->response.set_error_detail("unimplemented command");
    return reply();
  }

 private:
  std::function<bool(const string&)> packet_filter_;
  std::function<void(const string&, TrainControlResponse*)> fill_response_;
  string request_packet_;
  Buffer<string>* response_packet_;
  DatagramClient* dg_client_;
};
}  // namespace server
