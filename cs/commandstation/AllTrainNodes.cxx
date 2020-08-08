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

#include "commandstation/FdiXmlGenerator.hxx"
#include "commandstation/FindProtocolServer.hxx"
#include "commandstation/TrainDb.hxx"
#include "dcc/Loco.hxx"
#include "openlcb/EventHandlerTemplates.hxx"
#include "openlcb/MemoryConfig.hxx"
#include "openlcb/SimpleNodeInfo.hxx"
#include "openlcb/TractionDefs.hxx"
#include "openlcb/TractionTrain.hxx"
#include "utils/format_utils.hxx"
#ifndef __FreeRTOS__
#include "openlcb/TractionTestTrain.hxx"
#endif

namespace commandstation {

class DccTrainDbEntry : public TrainDbEntry {
 public:
  DccTrainDbEntry(unsigned address, DccMode mode)
      : address_(address), mode_(mode) {}

  /** Retrieves the name of the train. */
  string get_train_name() override {
    return openlcb::TractionDefs::train_node_name_from_legacy(
        dcc_mode_to_address_type(mode_, address_), address_);
  }

  /** Retrieves the legacy address of the train. */
  int get_legacy_address() override { return address_; }

  /** Retrieves the traction drive mode of the train. */
  DccMode get_legacy_drive_mode() override { return mode_; }

  /** Retrieves the label assigned to a given function, or FN_UNUSED if the
      function does not exist. */
  unsigned get_function_label(unsigned fn_id) override {
    if (is_dcc_mode()) {
      switch (fn_id) {
        case 0:
          return LIGHT;
        case 1:
          return BELL;
        case 2:
          return HORN;
        default:
          return FN_UNKNOWN;
      }
    } else {
      switch (fn_id) {
        case 0:
          return LIGHT;
        case 4:
          return ABV;
        default:
          return FN_UNKNOWN;
      }
    }
  }

  /** Returns the largest valid function ID for this train, or -1 if the train
      has no functions. */
  int get_max_fn() override {
    if (is_dcc_mode()) {
      return 8;
    } else {
      if (mode_ & MARKLIN_TWOADDR) {
        return 8;
      } else {
        return 4;
      }
    }
  }

  openlcb::NodeID get_traction_node() override {
    return openlcb::TractionDefs::train_node_id_from_legacy(
        dcc_mode_to_address_type(mode_, address_), address_);
  }

  string identifier() override {
    string ret("dcc/");
    char buf[10];
    integer_to_buffer(mode_, buf);
    ret.append(buf);
    ret.push_back('/');
    integer_to_buffer(address_, buf);
    ret.append(buf);
    return ret;
  }

  void start_read_functions() override { }

 private:
  /// @returns true for DCC, false for Marklin trains.
  bool is_dcc_mode() { return (mode_ & DCC_ANY); }

