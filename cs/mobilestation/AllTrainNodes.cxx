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
 * \file AllTrainNodes.hxx
 *
 * A class that instantiates every train node from the TrainDb.
 *
 * @author Balazs Racz
 * @date 20 May 2014
 */

#include "mobilestation/AllTrainNodes.hxx"

#include "dcc/Loco.hxx"
#include "mobilestation/FdiXmlGenerator.hxx"
#include "mobilestation/TrainDb.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/MemoryConfig.hxx"
#include "nmranet/MemoryConfigDefs.hxx"
#include "nmranet/SimpleNodeInfo.hxx"
#include "nmranet/TractionDefs.hxx"
#include "nmranet/TractionTrain.hxx"

namespace mobilestation {

struct AllTrainNodes::Impl {
 public:
  ~Impl() {
    delete eventHandler_;
    delete node_;
    delete train_;
  }
  int id;
  nmranet::SimpleEventHandler* eventHandler_ = nullptr;
  nmranet::Node* node_ = nullptr;
  nmranet::TrainImpl* train_ = nullptr;
};

class AllTrainNodes::TrainSnipHandler
    : public nmranet::IncomingMessageStateFlow {
 public:
  TrainSnipHandler(AllTrainNodes* parent, nmranet::SimpleInfoFlow* info_flow)
      : IncomingMessageStateFlow(parent->tractionService_->iface()),
        parent_(parent),
        responseFlow_(info_flow) {
    iface()->dispatcher()->register_handler(
        this, nmranet::Defs::MTI_IDENT_INFO_REQUEST, nmranet::Defs::MTI_EXACT);
  }
  ~TrainSnipHandler() {
    iface()->dispatcher()->unregister_handler(
        this, nmranet::Defs::MTI_IDENT_INFO_REQUEST, nmranet::Defs::MTI_EXACT);
  }

  Action entry() OVERRIDE {
    if (!nmsg()->dstNode) return release_and_exit();
    // Let's find the train ID.
    impl_ = nullptr;
    for (auto* i : parent_->trains_) {
      if (i->node_ == nmsg()->dstNode) {
        impl_ = i;
        break;
      }
    }
    if (!impl_) return release_and_exit();
    return allocate_and_call(responseFlow_, STATE(send_response_request));
  }

  Action send_response_request() {
    auto* b = get_allocation_result(responseFlow_);
    snipResponse_[6].data = parent_->db_->get_train_name(impl_->id);
    b->data()->reset(nmsg(), snipResponse_,
                     nmranet::Defs::MTI_IDENT_INFO_REPLY);
    // We must wait for the data to be sent out because we have a static member
    // that we are changing constantly.
    b->set_done(n_.reset(this));
    responseFlow_->send(b);
    release();
    return wait_and_call(STATE(send_done));
  }

  Action send_done() { return exit(); }

 private:
  AllTrainNodes* parent_;
  nmranet::SimpleInfoFlow* responseFlow_;
  AllTrainNodes::Impl* impl_;
  BarrierNotifiable n_;
  static nmranet::SimpleInfoDescriptor snipResponse_[];
};

nmranet::SimpleInfoDescriptor AllTrainNodes::TrainSnipHandler::snipResponse_[] =
    {{nmranet::SimpleInfoDescriptor::LITERAL_BYTE, 4, 0, nullptr},
     {nmranet::SimpleInfoDescriptor::C_STRING, 0, 0,
      nmranet::SNIP_STATIC_DATA.manufacturer_name},
     {nmranet::SimpleInfoDescriptor::C_STRING, 41, 0, "Virtual train node"},
     {nmranet::SimpleInfoDescriptor::C_STRING, 0, 0, "n/a"},
     {nmranet::SimpleInfoDescriptor::C_STRING, 0, 0,
      nmranet::SNIP_STATIC_DATA.software_version},
     {nmranet::SimpleInfoDescriptor::LITERAL_BYTE, 2, 0, nullptr},
     {nmranet::SimpleInfoDescriptor::C_STRING, 63, 1, nullptr},
     {nmranet::SimpleInfoDescriptor::C_STRING, 0, 0, "n/a"},
     {nmranet::SimpleInfoDescriptor::END_OF_DATA, 0, 0, 0}};

class AllTrainNodes::TrainFDISpace : nmranet::MemorySpace {
 public:
  TrainFDISpace(AllTrainNodes* parent) : parent_(parent), nodeOffset_(0) {}

