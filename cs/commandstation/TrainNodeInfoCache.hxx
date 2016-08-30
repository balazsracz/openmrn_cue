/** \copyright
 * Copyright (c) 2016, Balazs Racz
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
 * \file TrainNodeInfoCache.hxx
 *
 * Class that helps navigating a sorted list of trains with low RAM overhead.
 *
 * @author Balazs Racz
 * @date 7 Feb 2016
 */

#ifndef _BRACZ_COMMANDSTATION_TRAINNODEINFOCACHE_HXX_
#define _BRACZ_COMMANDSTATION_TRAINNODEINFOCACHE_HXX_

#include <functional>

#include "commandstation/FindTrainNode.hxx"
#include "nmranet/If.hxx"
#include "utils/format_utils.hxx"

namespace commandstation {

struct TrainNodeCacheOutput {
  std::vector<std::string*> entry_names;
};

class TrainNodeInfoCache : public StateFlowBase {
 public:
  TrainNodeInfoCache(nmranet::Node* node, RemoteFindTrainNode* find_client,
                     TrainNodeCacheOutput* output)
      : StateFlowBase(node->iface()->dispatcher()->service()),
        node_(node),
        findClient_(find_client),
        output_(output), pendingSearch_(0) {

    node_->iface()->dispatcher()->register_handler(&snipResponseHandler_, nmranet::Defs::MTI_IDENT_INFO_REPLY, nmranet::Defs::MTI_EXACT);
  }

  ~TrainNodeInfoCache() {
    node_->iface()->dispatcher()->unregister_handler(&snipResponseHandler_, nmranet::Defs::MTI_IDENT_INFO_REPLY, nmranet::Defs::MTI_EXACT);
  }

  /// Resets the parameters of the search and triggers a new search request.
  ///
  /// @param params arguments to the search
  /// @param ui_refresh a repeatable notifiable that will be called every time
  /// that the UI should be refreshed. Will be called on the openlcb
  /// interface's executor.
  ///
  void reset_search(BufferPtr<RemoteFindTrainNodeRequest> params,
                    Notifiable* ui_refresh) {
    searchParams_ = std::move(params);
    minResult_ = kMinNode;
    maxResult_ = kMaxNode;
    topNodeId_ = kMinNode;
    resultOffset_ = 0;
    uiNotifiable_ = ui_refresh;
    invoke_search();
  }

  /// @return the last time (in os_time) that the UI data has changed. Helpfu
  /// in deduping multiple UI refresh notifications.
  long long last_ui_refresh() { return lastOutputRefresh_; }

  ///
  /// @return the number of matches the last search found (unfiltered).
  ///
  unsigned num_results() {
    return numResults_;
  }

  /// @return where we are in the scrolling of results, i.e. the offset of the
  /// first result in the list of results.
  unsigned first_result_offset() {
    return resultOffset_;
  }

  void scroll_down() {
    if (pendingSearch_) {
      ++pendingScroll_;
      return;
    }
    auto it = trainNodes_.lower_bound(topNodeId_);
    if (!try_move_iterator(1, it)) {
      // can't go down at all. Do not change anything.
      return;
    }
    if (!try_move_iterator(kNodesToShow, it)) {
      // not enough results left to fill the page. Do not change anything.
      // TODO: maybe we need to add a pending search here?
      return;
    }
    bool need_refill_cache = !try_move_iterator(kMinimumPrefetchSize, it);
    // Now: we can actually move down.
    it = trainNodes_.lower_bound(topNodeId_);
    ++it;
    topNodeId_ = it->first;
    ++resultOffset_;
    update_ui_output();

    // Let's see if we need to kick off a new search.
    if (need_refill_cache && hasEvictBack_) {
      if (!try_move_iterator(-kMinimumPrefetchSize, it)) {
        LOG(VERBOSE,
            "Tried to paginate forward but cannot find new start position.");
        return;
      }
      minResult_ = it->first;
      maxResult_ = kMaxNode;
      // We invoke search here, which is an asynchronous operation. The
      // expectation is that the caller will refresh their display before we
      // actually get around to killing the state vectors.
      invoke_search();
    }
  }

  void scroll_up() {
    if (pendingSearch_) {
      --pendingScroll_;
      return;
    }
    auto it = trainNodes_.lower_bound(topNodeId_);
    if (!try_move_iterator(-1, it)) {
      // can't go up at all. Do not change anything.
      return;
    }
    topNodeId_ = it->first;
    --resultOffset_;
    update_ui_output();
    bool need_refill_cache = !try_move_iterator(-kMinimumPrefetchSize, it);

    // Let's see if we need to kick off a new search.
    if (need_refill_cache && hasEvictFront_) {
      it = trainNodes_.lower_bound(topNodeId_);
      if (!try_move_iterator(kNodesToShow + kMinimumPrefetchSize - 1, it)) {
        LOG(VERBOSE,
            "Tried to paginate backwards but cannot find new start position.");
        return;
      }
      minResult_ = kMinNode;
      maxResult_ = it->first;
      // We invoke search here, which is an asynchronous operation. The
      // expectation is that the caller will refresh their display before we
      // actually get around to killing the state vectors.
      invoke_search();
    }
  }