  uint16_t address_;
  DccMode mode_;
};

struct AllTrainNodes::Impl {
 public:
  ~Impl() {
    delete eventHandler_;
    delete node_;
    delete train_;
  }
  int id;
  openlcb::SimpleEventHandler* eventHandler_ = nullptr;
  openlcb::Node* node_ = nullptr;
  openlcb::TrainImpl* train_ = nullptr;
};

openlcb::TrainImpl* AllTrainNodes::get_train_impl(int id) {
  if (id >= (int)trains_.size()) return nullptr;
  return trains_[id]->train_;
}

AllTrainNodes::Impl* AllTrainNodes::find_node(openlcb::Node* node) {
  for (auto* i : trains_) {
    if (i->node_ == node) {
      return i;
    }
  }
  return nullptr;
}

AllTrainNodes::Impl* AllTrainNodes::find_node(openlcb::NodeID node_id) {
  for (auto* i : trains_) {
    if (i->node_->node_id() == node_id) {
      return i;
    }
  }
  return nullptr;
}

/// Returns a traindb entry or nullptr if the id is too high.
std::shared_ptr<TrainDbEntry> AllTrainNodes::get_traindb_entry(size_t id) {
  return db_->get_entry(id);
}

/// Returns a node id or 0 if the id is not known to be a train.
openlcb::NodeID AllTrainNodes::get_train_node_id(size_t id) {
  if (id >= trains_.size()) return 0;
  if (trains_[id]->node_) {
    return trains_[id]->node_->node_id();
  }
  return 0;
}

class AllTrainNodes::TrainSnipHandler
    : public openlcb::IncomingMessageStateFlow {
 public:
  TrainSnipHandler(AllTrainNodes* parent, openlcb::SimpleInfoFlow* info_flow)
      : IncomingMessageStateFlow(parent->train_service()->iface()),
        parent_(parent),
        responseFlow_(info_flow) {
    iface()->dispatcher()->register_handler(
        this, openlcb::Defs::MTI_IDENT_INFO_REQUEST, openlcb::Defs::MTI_EXACT);
  }
  ~TrainSnipHandler() {
    iface()->dispatcher()->unregister_handler(
        this, openlcb::Defs::MTI_IDENT_INFO_REQUEST, openlcb::Defs::MTI_EXACT);
  }

  Action entry() OVERRIDE {
    // Let's find the train ID.
    impl_ = parent_->find_node(nmsg()->dstNode);
    if (!impl_) return release_and_exit();
    return allocate_and_call(responseFlow_, STATE(send_response_request));
  }

  Action send_response_request() {
    auto* b = get_allocation_result(responseFlow_);
    auto entry = parent_->get_traindb_entry(impl_->id);
    if (entry.get()) {
      snipName_ = entry->get_train_name();
    } else {
      snipName_.clear();
    }
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
  AllTrainNodes* parent_;
  openlcb::SimpleInfoFlow* responseFlow_;
  AllTrainNodes::Impl* impl_;
  BarrierNotifiable n_;
  string snipName_;
  static openlcb::SimpleInfoDescriptor snipResponse_[];
};

openlcb::SimpleInfoDescriptor AllTrainNodes::TrainSnipHandler::snipResponse_[] =
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

class AllTrainNodes::TrainPipHandler
    : public openlcb::IncomingMessageStateFlow {
 public:
  TrainPipHandler(AllTrainNodes* parent)
      : IncomingMessageStateFlow(parent->train_service()->iface()),
        parent_(parent) {
    iface()->dispatcher()->register_handler(
        this, openlcb::Defs::MTI_PROTOCOL_SUPPORT_INQUIRY,
        openlcb::Defs::MTI_EXACT);
  }

  ~TrainPipHandler() {
    iface()->dispatcher()->unregister_handler(
        this, openlcb::Defs::MTI_PROTOCOL_SUPPORT_INQUIRY,
        openlcb::Defs::MTI_EXACT);
  }

 private:
  Action entry() override {
    if (parent_->find_node(nmsg()->dstNode) == nullptr) {
      return release_and_exit();
    }

    return allocate_and_call(iface()->addressed_message_write_flow(),
                             STATE(fill_response_buffer));
  }

  Action fill_response_buffer() {
    // Grabs our allocated buffer.
    auto* b = get_allocation_result(iface()->addressed_message_write_flow());
    auto reply = pipReply_;
    /* All trains now support CDI.
    Impl* impl = parent_->find_node(nmsg()->dstNode);
    if (parent_->db_->is_train_id_known(impl->id) &&
        parent_->db_->get_entry(impl->id)->file_offset() >= 0) {
      reply |= openlcb::Defs::CDI;
      }*/
    // Fills in response. We use node_id_to_buffer because that converts a
    // 48-bit value to a big-endian byte string.
    b->data()->reset(openlcb::Defs::MTI_PROTOCOL_SUPPORT_REPLY,
                     nmsg()->dstNode->node_id(), nmsg()->src,
                     openlcb::node_id_to_buffer(reply));

    // Passes the response to the addressed message write flow.
    iface()->addressed_message_write_flow()->send(b);

    return release_and_exit();
  }

  AllTrainNodes* parent_;
  static constexpr uint64_t pipReply_ =
      openlcb::Defs::SIMPLE_PROTOCOL_SUBSET | openlcb::Defs::DATAGRAM |
      openlcb::Defs::MEMORY_CONFIGURATION | openlcb::Defs::EVENT_EXCHANGE |
      openlcb::Defs::SIMPLE_NODE_INFORMATION | openlcb::Defs::TRACTION_CONTROL |
      openlcb::Defs::TRACTION_FDI | openlcb::Defs::CDI;
};

class AllTrainNodes::TrainFDISpace : public openlcb::MemorySpace {
 public:
  TrainFDISpace(AllTrainNodes* parent) : parent_(parent) {}

  bool set_node(openlcb::Node* node) override {
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
      *error = openlcb::Defs::ERROR_PERMANENT;
      return 0;
    }
    if (result == 0) {
      *error = openlcb::MemoryConfigDefs::ERROR_OUT_OF_BOUNDS;
    } else {
      *error = 0;
    }
    return result;
  }

 private:
  void reset_file() {
    auto e = parent_->get_traindb_entry(impl_->id);
    e->start_read_functions();
    gen_.reset(std::move(e));
  }

  FdiXmlGenerator gen_;
  AllTrainNodes* parent_;
  // Train object structure.
  Impl* impl_{nullptr};
};

class AllTrainNodes::TrainConfigSpace : public openlcb::FileMemorySpace {
 public:
  TrainConfigSpace(int fd, AllTrainNodes* parent, size_t file_end)
      : FileMemorySpace(fd, file_end), parent_(parent) {}

