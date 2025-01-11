/** \copyright
 * Copyright (c) 2014, Balazs Racz
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
 * \file TrainSnipHandler.hxx
 *
 * SNIP handler for TrainDb entries. This works with a single DB entry for a
 * single node, which is typcally used in unit tests or with deadrail
 * locomotives.
 *
 * @author Balazs Racz
 * @date 11 Jan 2025
 */

#ifndef _COMMANDSTATION_TRAINSNIPHANDLER_HXX_
#define _COMMANDSTATION_TRAINSNIPHANDLER_HXX_

#include "commandstation/TrainDb.hxx"
#include "openlcb/SimpleInfoProtocol.hxx"
#include "openlcb/Defs.hxx"

namespace commandstation {

class TrainSnipHandler : public openlcb::IncomingMessageStateFlow {
 public:
  TrainSnipHandler(openlcb::Node* node, TrainDbEntry* entry,
                   openlcb::SimpleInfoFlow* info_flow)
      : IncomingMessageStateFlow(node->iface()),
        node_(node),
        entry_(entry),
        responseFlow_(info_flow) {
    node_->iface()->dispatcher()->register_handler(
        this, openlcb::Defs::MTI_IDENT_INFO_REQUEST, openlcb::Defs::MTI_EXACT);
  }
  ~TrainSnipHandler() {
    if (registered_) {
      iface()->dispatcher()->unregister_handler(
          this, openlcb::Defs::MTI_IDENT_INFO_REQUEST,
          openlcb::Defs::MTI_EXACT);
    }
  }

  void unregister() {
    registered_ = false;
    iface()->dispatcher()->unregister_handler(
        this, openlcb::Defs::MTI_IDENT_INFO_REQUEST, openlcb::Defs::MTI_EXACT);
  }
  
  Action entry() OVERRIDE {
    if (nmsg()->dstNode != node_) return release_and_exit();
    return allocate_and_call(responseFlow_, STATE(send_response_request));
  }

  Action send_response_request() {
    auto* b = get_allocation_result(responseFlow_);
    snipName_ = entry_->get_train_name();
    snipResponse_[6].data = snipName_.c_str();
    b->data()->reset(nmsg(), snipResponse_,
                     openlcb::Defs::MTI_IDENT_INFO_REPLY);
    // We must wait for the data to be sent out because we have a static member
    // that we are changing constantly.
    b->set_done(n_.reset(this));
    responseFlow_->send(b);
    release();
    return wait_and_call(STATE(send_done));
  }

  Action send_done() { return exit(); }

 private:
  bool registered_ = true;
  openlcb::Node* node_;
  TrainDbEntry* entry_;
  openlcb::SimpleInfoFlow* responseFlow_;
  BarrierNotifiable n_;
  string snipName_;
  static openlcb::SimpleInfoDescriptor snipResponse_[];
};

openlcb::SimpleInfoDescriptor TrainSnipHandler::snipResponse_[] =
    {{openlcb::SimpleInfoDescriptor::LITERAL_BYTE, 4, 0, nullptr},
     {openlcb::SimpleInfoDescriptor::C_STRING, 0, 0,
      openlcb::SNIP_STATIC_DATA.manufacturer_name},
     {openlcb::SimpleInfoDescriptor::C_STRING, 41, 0, "Virtual train node"},
     {openlcb::SimpleInfoDescriptor::C_STRING, 0, 0, "n/a"},
     {openlcb::SimpleInfoDescriptor::C_STRING, 0, 0,
      openlcb::SNIP_STATIC_DATA.software_version},
     {openlcb::SimpleInfoDescriptor::LITERAL_BYTE, 2, 0, nullptr},
     {openlcb::SimpleInfoDescriptor::C_STRING, 63, 1, nullptr},
     {openlcb::SimpleInfoDescriptor::C_STRING, 0, 0, "n/a"},
     {openlcb::SimpleInfoDescriptor::END_OF_DATA, 0, 0, 0}};

} // namespace commandstation

#endif  // _COMMANDSTATION_TRAINSNIPHANDLER_HXX_
