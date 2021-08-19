/** \copyright
 * Copyright (c) 2018, Balazs Racz
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are strictly prohibited without written consent.
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
 * \file ProgrammingTrackCVSpace.hxx
 *
 * CV space to access the programming track flow.
 *
 * @author Balazs Racz
 * @date 2 June 2018
 */

#ifndef _COMMANDSTATION_PROGRAMMINGTRACKCVSPACE_HXX_
#define _COMMANDSTATION_PROGRAMMINGTRACKCVSPACE_HXX_

#include "commandstation/ProgrammingTrackSpaceConfig.hxx"
#include "commandstation/ProgrammingTrackFrontend.hxx"
#include "utils/ConfigUpdateListener.hxx"
#include "openlcb/MemoryConfig.hxx"
#include "openlcb/TractionDefs.hxx"

namespace commandstation {

class ProgrammingTrackCVSpace : private openlcb::MemorySpace,
                                private DefaultConfigUpdateListener,
                                public StateFlowBase {
 public:
  static constexpr unsigned MAX_CV = 1023;
  
  ProgrammingTrackCVSpace(openlcb::MemoryConfigHandler* parent,
                          ProgrammingTrackFrontend* frontend,
                          openlcb::Node* node)
      : StateFlowBase(parent->service()),
        parent_(parent),
        frontend_(frontend),
        node_(node) {
    parent_->registry()->insert(nullptr, SPACE_ID, this);
    memset(&store_, 0, sizeof(store_));
  }

  ~ProgrammingTrackCVSpace() {
    parent_->registry()->erase(nullptr, SPACE_ID, this);
  }

  /// @returns whether the memory space does not accept writes.
  bool read_only() override
  {
    return false;
  }

  address_t max_address() override {
    return MAX_ADDRESS;
  }

  bool set_node(openlcb::Node *node) override {
    if (!node)
    {
        return false;
    }
    if (node == node_) {
      pomAddressType_ = dcc::TrainAddressType::UNSPECIFIED;
      return true;
    }
    openlcb::NodeID id = node->node_id();
    if (!openlcb::TractionDefs::legacy_address_from_train_node_id(
            id, &pomAddressType_, &pomAddress_)) {
      return false;
    }
    switch (pomAddressType_) {
      case dcc::TrainAddressType::DCC_SHORT_ADDRESS:
      case dcc::TrainAddressType::DCC_LONG_ADDRESS:
        return true;
      default:
        return false;
    }
  }

  size_t write(address_t destination, const uint8_t *data, size_t len,
               errorcode_t *error, Notifiable *again) override {
    if (!precompute_address(&destination, error)) {
      return 0;
    }
    if (destination <= MAX_CV) {
      len = 1;
      store_.cv = htobe32(destination + 1);
      store_.value = htobe32(data[0]);
      return eval_async_state(STATE(do_cv_write), again, error, len);
    }
    if (destination < MIN_ADDRESS || destination > MAX_ADDRESS) {
      *error = openlcb::MemoryConfigDefs::ERROR_OUT_OF_BOUNDS;
      return 0;
    }
    if (destination + len - 1 > MAX_ADDRESS) {
      len = MAX_ADDRESS - destination + 1;
    }
    if ((destination + len) > ((destination & (~3)) + 4)) {
      // Write goes across fields. Trim len.
      len = ((destination & (~3)) + 4) - destination;
    }
    memcpy(((uint8_t*)&store_) + (destination - MIN_ADDRESS), data, len);
    destination &= ~3;
    switch(destination) {
      case cfg.value().offset():
        return eval_async_state(STATE(do_cv_write), again, error, len);
      case cfg.bit_write_value().offset():
        return eval_async_state(STATE(do_bit_write), again, error, len);
      case cfg.advanced().repeat_verify().offset():
        frontend_->set_repeat_verify(be32toh(store_.verify_repeats));
        break;
      case cfg.advanced().repeat_cooldown_reset().offset():
        frontend_->set_repeat_verify_cooldown(
            be32toh(store_.verify_cooldown_repeats));
        break;
    }
    return len;
  }
  
  size_t read(address_t source, uint8_t *dst, size_t len, errorcode_t *error,
              Notifiable *again) override {
    if (!precompute_address(&source, error)) {
      return 0;
    }
    if (source <= MAX_CV) {
      len = 1;
      // saves the stored CV value to the caller buffer in case this is the
      // second call after async done.
      *dst = be32toh(store_.value);
      store_.cv = htobe32(source + 1);
      return eval_async_state(STATE(do_cv_read), again, error, len);
    }
    if (source < MIN_ADDRESS || source > MAX_ADDRESS) {
      *error = openlcb::MemoryConfigDefs::ERROR_OUT_OF_BOUNDS;
      return 0;
    }
    if (source + len - 1 > MAX_ADDRESS) {
      len = MAX_ADDRESS - source + 1;
    }
    memcpy(dst, ((uint8_t*)&store_) + (source - MIN_ADDRESS), len);
    if (len > 4 || (source & 3) || store_.mode == 0) {
      // We do actual operations only if individual fields are requested.
      return len;
    }
    switch(source) {
      case cfg.value().offset():
        return eval_async_state(STATE(do_cv_read), again, error, len);
    }
    return len;
  }

  
 private:
  void factory_reset(int fd) override {}
  UpdateAction apply_configuration(int fd, bool initial_load,
                                   BarrierNotifiable *done) override {
    AutoNotify an(done);
    // We reset the backing store to all zeros, causing the programming track
    // to be disabled, when the user closes the JMRI configuration window. This
    // will avoid reads causing spurious operations when the window is next
    // opened.
    memset(&store_, 0, sizeof(store_));
    return UPDATED;
  }

  /// Helper function to process the memory space address. Fills in
  /// store_.mode.
  /// @param address will be normalized to the 0..1023 CV space if mode
  /// supported
  /// @param error will be filled in if the mode is not supported.
  /// @return false in case of error.
  bool precompute_address(address_t* address, errorcode_t* error) {
    if ((*address >> 24) == (MIN_ADDRESS >> 24)) {
      // The virtual address space.
      return true;
    }
    bool valid_cv = (*address & 0xFFFFFFu) < 1024;
    if (!valid_cv) {
      *error = openlcb::MemoryConfigDefs::ERROR_OUT_OF_BOUNDS;
      return false;
    }
    switch (*address >> 24) {
      case 0x00:
        // Default mode.
        if (pomAddressType_ == dcc::TrainAddressType::DCC_SHORT_ADDRESS ||
            pomAddressType_ == dcc::TrainAddressType::DCC_LONG_ADDRESS) {
          store_.mode = htobe32(ProgrammingTrackSpaceConfig::POM_MODE);
        } else if (pomAddressType_ == dcc::TrainAddressType::UNSPECIFIED) {
          store_.mode = htobe32(ProgrammingTrackSpaceConfig::DIRECT_MODE);
        } else {
          *error = openlcb::Defs::ERROR_INVALID_ARGS;
          return false;
        }
        break;
      case 0x01:
        store_.mode = htobe32(ProgrammingTrackSpaceConfig::DIRECT_MODE);
        break;
      case 0x02:
        if (pomAddressType_ == dcc::TrainAddressType::DCC_SHORT_ADDRESS ||
            pomAddressType_ == dcc::TrainAddressType::DCC_LONG_ADDRESS) {
          store_.mode = htobe32(ProgrammingTrackSpaceConfig::POM_MODE);
        } else {
          *error = openlcb::MemoryConfigDefs::ERROR_OUT_OF_BOUNDS;
          return false;
        }
        break;
      case 0x03:
        store_.mode = htobe32(ProgrammingTrackSpaceConfig::PAGED_MODE);
        break;
      default:
        *error = openlcb::MemoryConfigDefs::ERROR_OUT_OF_BOUNDS;
        return false;
    }
    *address &= 0xFFFFFFu;
    return true;
  }

  /// Helper function for calling async states from write() and read()
  /// commands.
  /// @param start_state is the state flow state to call to startthe async
  /// processing.
  /// @param done is the notifiable to call when async processing is completed
  /// @param error is where to save the async processing error code
  /// @param len is the return value when the read/write completed successfully.
  /// @return what needs to be returned from the read/write virtual function.
  size_t eval_async_state(Callback start_state, Notifiable* done, errorcode_t* error, size_t len) {
    if (asyncState_ == DONE) {
      asyncState_ = IDLE;
      *error = asyncError_;
      return len;
    }
    HASSERT(asyncState_ == IDLE);
    HASSERT(is_terminated());
    start_flow(start_state);
    *error = ERROR_AGAIN;
    done_ = done;
    asyncState_ = PENDING;
    asyncError_ = 0;
    return 0;
  }

  Action do_cv_write() {
    uint32_t mode = be32toh(store_.mode);
    if (mode == ProgrammingTrackSpaceConfig::DIRECT_MODE) {
      update_bits_decomposition();
      return invoke_subflow_and_wait(
          frontend_, STATE(cv_write_done),
          ProgrammingTrackFrontendRequest::DIRECT_WRITE_BYTE,
          be32toh(store_.cv), be32toh(store_.value));
    }
    if (mode == ProgrammingTrackSpaceConfig::PAGED_MODE) {
      update_bits_decomposition();
      return invoke_subflow_and_wait(
          frontend_, STATE(cv_write_done),
          ProgrammingTrackFrontendRequest::PAGED_WRITE_BYTE, be32toh(store_.cv),
          be32toh(store_.value));
    }
    if (mode == ProgrammingTrackSpaceConfig::POM_MODE) {
      if (pomAddressType_ == dcc::TrainAddressType::DCC_SHORT_ADDRESS ||
          pomAddressType_ == dcc::TrainAddressType::DCC_LONG_ADDRESS) {
        update_bits_decomposition();
        return invoke_subflow_and_wait(
            frontend_, STATE(cv_read_done),
            ProgrammingTrackFrontendRequest::POM_WRITE_BYTE, pomAddressType_,
            pomAddress_, be32toh(store_.cv), be32toh(store_.value));
      }
      return finish_async_state(openlcb::Defs::ERROR_INVALID_ARGS);
    }
    return finish_async_state(openlcb::Defs::ERROR_UNIMPLEMENTED);
  }

  Action cv_write_done() {
    auto b = get_buffer_deleter(full_allocation_result(frontend_));
    return finish_async_state(b->data()->resultCode);
  }

  /// Binary logarithm command. Sets *bit to the bit number where 1<<*bit ==
  /// exp. For example for exp==128 input, *bit will be set to 7.
  /// @param exp is 1 to 128
  /// @param bit is an output argument
  /// @return true if there was only one bit set in exp and *bit was written,
  /// false if there was no unique bit to be found.
  bool find_unique_bit(unsigned exp, unsigned* bit) {
    for (unsigned u = 0; u <= 7; ++u) {
      if ((1u<<u) == exp) {
        *bit = u;
        return true;
      }
    }
    return false;
  }

  /// Update the string in store_.bit_value_string after store_.value changed.
  void update_bits_decomposition() {
    char* endp = store_.bit_value_string;
    uint8_t value = be32toh(store_.value);
    if (!value) {
      strcpy(store_.bit_value_string, "none");
      return;
    }
    bool empty = true;
    for (unsigned bit = 0; bit <= 7; ++bit) {
      if (value & (1<<bit)) {
        if (!empty) {
          *endp++ = '+';
        }
        endp = unsigned_integer_to_buffer(1<<bit, endp);
        empty = false;
      }
    }
  }

  Action do_bit_write() {
    unsigned value = be32toh(store_.bit_write_value);
    unsigned bit = 8;
    bool new_value = false;
    if (value >= 100 && value <= 107) {
      bit = value - 100;
      new_value = true;
    } else if (value >= 200 && value <= 207) {
      bit = value - 200;
      new_value = false;
    } else if (value >= 1001 && value <= 1128) {
      if (!find_unique_bit(value - 1000, &bit)) {
        return finish_async_state(openlcb::Defs::ERROR_INVALID_ARGS);
      }
      new_value = true;
    } else if (value >= 2001 && value <= 2128) {
      if (!find_unique_bit(value - 2000, &bit)) {
        return finish_async_state(openlcb::Defs::ERROR_INVALID_ARGS);
      }
      new_value = false;
    } else {
      // Ignore write.
      return finish_async_state(0);
    }
    return invoke_subflow_and_wait(
        frontend_, STATE(cv_write_done),
        ProgrammingTrackFrontendRequest::DIRECT_WRITE_BIT, be32toh(store_.cv),
        bit, new_value);
  }

  Action do_cv_read() {
    uint32_t mode = be32toh(store_.mode);
    if (mode == ProgrammingTrackSpaceConfig::DIRECT_MODE) {
      return invoke_subflow_and_wait(
          frontend_, STATE(cv_read_done),
          ProgrammingTrackFrontendRequest::DIRECT_READ_BYTE,
          be32toh(store_.cv));
    }
    if (mode == ProgrammingTrackSpaceConfig::POM_MODE) {
      if (pomAddressType_ == dcc::TrainAddressType::DCC_SHORT_ADDRESS ||
          pomAddressType_ == dcc::TrainAddressType::DCC_LONG_ADDRESS) {
        return invoke_subflow_and_wait(
            frontend_, STATE(cv_read_done),
            ProgrammingTrackFrontendRequest::POM_READ_BYTE, pomAddressType_,
            pomAddress_, be32toh(store_.cv));
      }
      return finish_async_state(openlcb::Defs::ERROR_INVALID_ARGS);
    }
    return finish_async_state(openlcb::Defs::ERROR_UNIMPLEMENTED);
  }

  Action cv_read_done() {
    auto b = get_buffer_deleter(full_allocation_result(frontend_));
    store_.value = htobe32(b->data()->value_);
    update_bits_decomposition();
    return finish_async_state(b->data()->resultCode);
  }

  /// Call this function form async processing states to stop the async state.
  Action finish_async_state(unsigned error_code) {
    asyncError_ = error_code;
    asyncState_ = DONE;
    done_->notify();
    return exit();
  }

 public:
  static constexpr unsigned SPACE_ID =
      ProgrammingTrackSpaceConfig::group_opts().segment();
  static constexpr unsigned MIN_ADDRESS =
      ProgrammingTrackSpaceConfig::group_opts().get_segment_offset();
  static constexpr unsigned MAX_ADDRESS =
      ProgrammingTrackSpaceConfig::group_opts().get_segment_offset() +
      ProgrammingTrackSpaceConfig::size() - 1;
 private:
  
  static constexpr ProgrammingTrackSpaceConfig cfg{ProgrammingTrackSpaceConfig::group_opts().get_segment_offset()};

  enum AsyncState {
    IDLE = 0,
    PENDING,
    DONE
  };

  /// Where are we with doing asynchronous operations like read/write.
  AsyncState asyncState_{IDLE};
  /// Error return value from async state.
  unsigned asyncError_;
  /// Address type of last addressed node, or UNSPECIFIED if the commandstation
  /// node was last addressed.
  dcc::TrainAddressType pomAddressType_;
  /// DCC address of the last addressed node.
  uint32_t pomAddress_;
  /// Caller to notify when async state completes.
  Notifiable* done_;
  /// RAM backing store for the CDI variables. Note that everything here is
  /// network-endian so reading and writing here needs to be done with endian
  /// conversion routines.
  ProgrammingTrackSpaceConfig::Shadow store_;

  /// Handler to the memory config service
  openlcb::MemoryConfigHandler* parent_;
  /// Frontend to use to execute programming track commands.
  ProgrammingTrackFrontend* frontend_;
  /// Node to which the programming track is attached to.
  openlcb::Node* node_;
  /// Which memory space we exported ourselves.
  uint8_t spaceId_;
};

}  // namespace commandstation

#endif  // _COMMANDSTATION_PROGRAMMINGTRACKCVSPACE_HXX_