  bool set_node(openlcb::Node* node) override {
    if (impl_ && impl_->node_ == node) {
      // same node.
      return true;
    }
    impl_ = parent_->find_node(node);
    if (impl_ == nullptr) return false;
    auto entry = parent_->db_->get_entry(impl_->id);
    if (!entry) return false;
    int offset = entry->file_offset();
    if (offset < 0) return false;
    offset_ = offset;
    return true;
  }

  size_t read(address_t source, uint8_t* dst, size_t len, errorcode_t* error,
              Notifiable* again) override {
    return FileMemorySpace::read(source + offset_, dst, len, error, again);
  }

  size_t write(address_t destination, const uint8_t* data, size_t len,
               errorcode_t* error, Notifiable* again) override {
    return FileMemorySpace::write(destination + offset_, data, len, error,
                                  again);
  }

 private:
  unsigned offset_;
  AllTrainNodes* parent_;
  // Train object structure.
  Impl* impl_{nullptr};
};

extern const char TRAINCDI_DATA[];
extern const size_t TRAINCDI_SIZE;
extern const char TRAINTMPCDI_DATA[];
extern const size_t TRAINTMPCDI_SIZE;

class AllTrainNodes::TrainCDISpace : public openlcb::MemorySpace {
 public:
  TrainCDISpace(AllTrainNodes* parent)
      : parent_(parent),
        dbCdi_(TRAINCDI_DATA, TRAINCDI_SIZE),
        tmpCdi_(TRAINTMPCDI_DATA, TRAINTMPCDI_SIZE) {}

  bool set_node(openlcb::Node* node) override {
    if (impl_ && impl_->node_ == node) {
      // same node.
      return true;
    }
    impl_ = parent_->find_node(node);
    if (impl_ == nullptr) return false;
    auto entry = parent_->db_->get_entry(impl_->id);
    if (!entry) return false;
    int offset = entry->file_offset();
    if (offset < 0) {
      proxySpace_ = &tmpCdi_;
    } else {
      proxySpace_ = &dbCdi_;
    }
    return true;
  }

  address_t max_address() override { return proxySpace_->max_address(); }

  size_t read(address_t source, uint8_t* dst, size_t len, errorcode_t* error,
              Notifiable* again) override {
    return proxySpace_->read(source, dst, len, error, again);
  }

