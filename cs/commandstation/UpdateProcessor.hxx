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
 * \file UpdateProcessor.hxx
 *
 * Control flow central to the command station: it round-robins between
 * refreshing the individual trains, while giving priority to the update
 * packets.
 *
 * @author Balazs Racz
 * @date 13 May 2014
 */

#ifndef _COMMANDSTATION_UPDATEPROCESSOR_HXX_
#define _COMMANDSTATION_UPDATEPROCESSOR_HXX_

#include <vector>
#include <algorithm>

#include "executor/StateFlow.hxx"
#include "dcc/Packet.hxx"
#include "dcc/TrackIf.hxx"
#include "dcc/UpdateLoop.hxx"

namespace dcc {
class PacketSource;
}

namespace commandstation {

/** This state flow is responsible for filling empty packets with useful data
 * from the packet sources. It receives empty packets from the track interface
 * throttler on its input, fills these packets by calling into the packet
 * sources (aka train implementations), and sends the full packets to the track
 * interface for actually rendering them . */
class UpdateProcessor : public StateFlow<Buffer<dcc::Packet>, QList<1> >,
                        private dcc::UpdateLoopBase {
 public:
  UpdateProcessor(Service* service, dcc::TrackIf* track_send);
  ~UpdateProcessor();

  /** Adds a new refresh source to the background refresh packets. */
  bool add_refresh_source(dcc::PacketSource* source,
                          unsigned priority) OVERRIDE {
    AtomicHolder h(this);
    bool ret = true;
    refreshSources_.push_back(source);
    hasRefreshSource_ = 1;
    auto& s = packetSourceStates_[source];
    s.priority_ = priority;
    s.lastPacketTime_ = 0;
    if (priority >= EXCLUSIVE_MIN_PRIORITY) {
      unsigned last_priority = 0;
      if (exclusiveIndex_ != NO_EXCLUSIVE) {
        last_priority =
            packetSourceStates_[refreshSources_[exclusiveIndex_]].priority_;
      }
      if (priority > last_priority) {
        exclusiveIndex_ = refreshSources_.size() - 1;
      } else {
        ret = false;
      }
    } else {
      if (exclusiveIndex_ != NO_EXCLUSIVE) {
        ret = false;
      }
    }
    return ret;
  }
  /** Deletes a packet refresh source. */
  void remove_refresh_source(dcc::PacketSource* source) OVERRIDE {
    AtomicHolder h(this);
    refreshSources_.erase(
        remove(refreshSources_.begin(), refreshSources_.end(), source),
        refreshSources_.end());
    if (refreshSources_.empty()) {
      hasRefreshSource_ = 0;
    }
    packetSourceStates_.erase(source);
    // Recomputes which is the largest priority and whether we have exclusive.
    unsigned max_priority = EXCLUSIVE_MIN_PRIORITY;
    unsigned max_index = NO_EXCLUSIVE;
    for (unsigned i = 0; i < refreshSources_.size(); ++i) {
      const auto& st = packetSourceStates_[refreshSources_[i]];
      if (st.priority_ >= max_priority) {
        max_index = i;
        max_priority = st.priority_;
      }
    }
    exclusiveIndex_ = max_index;
  }

  /** Notifies that a packet source has an urgent packet. */
  void notify_update(dcc::PacketSource* source, unsigned code) OVERRIDE;

  // Entry to the state flow -- when a new packet needs to be sent.
  Action entry() OVERRIDE;

  // Used by unittests to inject a packet processor mock.
  void TEST_set_packet_processor(dcc::TrackIf* track_send) {
    trackSend_ = track_send;
  }

 private:
  struct SourceState {
    /// Stores the last time we sent a packet to a given loco. Suppresses
    /// refresh packet if this time is too recent to avoid confusing DCC
    /// decoders.
    long long lastPacketTime_;
    /// Priority of packet source. This also encodes whether this is an
    /// exclusive packet source.
    unsigned priority_;
  };

  /// @return if we have an exclusive source.
  bool has_exclusive() { return exclusiveIndex_ != NO_EXCLUSIVE; }

  /// Place where we forward the packets filled in.
  dcc::TrackIf* trackSend_;

  /// Holds the list of train nodes that have reported a change. We will always
  /// take a train node from this list first before starting background refresh.
  QList<1> priorityUpdates_;

  /// Packet sources to ask about refreshing data periodically.
  vector<dcc::PacketSource*> refreshSources_;

  /// Stores additional data about each packet source.
  map<dcc::PacketSource*, SourceState> packetSourceStates_;

  /// Which is the next guy on the refresh source list to add.
  unsigned nextRefreshIndex_ : 15;
  /// The highest priority refresh index, if we have a exclusive index.
  unsigned exclusiveIndex_ : 15;
  /// Non-zero if we have any refresh sources.
  unsigned hasRefreshSource_ : 1;

  /// This is stored in the exclusiveIndex_ when we have no exclusive refresh
  /// source.
  static constexpr unsigned NO_EXCLUSIVE = 0x7FF;
};

}  // namespace commandstation

#endif // _COMMANDSTATION_UPDATEPROCESSOR_HXX_