  bool set_node(nmranet::Node* node) override {
    if (nodeOffset_ < parent_->trains_.size() &&
        parent_->trains_[nodeOffset_]->node_ == node) {
      // same node.
      return true;
    }
    for (nodeOffset_ = 0; nodeOffset_ < parent_->trains_.size();
         ++nodeOffset_) {
      if (parent_->trains_[nodeOffset_]->node_ == node) {
        // found it.
        reset_file();
        return true;
      }
    }
    return false;
  }

  address_t max_address() override {
    // We don't really know how long this space is 16 MB is an upper bound.
    return 16 << 20;
  }

  size_t read(address_t source, uint8_t* dst, size_t len, errorcode_t* error,
              Notifiable* again) override {
    if (source <= gen_.file_offset()) {
      reset_file();
    }
    ssize_t result = gen_.read(source, dst, len);
    if (result < 0) {
      *error = nmranet::Defs::ERROR_PERMANENT;
      return 0;
    }
    *error = 0;
    return result;
  }

 private:
  void reset_file() {
    gen_.reset_for(const_lokdb + parent_->trains_[nodeOffset_]->id);
  }

  FdiXmlGenerator gen_;
  AllTrainNodes* parent_;
  unsigned nodeOffset_;
};

AllTrainNodes::AllTrainNodes(TrainDb* db,
                             nmranet::TrainService* traction_service,
                             nmranet::SimpleInfoFlow* info_flow,
                             nmranet::MemoryConfigHandler* memory_config)
    : db_(db),
      tractionService_(traction_service),
      memoryConfigService_(memory_config),
      snipHandler_(new TrainSnipHandler(this, info_flow)) {
  for (unsigned train_id = 0; train_id < const_lokdb_size; ++train_id) {
    if (!db->is_train_id_known(train_id)) continue;
    Impl* impl = new Impl;
    impl->id = train_id;
    trains_.push_back(impl);
    unsigned address = db->get_traction_node(train_id) & 0xffffffff;
    unsigned mode = db->get_drive_mode(train_id);
    switch (mode & 7) {
      case MARKLIN_OLD: {
        impl->train_ = new dcc::MMOldTrain(dcc::MMAddress(address));
        break;
      }
      case MARKLIN_NEW:
      case MFX: {
        impl->train_ = new dcc::MMNewTrain(dcc::MMAddress(address));
        break;
      }
      case DCC_28: {
        impl->train_ = new dcc::Dcc28Train(dcc::DccShortAddress(address));
        break;
      }
      case DCC_128: {
        impl->train_ = new dcc::Dcc128Train(dcc::DccShortAddress(address));
        break;
      }
      default:
        DIE("Unhandled train drive mode.");
    }
    if (impl->train_) {
      impl->node_ = new nmranet::TrainNode(traction_service, impl->train_);
      impl->eventHandler_ = new nmranet::FixedEventProducer<
          nmranet::TractionDefs::IS_TRAIN_EVENT>(impl->node_);
    }
  }
  fdiSpace_.reset(new TrainFDISpace(this));
  memoryConfigService_->registry()->insert(nullptr, MemoryConfigDefs::SPACE_FDI, fdiSpace_.get());
}

AllTrainNodes::~AllTrainNodes() {
  for (auto* t : trains_) {
    delete t;
  }
  memoryConfigService_->registry()->remove(nullptr, MemoryConfigDefs::SPACE_FDI, fdiSpace_.get());
}

}  // namespace mobilestation