  AllTrainNodes* parent_;
  // Train object structure.
  Impl* impl_{nullptr};
  openlcb::MemorySpace* proxySpace_;
  openlcb::ReadOnlyMemoryBlock dbCdi_;
  openlcb::ReadOnlyMemoryBlock tmpCdi_;
};

class AllTrainNodes::TrainNodesUpdater : private DefaultConfigUpdateListener {
 public:
  TrainNodesUpdater(AllTrainNodes* parent) : parent_(parent) {}

  // ConfigUpdate interface
  UpdateAction apply_configuration(int fd, bool initial_load,
                                   BarrierNotifiable* done) override {
    AutoNotify n(done);
    size_t file_end = parent_->db_->load_from_file(fd, initial_load);
    parent_->update_config();
    if (initial_load) {
      parent_->configSpace_.reset(new TrainConfigSpace(fd, parent_, file_end));
      parent_->memoryConfigService_->registry()->insert(
          nullptr, openlcb::MemoryConfigDefs::SPACE_CONFIG,
          parent_->configSpace_.get());
    }
    return UPDATED;
  }

  void factory_reset(int fd) override {}

 private:
  AllTrainNodes* parent_;
};

AllTrainNodes::AllTrainNodes(TrainDb* db,
                             openlcb::TrainService* traction_service,
                             openlcb::SimpleInfoFlow* info_flow,
                             openlcb::MemoryConfigHandler* memory_config)
    : AllTrainNodesInterface(traction_service), db_(db),
      memoryConfigService_(memory_config),
      snipHandler_(new TrainSnipHandler(this, info_flow)),
      pipHandler_(new TrainPipHandler(this)) {
  for (unsigned train_id = 0; train_id < const_lokdb_size; ++train_id) {
    if (!db->is_train_id_known(train_id)) continue;
    auto e = db->get_entry(train_id);
    if (!e.get()) continue;
    create_impl(train_id, e->get_legacy_drive_mode(), e->get_legacy_address());
  }
  fdiSpace_.reset(new TrainFDISpace(this));
  memoryConfigService_->registry()->insert(
      nullptr, openlcb::MemoryConfigDefs::SPACE_FDI, fdiSpace_.get());
  if (db_->has_file()) {
    trainUpdater_.reset(new TrainNodesUpdater(this));
  }
  cdiSpace_.reset(new TrainCDISpace(this));
  memoryConfigService_->registry()->insert(
      nullptr, openlcb::MemoryConfigDefs::SPACE_CDI, cdiSpace_.get());
  findProtocolServer_.reset(new FindProtocolServer(this));
}

void AllTrainNodes::update_config() {
  // First delete all implementations of trains that do not exist anymore.
  for (unsigned id = 0; id < trains_.size(); ++id) {
    Impl* impl = trains_[id];
    auto entry = db_->find_entry(impl->node_->node_id(), impl->id);
    if (entry) continue;
    // Delete current node.
    trains_[id] = nullptr;
    impl->node_->iface()->delete_local_node(impl->node_);
    delete impl;
    impl = trains_.back();
    trains_.pop_back();
    if (impl) {
      trains_[id] = impl;
      --id;  // to be incremented by the loop
      continue;
    }  // else we're at the end of the loop anyway
  }
  // Now create new implementations for all new trains.
  for (unsigned train_id = 0; train_id < db_->size(); ++train_id) {
    auto entry = db_->get_entry(train_id);
    if (!entry) continue;
    Impl* impl = find_node(entry->get_traction_node());
    if (impl) {
      impl->id = train_id;
      continue;
    }
    // Add a new entry.
    create_impl(train_id, entry->get_legacy_drive_mode(),
                entry->get_legacy_address());
  }
}

AllTrainNodes::Impl* AllTrainNodes::create_impl(int train_id, DccMode mode,
                                                int address) {
  Impl* impl = new Impl;
  impl->id = train_id;
#ifdef __EMSCRIPTEN__
  switch (mode) {
    case MARKLIN_OLD:
    case MARKLIN_DEFAULT:
    case MARKLIN_NEW:
    case MARKLIN_TWOADDR:
      impl->train_ =
          new openlcb::LoggingTrain(address, dcc::TrainAddressType::MM);
      break;
    case DCC_14:      
    case DCC_14_LONG_ADDRESS:      
    case DCC_28:
    case DCC_28_LONG_ADDRESS:
    case DCC_128:
    case DCC_128_LONG_ADDRESS:
      if ((mode & DCC_LONG_ADDRESS) || address >= 128) {
        impl->train_ =
            new openlcb::LoggingTrain(address, dcc::TrainAddressType::DCC_LONG_ADDRESS);
      } else {
        impl->train_ =
            new openlcb::LoggingTrain(address, dcc::TrainAddressType::DCC_SHORT_ADDRESS);
      }
      break;
    default:
      impl->train_ = new openlcb::LoggingTrain(
          address, dcc::TrainAddressType::DCC_LONG_ADDRESS);
      break;
    };
#else  
  switch (mode) {
    case MARKLIN_OLD: {
      impl->train_ = new dcc::MMOldTrain(dcc::MMAddress(address));
      break;
    }
    case MARKLIN_DEFAULT:
    case MARKLIN_NEW:
      /// @todo (balazs.racz) implement marklin twoaddr train drive mode.
    case MARKLIN_TWOADDR: {
      impl->train_ = new dcc::MMNewTrain(dcc::MMAddress(address));
      break;
    }
      /// @todo (balazs.racz) implement dcc 14 train drive mode.
    case DCC_14:      
    case DCC_14_LONG_ADDRESS:      
    case DCC_28:
    case DCC_28_LONG_ADDRESS: {
      if ((mode & DCC_LONG_ADDRESS) || address >= 128) {
        impl->train_ = new dcc::Dcc28Train(dcc::DccLongAddress(address));
      } else {
        impl->train_ = new dcc::Dcc28Train(dcc::DccShortAddress(address));
      }
      break;
    }
    case DCC_128:
    case DCC_128_LONG_ADDRESS: {
      if ((mode & DCC_LONG_ADDRESS) || address >= 128) {
        impl->train_ = new dcc::Dcc128Train(dcc::DccLongAddress(address));
      } else {
        impl->train_ = new dcc::Dcc128Train(dcc::DccShortAddress(address));
      }
      break;
    }
#ifndef __FreeRTOS__
    case DCCMODE_FAKE_DRIVE: {
      impl->train_ = new openlcb::LoggingTrain(address);
      break;
    }
#endif
    default:
      impl->train_ = nullptr;
      LOG_ERROR("Unhandled train drive mode.");
  }
#endif  
  if (impl->train_) {
    trains_.push_back(impl);
    impl->node_ =
        new openlcb::TrainNodeForProxy(train_service(), impl->train_);
    impl->eventHandler_ =
        new openlcb::FixedEventProducer<openlcb::TractionDefs::IS_TRAIN_EVENT>(
            impl->node_);
    return impl;
  } else {
    delete impl;
    return nullptr;
  }
}

openlcb::NodeID AllTrainNodes::allocate_node(DccMode drive_type, unsigned address) {
  Impl* impl = create_impl(-1, drive_type, address);
  if (!impl) return 0; // failed.
  impl->id = db_->add_dynamic_entry(new DccTrainDbEntry(address, drive_type));
  return impl->node_->node_id();
}

// For testing.
bool AllTrainNodes::find_flow_is_idle() {
  return findProtocolServer_->is_idle();
}

AllTrainNodes::~AllTrainNodes() {
  for (auto* t : trains_) {
    delete t;
  }
  memoryConfigService_->registry()->erase(
      nullptr, openlcb::MemoryConfigDefs::SPACE_FDI, fdiSpace_.get());
}

}  // namespace commandstation
