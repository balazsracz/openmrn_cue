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

#include "commandstation/ProgrammingTrackFrontend.hxx"
#include "commandstation/ProgrammingTrackSpaceConfig.hxx"
#include "utils/ConfigUpdateListener.hxx"
#include "openlcb/MemoryConfig.hxx"

namespace commandstation {

class ProgrammingTrackCVSpace : private openlcb::MemorySpace,
                                private DefaultConfigUpdateListener,
                                public StateFlowBase {
 public:
  ProgrammingTrackCVSpace(openlcb::MemoryConfigHandler* parent,
                          ProgrammingTrackFrontend* frontend,
                          openlcb::Node* node)
      : StateFlowBase(parent->service()),
        parent_(parent),
        frontend_(frontend),
        node_(node) {
    parent_->registry()->insert(node_, SPACE_ID, this);
    memset(&store_, 0, sizeof(store_));
  }

  ~ProgrammingTrackCVSpace() {
    parent_->registry()->erase(node_, SPACE_ID, this);
  }

  /// @returns whether the memory space does not accept writes.
  bool read_only() override
  {
    return false;
  }

  address_t max_address() override {
    return MAX_ADDRESS;
  }

  size_t write(address_t destination, const uint8_t *data, size_t len,
               errorcode_t *error, Notifiable *again) override {
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
    }
    return len;
  }
  
  size_t read(address_t source, uint8_t *dst, size_t len, errorcode_t *error,
              Notifiable *again) override {
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
    return finish_async_state(openlcb::Defs::ERROR_UNIMPLEMENTED);
  }
  
  Action do_cv_read() {
    return invoke_subflow_and_wait(
        frontend_, STATE(cv_read_done),
        ProgrammingTrackFrontendRequest::DIRECT_READ_BYTE, ntohl(store_.cv));
  }

  Action cv_read_done() {
    auto b = get_buffer_deleter(full_allocation_result(frontend_));
    store_.value = htobe32(b->data()->value_);
    return finish_async_state(b->data()->resultCode);
  }

  /// Call this function form async processing states to stop the async state.
  Action finish_async_state(unsigned error_code) {
    asyncError_ = error_code;
    asyncState_ = DONE;
    done_->notify();
    return exit();
  }
  
  static constexpr unsigned SPACE_ID =
      ProgrammingTrackSpaceConfig::group_opts().segment();
  static constexpr unsigned MIN_ADDRESS =
      ProgrammingTrackSpaceConfig::group_opts().get_segment_offset();
  static constexpr unsigned MAX_ADDRESS =
      ProgrammingTrackSpaceConfig::group_opts().get_segment_offset() +
      ProgrammingTrackSpaceConfig::size() - 1;
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