 private:
  friend class FindManyTrainTestBase;

  struct TrainNodeInfo {
    TrainNodeInfo() : hasNodeName_(0) {}
    TrainNodeInfo(TrainNodeInfo&& other)
        : name_(std::move(other.name_)), hasNodeName_(other.hasNodeName_) {}
    TrainNodeInfo& operator=(TrainNodeInfo&& other) {
      name_ = std::move(other.name_);
      hasNodeName_ = other.hasNodeName_;
      return *this;
    }
    string name_;
    unsigned hasNodeName_ : 1;
  };

  typedef std::map<nmranet::NodeID, TrainNodeInfo> NodeCacheMap;

  /// How many entries we should keep in our inmemory cache.
  static constexpr unsigned kCacheMaxSize = 16;
  /// Lower bound for all valid node IDs.
  static constexpr nmranet::NodeID kMinNode = 0;
  /// Upper bound for all valid node IDs.
  static constexpr nmranet::NodeID kMaxNode = 0xFFFFFFFFFFFF;
  /// How many entries to put into the output vector. THis should probably be
  /// mvoed to a parameter set by the UI soon.
  static constexpr unsigned kNodesToShow = 3;
  /// How many filled cache entries we should keep ahead and behind before we
  /// redo the search with a different offset.
  static constexpr int kMinimumPrefetchSize = 4;

  void invoke_search() {
    needSearch_ = 1;
    if (is_terminated()) {
      start_flow(STATE(send_search_request));
    }
  }

