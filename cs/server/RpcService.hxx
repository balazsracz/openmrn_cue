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
 * \file RpcService.hxx
 *
 * Implementation flows that parse the RPC requests and serialize the replies.
 *
 * @author Balazs Racz
 * @date 4 May 2015
 */

#ifndef _SERVER_RPCSERVICE_HXX_
#define _SERVER_RPCSERVICE_HXX_

#include "server/RpcDefs.hxx"
#include "server/PacketStreamSender.hxx"

namespace server {

class RpcService : public Service {
 public:
  RpcService(ExecutorBase* executor, RpcServiceInterface* impl)
    : Service(executor), impl_(impl), parser_(this) {}

  RpcServiceInterface* impl() { return impl_; }

 private:
  class ParserFlow : public PacketFlow {
   public:
    ParserFlow(RpcService* s) : PacketFlow(s) {}

    Action entry() OVERRIDE {
      return allocate_and_call(service()->impl(), STATE(have_buffer));
    }

    Action have_buffer() {
      auto* b = get_allocation_result(service()->impl());
      b->data()->request.ParseFromString(*message()->data());
      service()->impl()->send(b);
      return release_and_exit();
    }

   private:
    RpcService* service() {
      return static_cast<RpcService*>(PacketFlow::service());
    }
  };

  RpcServiceInterface* impl_;
  ParserFlow parser_;
};

}  // namespace server

#endif  // _SERVER_RPCSERVICE_HXX_
