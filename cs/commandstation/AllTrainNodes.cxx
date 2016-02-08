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

#include "commandstation/AllTrainNodes.hxx"

#include "dcc/Loco.hxx"
#include "utils/format_utils.hxx"
#include "commandstation/FdiXmlGenerator.hxx"
#include "commandstation/TrainDb.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/MemoryConfig.hxx"
#include "nmranet/SimpleNodeInfo.hxx"
#include "nmranet/TractionDefs.hxx"
#include "nmranet/TractionTrain.hxx"
#ifndef __FreeRTOS__
#include "nmranet/TractionTestTrain.hxx"
#endif

namespace commandstation {

struct AllTrainNodes::Impl {
 public:
  ~Impl() {
    delete eventHandler_;
    delete node_;
    delete train_;
    delete lokdb_entry_;
  }
  int id;
  nmranet::SimpleEventHandler* eventHandler_ = nullptr;
  nmranet::Node* node_ = nullptr;
  nmranet::TrainImpl* train_ = nullptr;
  // Filled in only for anonymous trains.
  const_loco_db_t* lokdb_entry_ = nullptr;
};

nmranet::TrainImpl* AllTrainNodes::get_train_impl(int id) {
  if (id >= (int)trains_.size()) return nullptr;
  return trains_[id]->train_;
}

AllTrainNodes::Impl* AllTrainNodes::find_node(nmranet::Node* node) {
  for (auto* i : trains_) {
    if (i->node_ == node) {
      return i;
    }
  }
  return nullptr;
}

/// Returns a traindb entry or nullptr if the id is too high.
const const_loco_db_t* AllTrainNodes::get_traindb_entry(int id) {
    if (id >= (int)trains_.size()) return nullptr;
  if (trains_[id]->lokdb_entry_) {
    return trains_[id]->lokdb_entry_;
  }
  if (db_->is_train_id_known(id)) {
    return const_lokdb + id;
  }
  return nullptr;
}

/// Returns a node id or 0 if the id is not known to be a train.
nmranet::NodeID AllTrainNodes::get_train_node_id(int id) {
    if (id >= (int)trains_.size()) return 0;
  if (trains_[id]->node_) {
    return trains_[id]->node_->node_id();
  }
  return 0;
}

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
    // Let's find the train ID.
    impl_ = parent_->find_node(nmsg()->dstNode);
    if (!impl_) return release_and_exit();
    return allocate_and_call(responseFlow_, STATE(send_response_request));
  }

  Action send_response_request() {
    auto* b = get_allocation_result(responseFlow_);
    if (impl_->id >= 0) {
      snipResponse_[6].data = parent_->db_->get_train_name(impl_->id);
    } else {
      snipResponse_[6].data = impl_->lokdb_entry_->name;
    }
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

class AllTrainNodes::TrainPipHandler
    : public nmranet::IncomingMessageStateFlow {
 public:
  TrainPipHandler(AllTrainNodes* parent)
      : IncomingMessageStateFlow(parent->tractionService_->iface()),
        parent_(parent) {
    iface()->dispatcher()->register_handler(
        this, nmranet::Defs::MTI_PROTOCOL_SUPPORT_INQUIRY,
        nmranet::Defs::MTI_EXACT);
  }

  ~TrainPipHandler() {
    iface()->dispatcher()->unregister_handler(
        this, nmranet::Defs::MTI_PROTOCOL_SUPPORT_INQUIRY,
        nmranet::Defs::MTI_EXACT);
  }

 private:
  Action entry() {
    if (parent_->find_node(nmsg()->dstNode) == nullptr) {
      return release_and_exit();
    }

    return allocate_and_call(iface()->addressed_message_write_flow(),
                             STATE(fill_response_buffer));
  }

  Action fill_response_buffer() {
    // Grabs our allocated buffer.
    auto* b = get_allocation_result(iface()->addressed_message_write_flow());
    // Fills in response. We use node_id_to_buffer because that converts a
    // 48-bit value to a big-endian byte string.
    b->data()->reset(nmranet::Defs::MTI_PROTOCOL_SUPPORT_REPLY,
                     nmsg()->dstNode->node_id(), nmsg()->src,
                     nmranet::node_id_to_buffer(pipReply_));

    // Passes the response to the addressed message write flow.
    iface()->addressed_message_write_flow()->send(b);

    return release_and_exit();
  }

  AllTrainNodes* parent_;
  static constexpr uint64_t pipReply_ =
      nmranet::Defs::SIMPLE_PROTOCOL_SUBSET | nmranet::Defs::DATAGRAM |
      nmranet::Defs::MEMORY_CONFIGURATION | nmranet::Defs::EVENT_EXCHANGE |
      nmranet::Defs::SIMPLE_NODE_INFORMATION | nmranet::Defs::TRACTION_CONTROL |
      nmranet::Defs::TRACTION_FDI;
};

class AllTrainNodes::TrainFDISpace : public nmranet::MemorySpace {
 public:
  TrainFDISpace(AllTrainNodes* parent) : parent_(parent) {}

  bool set_node(nmranet::Node* node) override {
    if (impl_ && impl_->node_ == node) {
      // same node.
      return true;
    }
    impl_ = parent_->find_node(node);
    if (impl_ != nullptr) {
      reset_file();
      return true;
    }
    return false;
  }

  address_t max_address() override {
    // We don't really know how long this space is; 16 MB is an upper bound.
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
    if (result == 0) {
      *error = nmranet::MemoryConfigDefs::ERROR_OUT_OF_BOUNDS;
    } else {
      *error = 0;
    }
    return result;
  }

 private:
  void reset_file() {
    if (impl_->id >= 0) {
      memcpy(gen_.entry(), const_lokdb + impl_->id, sizeof(*gen_.entry()));
    } else {
      memcpy(gen_.entry(), impl_->lokdb_entry_, sizeof(*gen_.entry()));
    }
    gen_.reset();
  }

  FdiXmlGenerator gen_;
  AllTrainNodes* parent_;
  // Train object structure.
  Impl* impl_{nullptr};
};

AllTrainNodes::AllTrainNodes(TrainDb* db,
                             nmranet::TrainService* traction_service,
                             nmranet::SimpleInfoFlow* info_flow,
                             nmranet::MemoryConfigHandler* memory_config)
    : db_(db),
      tractionService_(traction_service),
      memoryConfigService_(memory_config),
      snipHandler_(new TrainSnipHandler(this, info_flow)),
      pipHandler_(new TrainPipHandler(this)) {
  for (unsigned train_id = 0; train_id < const_lokdb_size; ++train_id) {
    if (!db->is_train_id_known(train_id)) continue;
    create_impl(train_id, db->get_drive_mode(train_id),
                db->get_legacy_address(train_id));
  }
  fdiSpace_.reset(new TrainFDISpace(this));
  memoryConfigService_->registry()->insert(
      nullptr, nmranet::MemoryConfigDefs::SPACE_FDI, fdiSpace_.get());
}

AllTrainNodes::Impl* AllTrainNodes::create_impl(int train_id, DccMode mode,
                                                int address) {
  Impl* impl = new Impl;
  impl->id = train_id;
  trains_.push_back(impl);
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
      if ((mode & DCC_LONG_ADDRESS) || address >= 128) {
        impl->train_ = new dcc::Dcc28Train(dcc::DccLongAddress(address));
      } else {
        impl->train_ = new dcc::Dcc28Train(dcc::DccShortAddress(address));
      }
      break;
    }
    case DCC_128: {
      if ((mode & DCC_LONG_ADDRESS) || address >= 128) {
        impl->train_ = new dcc::Dcc128Train(dcc::DccLongAddress(address));
      } else {
        impl->train_ = new dcc::Dcc128Train(dcc::DccShortAddress(address));
      }
      break;
    }
#ifndef __FreeRTOS__
    case FAKE_DRIVE: {
      impl->train_ = new nmranet::LoggingTrain(address);
      break;
    }
#endif
    default:
      DIE("Unhandled train drive mode.");
  }
  if (impl->train_) {
    impl->node_ = new nmranet::TrainNode(tractionService_, impl->train_);
    impl->eventHandler_ =
        new nmranet::FixedEventProducer<nmranet::TractionDefs::IS_TRAIN_EVENT>(
            impl->node_);
  }
  return impl;
}