  Action send_search_request() {
    needSearch_ = 0;
    numResults_ = 0;
    previousCache_ = std::move(trainNodes_);
    trainNodes_.clear();
    output_->entry_names.clear();  // will be refreshed later
    pendingSearch_ = 1;
    hasEvictBack_ = 0;
    hasEvictFront_ = 0;
    return invoke_subflow_and_wait(
        findClient_, STATE(search_done), *searchParams_->data(),
        std::bind(&TrainNodeInfoCache::result_callback, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  Action search_done() {
    // @todo (balazs.racz): do pending scroll actions.
    pendingSearch_ = 0;
    if (needSearch_) {
      return call_immediately(STATE(send_search_request));
    }
    lastOutputRefresh_ = os_get_time_monotonic();
    update_ui_output();
    // Let's see what we got and start kicking off name lookup requests.
    lookupIt_ = trainNodes_.begin();
    return call_immediately(STATE(iter_results));
  }

  Action iter_results() {
    if (lookupIt_ == trainNodes_.end()) {
      return call_immediately(STATE(iter_done));
    }
    if (lookupIt_->second.hasNodeName_) {
      // Nothing to look up.
      ++lookupIt_;
      return again();
    }
    return allocate_and_call(node_->iface()->addressed_message_write_flow(),
                             STATE(send_query));
  }

  Action send_query() {
    auto* b =
        get_allocation_result(node_->iface()->addressed_message_write_flow());
    b->data()->reset(nmranet::Defs::MTI_IDENT_INFO_REQUEST, node_->node_id(),
                     nmranet::NodeHandle(lookupIt_->first), nmranet::EMPTY_PAYLOAD);
    b->set_done(bn_.reset(this));
    node_->iface()->addressed_message_write_flow()->send(b);
    return wait_and_call(STATE(send_query_done));
  }

  Action send_query_done() {
    ++lookupIt_;
    return call_immediately(STATE(iter_results));
  }

  Action iter_done() {
    if (needSearch_) {
      // We got another search request while we were processing the previous
      // one.
      return call_immediately(STATE(send_search_request));
    }
    return exit();
  }

  /// Callback from the train node finder when a node has responded to our
  /// search query. Called on the openlcb executor.
  void result_callback(nmranet::EventState state, nmranet::NodeID node) {
    numResults_++;
    if (trainNodes_.find(node) != trainNodes_.end()) {
      // We already have this node, good.
      return;
    }
    if (node < minResult_) {
      // Outside of the search range -- ignore.
      hasEvictFront_ = 1;
      return;
    }
    if (node > maxResult_) {
      // Outside of the search range -- ignore.
      hasEvictBack_ = 1;
      return;
    }
    auto& node_state = trainNodes_[node];
    // See if we have stored data
    auto it = previousCache_.find(node);
    if (it != previousCache_.end()) {
      LOG(VERBOSE, "Found cached node name for %012" PRIx64, node);
      node_state = std::move(it->second);
      previousCache_.erase(it);
    }

    // Check if we need to evict something from the cache.
    while (trainNodes_.size() > kCacheMaxSize) {
      if (scrolling_down()) {
        // delete from the end
        trainNodes_.erase(--trainNodes_.end());
        hasEvictBack_ = 1;
      } else {
        // delete from the beginning
        trainNodes_.erase(trainNodes_.begin());
        hasEvictFront_ = 1;
      }
    }

    if (node_state.name_.empty()) {
      // Need lookup.
    }
  }

  bool scrolling_down() {
    if (maxResult_ >= kMaxNode) return true;
    if (minResult_ <= kMinNode) return false;
    // both upper and lower bound nontrivial. Dunno.
    return true;
  }

  void handle_snip_response(Buffer<nmranet::NMRAnetMessage>* b) {
    auto bd = get_buffer_deleter(b);
    if (!b->data()->src.id) {
      LOG(VERBOSE, "SNIP response coming in without source node ID");
      return;
    }
    auto it = trainNodes_.find(b->data()->src.id);
    if (it == trainNodes_.end()) {
      LOG(VERBOSE, "SNIP response for unknown node");
      return;
    }
    if (it->second.hasNodeName_) {
      // we already have a name.
      return;
    }
    const auto& payload = b->data()->payload;
    nmranet::SnipDecodedData decoded_data;
    nmranet::decode_snip_response(payload, &decoded_data);
    it->second.hasNodeName_ = 1;
    if (!decoded_data.user_name.empty()) {
      it->second.name_ = std::move(decoded_data.user_name);
    } else if (!decoded_data.user_description.empty()) {
      it->second.name_ = std::move(decoded_data.user_description);
    } else if (!decoded_data.model_name.empty()) {
      it->second.name_ = std::move(decoded_data.model_name);
    } else if (!decoded_data.manufacturer_name.empty()) {
      it->second.name_ = std::move(decoded_data.manufacturer_name);
    } else {
      it->second.hasNodeName_ = 0;
      LOG(VERBOSE, "Could not figure out node name from SNIP response. '%s'",
          payload.c_str());
    }
    if (it->second.hasNodeName_) {
      lastOutputRefresh_ = os_get_time_monotonic();
      uiNotifiable_->notify();
    }
  }

  void update_ui_output() {
    auto it = trainNodes_.lower_bound(topNodeId_);
    output_->entry_names.clear();
    output_->entry_names.reserve(kNodesToShow);
    for (unsigned res = 0; res < kNodesToShow && it != trainNodes_.end();
         ++res, ++it) {
      if (it->second.name_.empty()) {
        // Format the node MAC address.
        uint8_t idbuf[6];
        nmranet::node_id_to_data(it->first, idbuf);  // convert to big-endian
        it->second.name_ = mac_to_string(idbuf);
      }
      output_->entry_names.push_back(&it->second.name_);
    }
  }

  /// Tries to advance the iterator forwards or backwards in the
  /// trainNodeCache_ map.
  ///
  /// @param count how many to move (forward or backwards)
  /// @param it the iterator to move. It will be destructively moved.
  ///
  /// @return true if there were sufficient number of elements to advance,
  /// false if we hit begin() or end() and still had to do some advancing.
  ///
  bool try_move_iterator(int count, NodeCacheMap::iterator& it) {
    while (count > 0 && it != trainNodes_.end()) {
      ++it;
      --count;
    }
    while (count < 0 && it != trainNodes_.begin()) {
      --it;
      ++count;
    }
    return (count == 0);
  }

  nmranet::MessageHandler::GenericHandler snipResponseHandler_{
      this, &TrainNodeInfoCache::handle_snip_response};

  /// Local node. All outgoing traffic will originate from this node.
  nmranet::Node* node_;
  /// Helper class for executing train search commands. Externally owned.
  RemoteFindTrainNode* findClient_;
  /// We will be writing the output (some number of lines of trains) to this
  /// location.
  TrainNodeCacheOutput* output_;
  /// Arguments of the current search.
  BufferPtr<RemoteFindTrainNodeRequest> searchParams_;
  /// Helper notifiable for sending messages' callback.
  BarrierNotifiable bn_;

  /// Constrains the results we accept to the cache.
  nmranet::NodeID minResult_;
  /// Constrains the results we accept to the cache.
  nmranet::NodeID maxResult_;

  /// Node ID of the first visible node in the output.
  nmranet::NodeID topNodeId_;

  /// Number of results we found in the last search.
  uint16_t numResults_{0};
  /// Offset of the topmost result in the result list.
  uint16_t resultOffset_{0};

  /// If a scroll action comes in while we are doing a search, we record the
  /// scroll action and execute it delayed.
  int8_t pendingScroll_{0};

  /// 1 if we need to start another search.
  uint8_t needSearch_ : 1;
  /// 1 if there is something in the SNIP cache refreshed.
  uint8_t newSnipData_ : 1;
  /// 1 if we have a pending search action.
  uint8_t pendingSearch_ : 1;
  uint8_t hasEvictFront_ : 1;
  uint8_t hasEvictBack_ : 1;

  /// Iterator for running through the cache and sending lookup requests.
  NodeCacheMap::iterator lookupIt_;

  /// os_time of when we last changed the output.
  long long lastOutputRefresh_;
  /// A repeatable notifiable that will be called to refresh the UI.
  Notifiable* uiNotifiable_;

  /// Internal data structure about the found nodes.
  NodeCacheMap trainNodes_;
  /// When we start a new search, we pre-populate the trainNodes_ structure
  /// form results of the previous search. This reduces network traffic.
  NodeCacheMap previousCache_;
};
}

#endif  // _BRACZ_COMMANDSTATION_TRAINNODEINFOCACHE_HXX_
