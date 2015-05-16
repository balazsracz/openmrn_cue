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
 * \file TrainControlService.hxx
 *
 * Implementation of the protocol described in train_control.proto
 *
 * @author Balazs Racz
 * @date 5 May 2015
 */

#ifndef _SERVER_TRAINCONTROLSERVICE_HXX_
#define _SERVER_TRAINCONTROLSERVICE_HXX_

#include "server/RpcService.hxx"
#include "nmranet/Datagram.hxx"

class Clock;

namespace server {

class TrainControlService : public RpcService {
 public:
  TrainControlService(ExecutorBase* e);
  ~TrainControlService();

  class Impl;
  Impl* impl() { return impl_.get(); }

  void initialize(nmranet::DatagramService* dg_service, nmranet::Node* node,
                  nmranet::NodeHandle client, const string& lokdb_ascii,
                  bool query_state);

  // Takes ownership of the injected object.
  void TEST_inject_clock(Clock* clock);

 private:
  // Initializes handler_factory_ and returns the resulting pointer. This is a
  // trick to hide the actual implementation class in the .cc file.
  RpcServiceInterface* create_handler_factory();

  std::unique_ptr<Impl> impl_;
  std::unique_ptr<RpcServiceInterface> handler_factory_;
};

}  // namespace server

#endif  // _SERVER_TRAINCONTROLSERVICE_HXX_