/// TODO(balazs.racz) this should be filled in properly.
static const struct const_loco_db_t anonymous_train_entry = {
    12,
    {
     0, 2, 3, 4, 0xff,
    },
    {
     LIGHT, BEAMER, HORN, ABV, 0xff,
    },
    "",
    MFX};

nmranet::NodeID AllTrainNodes::allocate_node(DccMode drive_type, int address) {
  if ((drive_type & 7) == 0) {
    drive_type = static_cast<DccMode>(drive_type | DCC_28);
  }
  Impl* impl = create_impl(-1, drive_type, address);

  impl->lokdb_entry_ = new const_loco_db_t(anonymous_train_entry);
  const_loco_db_t* entry = impl->lokdb_entry_;
  memset(entry, 0, sizeof(*entry));
  integer_to_buffer(impl->train_->legacy_address(),
                    const_cast<char*>(entry->name));
  *const_cast<uint16_t*>(&entry->address) = address;
  uint8_t* fnmap = const_cast<uint8_t*>(entry->function_mapping);
  uint8_t* fnlab = const_cast<uint8_t*>(entry->function_labels);
  for (int i = 0; i < DCC_MAX_FN - 1; ++i) {
    fnmap[i] = i;
    fnlab[i] = FN_UNKNOWN;
  }
  fnmap[DCC_MAX_FN - 1] = 0xff;
  fnlab[DCC_MAX_FN - 1] = 0xff;
  fnlab[0] = LIGHT;
  fnlab[2] = HORN;
  fnlab[3] = BELL;
  return impl->node_->node_id();
}

AllTrainNodes::~AllTrainNodes() {
  for (auto* t : trains_) {
    delete t;
  }
  memoryConfigService_->registry()->erase(
      nullptr, nmranet::MemoryConfigDefs::SPACE_FDI, fdiSpace_.get());
}

}  // namespace commandstation
