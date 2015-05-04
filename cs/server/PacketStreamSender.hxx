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
 * \file PacketStreamSender.hxx
 *
 * State flow that sends a stream of packets to an fd using length encoding.
 *
 * @author Balazs Racz
 * @date 4 May 2015
 */

#ifndef _SERVER_PACKETSTREAMSENDER_HXX_
#define _SERVER_PACKETSTREAMSENDER_HXX_

#include <arpa/inet.h>
#include <fcntl.h>
#include "executor/StateFlow.hxx"

namespace server {

typedef FlowInterface<Buffer<string> > PacketFlowInterface;
typedef StateFlow<Buffer<string>, QList<1> > PacketFlow;

class PacketStreamSender : public PacketFlow {
 public:
  PacketStreamSender(Service* s, int fd) : PacketFlow(s), fd_(fd) {
    ::fcntl(fd_, F_SETFL, O_NONBLOCK);
  }

 private:
  Action entry() OVERRIDE {
    length_ = htonl(message()->data()->size());
    return this->write_repeated(&selectHelper_, fd_, &length_, 4,
                                STATE(length_done));
  }

  Action length_done() {
    return this->write_repeated(&selectHelper_, fd_, message()->data()->data(),
                                message()->data()->size(), STATE(all_done));
  }

  Action all_done() { return release_and_exit(); }

  int fd_;
  uint32_t length_;
  StateFlowSelectHelper selectHelper_{this};
};

class PacketStreamReceiver : public StateFlowBase {
 public:
  PacketStreamReceiver(Service* s, PacketFlowInterface* handler, int fd)
      : StateFlowBase(s), fd_(fd), handler_(handler) {
    ::fcntl(fd_, F_SETFL, O_NONBLOCK);
    start_flow(STATE(read_length));
  }

  ~PacketStreamReceiver() {
    service()->executor()->sync_run([this]() { shutdown(); });
  }

  void shutdown() { this->service()->executor()->unselect(&selectHelper_); }

 private:
  Action read_length() {
    length_ = 0;
    return read_repeated(&selectHelper_, fd_, &length_, 4, STATE(length_done));
  }

  Action length_done() {
    length_ = ntohl(length_);
    return allocate_and_call(handler_, STATE(alloc_done));
  }

  Action alloc_done() {
    msg_ = get_allocation_result(handler_);
    msg_->data()->resize(length_);
    return read_repeated(&selectHelper_, fd_, &(*msg_->data())[0], length_,
                         STATE(all_done));
  }

  Action all_done() {
    handler_->send(msg_);
    msg_ = nullptr;
    return call_immediately(STATE(read_length));
  }

  int fd_;
  uint32_t length_;
  PacketFlowInterface* handler_;
  PacketFlow::message_type* msg_ = nullptr;
  StateFlowSelectHelper selectHelper_{this};
};
}

#endif  // _SERVER_PACKETSTREAMSENDER_HXX_
