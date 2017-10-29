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
#include <algorithm>

#include "commandstation/FindTrainNode.hxx"
#include "openlcb/If.hxx"
#include "utils/format_utils.hxx"

namespace commandstation {

struct TrainNodeCacheOutput {
  std::vector<std::string*> entry_names;
};

class TrainNodeInfoCache : public StateFlowBase {
 public:
  TrainNodeInfoCache(openlcb::Node* node, RemoteFindTrainNode* find_client,
                     TrainNodeCacheOutput* output)
      : StateFlowBase(node->iface()->dispatcher()->service()),
        node_(node),
        findClient_(find_client),
        output_(output),
        enablePartialScroll_(0),
        nodesToShow_(kNodesToShowDefault),
        cacheMaxSize_(kCacheMaxSizeDefault),
        scrollPrefetchSize_(kScrollPrefetchSizeDefault)
  {

    node_->iface()->dispatcher()->register_handler(&snipResponseHandler_, openlcb::Defs::MTI_IDENT_INFO_REPLY, openlcb::Defs::MTI_EXACT);
  }

  ~TrainNodeInfoCache() {
    node_->iface()->dispatcher()->unregister_handler(&snipResponseHandler_, openlcb::Defs::MTI_IDENT_INFO_REPLY, openlcb::Defs::MTI_EXACT);
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
    targetNodeId_ = kMinNode;
    resultsBeforeTarget_ = 0;
    resultsAfterTarget_ = nodesToShow_ - 1;
    uiNotifiable_ = ui_refresh;
    // @TODO we need to clear stuff here.
    previousCache_ = std::move(trainNodes_.nodes_);
    trainNodes_.reset();
    
    invoke_search();
  }

  /// Prevents further searches going out by forgetting about past
  /// reset_search() calls.
  void cancel() {
    needSearch_ = 0;
  }
  
  /// @return the last time (in os_time) that the UI data has changed. Helpful
  /// in deduping multiple UI refresh notifications.
  long long last_ui_refresh() { return lastOutputRefresh_; }

  ///
  /// @return the number of matches the last search found (unfiltered).
  ///
  unsigned num_results() {
    return trainNodes_.num_results();
  }

  /// @return where we are in the scrolling of results, i.e. the offset of the
  /// first result in the list of results.
  unsigned first_result_offset() {
    return trainNodes_.resultOffset_;
  }

  /// Retrieves the Node ID of a the result at a given offset.
  /// @param offset is an index into the output array (i.e. counting form
  /// first_result_offset). Returns 0 on error.
  openlcb::NodeID get_result_id(unsigned offset) {
    auto it = trainNodes_.nodes_.lower_bound(topNodeId_);
    if (!try_move_iterator(offset, it)) {
      LOG(VERBOSE, "Requested nonexistant result offset %u", offset);
      return 0; // invalid node ID; could not find result.
    }
    if (offset < output_->entry_names.size() && 
        &it->second->name_ == output_->entry_names[offset]) {
      // We are sure we have the right train.
      return it->first;
    }
    LOG(INFO, "Requested a train which does not seem to match the result array.");
    return 0;
  }

  /// Moves the window of displayed entries one down.
  /// @return true if something changed and the display should be redrawn.
  bool scroll_down() {
    auto it = trainNodes_.nodes_.lower_bound(topNodeId_);
    if (!try_move_iterator(1, it)) {
      // can't go down at all. Do not change anything.
      return false;
    }
    auto id = it->first;
    if (!try_move_iterator(enablePartialScroll_ ? 1 : nodesToShow_, it)) {
      // not enough results left to fill the page. Do not change anything.
      // TODO: maybe we need to add a pending search here?
      return false;
    }
    scroll_to(id, 0, nodesToShow_ - 1);
    return true;
  }

  /// Moves the window of displayed entries one up.
  /// @return true if something changed and the display should be redrawn.
  bool scroll_up() {
    auto it = trainNodes_.nodes_.lower_bound(topNodeId_);
    if (!try_move_iterator(-1, it)) {
      // can't go up at all. Do not change anything.
      return false;
    }
    // New scrolling implementation in compatibility mode.
    scroll_to(it->first, 0, nodesToShow_ - 1);
    return true;
  }

  /// Result set missing results on the top due to clipping. There are more
  /// results available there that will be coming asynchronously.
  static constexpr unsigned FLAGS_CLIPPED_AT_TOP = 1;
  /// Result set missing results on the bottom due to clipping. There are more
  /// results available there that will be coming asynchronously.
  static constexpr unsigned FLAGS_CLIPPED_AT_BOTTOM = 2;
  /// We will kick off an asynchronous search to refill the cache. This is
  /// probably not super important for the client.
  static constexpr unsigned FLAGS_NEED_REFILL_CACHE = 4;
  /// The given node ID was not found in the result set.
  static constexpr unsigned FLAGS_TARGET_NOT_FOUND = 8;
  
  /// Fills in the output results array with a search to a given target to a
  /// given position in the page.
  /// @param target_node is the node ID of the node we're seeking to
  /// @param num_before up to this many results should be populated before the
  /// target.
  /// @param num_after up to this many results should be populated after the
  /// target.
  /// @return a bitmask of the FLAGS values.
  unsigned scroll_to(openlcb::NodeID target_node, uint16_t num_before,
                 uint16_t num_after) {
    targetNodeId_ = target_node;
    resultsBeforeTarget_ = num_before;
    resultsAfterTarget_ = num_after;
    unsigned flags = 0;
    openlcb::NodeID refill_min_node = kMinNode;
    openlcb::NodeID refill_max_node = kMaxNode;

    update_ui_output(flags, refill_min_node, refill_max_node);

    if ((flags & FLAGS_NEED_REFILL_CACHE) && !pendingSearch_) {
      minResult_ = refill_min_node;
      maxResult_ = refill_max_node;
      /// @todo maybe we have a race condition here, when there is a pending
      /// search; we will not modify the min/max and somehow assume that the
      /// results of the pending search will be okay. How do we distinguish the
      /// case when the pending search needs to be repeated?
      
      // We invoke search here, which is an asynchronous operation. The
      // expectation is that the caller will refresh their display before we
      // actually get around to killing the state vectors.
      invoke_paginate();
    }
    return flags;
  }
  
  /// @param sz is the number of nodes to keep in the cache maximum
  /// @param prefetch is the number of nodes to keep behind the visible screen
  /// when prefetching.
  void set_cache_max_size(uint16_t sz, uint16_t prefetch) {
    cacheMaxSize_ = sz;
    scrollPrefetchSize_ = prefetch;
  }

  /// @param lines is the number of lines on the screen.
  void set_nodes_to_show(uint16_t lines) {
    nodesToShow_ = lines;
  }

  /// @param line is true if scrolling should go to the last line, false if it
  /// should go to the last page.
  void set_scroll_to_last_line(bool line) {
    enablePartialScroll_ = line ? 1 : 0;
  }

  /// Lower bound for all valid node IDs.
  static constexpr openlcb::NodeID kMinNode = 0;
  /// Upper bound for all valid node IDs.
  static constexpr openlcb::NodeID kMaxNode = 0xFFFFFFFFFFFF;

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

  typedef std::map<openlcb::NodeID, std::shared_ptr<TrainNodeInfo> > NodeCacheMap;
  //typedef std::map<openlcb::NodeID, TrainNodeInfo> NodeCacheMap;

  struct ResultList {
    NodeCacheMap nodes_;
    /// @return the number of results we found in the last search.
    unsigned num_results() {
      return resultsClippedAtTop_ + resultsClippedAtBottom_ + nodes_.size();
    }
    /// How many results we lost at the top of the scroll due to the restriction
    /// or size clipping.
    uint16_t resultsClippedAtTop_;
    /// How many results we lost at the bottom of the scroll due to the
    /// restriction or size clipping.
    uint16_t resultsClippedAtBottom_;
    /// Offset of the topNodeId_ in the result list, including any topmost
    /// evictions.
    int16_t resultOffset_;

    /// Nodes clipped in the paginated search at the bottom.
    uint16_t newClippedAtTop_;
    /// Nodes clipped in the paginated search at the top.
    uint16_t newClippedAtBottom_;
    
    /// Node ID that was the limit of the previous clipping region (exclusive:
    /// this ID is not clipped).
    openlcb::NodeID previousMinNode_;
    /// Node ID that was the limit of the previous clipping region. (exclusive:
    /// this ID is not clipped).
    openlcb::NodeID previousMaxNode_;
    
    void reset() {
      nodes_.clear();
      resultsClippedAtTop_ = 0;
      resultsClippedAtBottom_ = 0;
      resultOffset_ = -1;
      newClippedAtBottom_ = 0;
      newClippedAtTop_ = 0;
      previousMaxNode_ = kMaxNode;
      previousMinNode_ = kMinNode;
    }

    void finalize_search() {
      resultsClippedAtBottom_ = newClippedAtBottom_;
      resultsClippedAtTop_ = newClippedAtTop_;
    }
    
    void clear_to_paginate() {
      newClippedAtBottom_ = 0;
      newClippedAtTop_ = 0;
      previousMaxNode_ = max_node();
      previousMinNode_ = min_node();
    }
    
    /// @return the smallest node ID that we store in the nodes_ cache, or
    /// kMinNode if the cache is empty.
    openlcb::NodeID min_node() const {
      if (nodes_.empty()) return kMinNode;
      return nodes_.begin()->first;
    }

    /// @return the largest node ID that we store in the nodes_ cache or
    /// kMaxNode if the cache is empty.
    openlcb::NodeID max_node() const {
      if (nodes_.empty()) return kMaxNode;
      return (--nodes_.end())->first;
    }
  };
  
  /// How many entries we should keep in our inmemory cache.
  static constexpr unsigned kCacheMaxSizeDefault = 48;
  /// How many entries to put into the output vector.
  static constexpr unsigned kNodesToShowDefault = 3;
  /// How many filled cache entries we should keep ahead and behind before we
  /// redo the search with a different offset.
  static constexpr int kScrollPrefetchSizeDefault = 16;

  void invoke_search() {
    needSearch_ = 1;
    pendingSearch_ = 1;
    if (is_terminated()) {
      start_flow(STATE(send_search_request));
    }
  }

  void invoke_paginate() {
    trainNodes_.finalize_search();
    trainNodes_.clear_to_paginate();
    invoke_search();
  }
  
  Action send_search_request() {
    needSearch_ = 0;
    resultSetChanged_ = 0;
    searchParams_->data()->resultCode = RemoteFindTrainNodeRequest::PERSISTENT_REQUEST;
    return invoke_subflow_and_wait(
        findClient_, STATE(search_done), *searchParams_->data(),
        std::bind(&TrainNodeInfoCache::result_callback, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  Action search_done() {
    // @TODO: consider moving the train nodes clipping offsets to the final
    // offsets values.
    
    // Find selection offset.
    find_selection_offset(&trainNodes_);
    
    // let's render the output first with whatever data we have.
    unsigned flags;
    openlcb::NodeID m1, m2;
    update_ui_output(flags, m1, m2);
    notify_ui();

    if (needSearch_) {
      return call_immediately(STATE(send_search_request));
    }
    pendingSearch_ = 0;
    return call_immediately(STATE(do_iter));
  }

  Action do_iter() {
    resultSetChanged_ = 0;
    // Let's see what we got and start kicking off name lookup requests.
    lookupIt_ = kMinNode;
    return call_immediately(STATE(iter_results));
  }

  Action iter_results() {
    auto it = trainNodes_.nodes_.lower_bound(lookupIt_);
    while (it != trainNodes_.nodes_.end()) {
      if (it->second->hasNodeName_) {
        // Nothing to look up.
        ++it;
        continue;
      } else {
        lookupIt_ = it->first;
        return allocate_and_call(node_->iface()->addressed_message_write_flow(),
                                 STATE(send_query));
      }
    }
    return call_immediately(STATE(iter_done));
  }

  Action send_query() {
    auto* b =
        get_allocation_result(node_->iface()->addressed_message_write_flow());
    b->data()->reset(openlcb::Defs::MTI_IDENT_INFO_REQUEST, node_->node_id(),
                     openlcb::NodeHandle(lookupIt_), openlcb::EMPTY_PAYLOAD);
    b->set_done(bn_.reset(this));
    node_->iface()->addressed_message_write_flow()->send(b);
    return wait_and_call(STATE(send_query_done));
  }

  Action send_query_done() {
    ++lookupIt_;
    return call_immediately(STATE(iter_results));
  }

  Action iter_done() {
    LOG(INFO, "iter done");
    find_selection_offset(&trainNodes_);
    unsigned flags;
    openlcb::NodeID m1, m2;
    update_ui_output(flags, m1, m2);
    notify_ui();
    if (needSearch_) {
      // We got another search request while we were processing the previous
      // one.
      return call_immediately(STATE(send_search_request));
    }
    if (resultSetChanged_) {
      return call_immediately(STATE(do_iter));
    }
    return exit();
  }

  void find_selection_offset(ResultList* list) {
    int ofs = list->resultsClippedAtTop_;
    for (auto it = list->nodes_.begin(); it != list->nodes_.end() && it->first < topNodeId_; ++it, ++ofs);
    list->resultOffset_ = ofs;
  }
  
  /// Adds a new search result to a result list.
  /// @param list is the resultset to add to
  /// @param node isthe new node id
  /// @param forced_new if true, increments the result count when the new node
  /// is clipped, if false, only increments result count if node is not
  /// clipped.
  void add_to_list(ResultList* list, openlcb::NodeID node) {
    if (list->nodes_.find(node) != list->nodes_.end()) {
      // We already have this node, good.
      return;
    }
    bool add_node = false;
    // Everything that's within the existing range of the map<> is always
    // added.
    add_node |= (node >= list->min_node() && node <= list->max_node());
    // We also add everything that's within the requested range.
    add_node |= (node >= minResult_ && node <= maxResult_);
    if (add_node) {
      auto& node_state = list->nodes_[node];
      // See if we have stored data
      auto it = previousCache_.find(node);
      if (it != previousCache_.end()) {
        LOG(VERBOSE, "Found cached node name for %012" PRIx64, node);
        node_state = it->second;
      } else {
        node_state.reset(new TrainNodeInfo);
      }
      if (node > list->previousMaxNode_) {
        list->resultsClippedAtBottom_--;
      }
      if (node < list->previousMinNode_) {
        list->resultsClippedAtTop_--;
        if (list->resultOffset_ > 0) {
          --list->resultOffset_;
        }
      } else if (node < topNodeId_) {
        // new node that arrived just now
        if (list->resultOffset_ > 0) {
          ++list->resultOffset_;
        }
      }
    } else {
      // Did not add node to cache. Let's see why.
      if (node < minResult_ && node < list->previousMinNode_) {
        list->newClippedAtTop_++;
      }
      if (node > maxResult_ && node > list->previousMaxNode_) {
        list->newClippedAtBottom_++;
      }
    }
    /*
    if (node < minResult_) {
      if (!forced_new) return;
      // Outside of the search range -- ignore.
      list->numResults_++;
      list->resultsClippedAtTop_++;
      if (list->resultOffset_ >= 0) {
        ++list->resultOffset_;
      }
      return;
    }
    if (node > maxResult_) {
      if (!forced_new) return;
      // Outside of the search range -- ignore.
      list->numResults_++;
      list->resultsClippedAtBottom_++;
      return;
      }*/

    // Check if we need to evict something from the cache.
    while (list->nodes_.size() > cacheMaxSize_) {
      bool clip_at_end = scrolling_down();
      //bool clip_eviction = clip_at_end;
      if (list->min_node() < minResult_) clip_at_end = false;
      else if (list->max_node() > maxResult_) clip_at_end = true;
      if (clip_at_end) {
        // delete from the end
        list->nodes_.erase(--list->nodes_.end());
        list->resultsClippedAtBottom_++;
        //if (clip_eviction == clip_at_end) {
          list->newClippedAtBottom_++;
          //}
      } else {
        // delete from the beginning
        list->nodes_.erase(list->nodes_.begin());
        list->resultsClippedAtTop_++;
        //if (clip_eviction == clip_at_end) {
          list->newClippedAtTop_++;
          //}
        if (list->resultOffset_ >= 0) {
          ++list->resultOffset_;
        }
      }
    }
  }

  
  /// Callback from the train node finder when a node has responded to our
  /// search query. Called on the openlcb executor.
  void result_callback(openlcb::EventState state, openlcb::NodeID node) {
    if (!node) return; // some error occured.
    add_to_list(&trainNodes_, node);
    resultSetChanged_ = 1;

    if (is_terminated()) {
      LOG(INFO, "restarting flow for additional results");
      start_flow(STATE(do_iter));
    }
  }

  /// @return our guess whether we did the new search request because we are
  /// scrolling down.
  bool scrolling_down() {
    if (maxResult_ >= kMaxNode) return true;
    if (minResult_ <= kMinNode) return false;
    // both upper and lower bound nontrivial. Dunno.
    return true;
  }

  /// Callback when a SNIP response arrives from the network.
  void handle_snip_response(Buffer<openlcb::GenMessage>* b) {
    LOG(INFO, "Snip response for %04x%08x",
        openlcb::node_high(b->data()->src.id),
        openlcb::node_low(b->data()->src.id));
    auto bd = get_buffer_deleter(b);
    if (!b->data()->src.id) {
      LOG(INFO, "SNIP response coming in without source node ID");
      return;
    }
    auto it = trainNodes_.nodes_.find(b->data()->src.id);
    if (it == trainNodes_.nodes_.end()) {
      LOG(INFO, "SNIP response for unknown node");
      return;
    }
    if (it->second->hasNodeName_) {
      // we already have a name.
      return;
    }
    const auto& payload = b->data()->payload;
    openlcb::SnipDecodedData decoded_data;
    openlcb::decode_snip_response(payload, &decoded_data);
    it->second->hasNodeName_ = 1;
    if (!decoded_data.user_name.empty()) {
      it->second->name_ = std::move(decoded_data.user_name);
    } else if (!decoded_data.user_description.empty()) {
      it->second->name_ = std::move(decoded_data.user_description);
    } else if (!decoded_data.model_name.empty()) {
      it->second->name_ = std::move(decoded_data.model_name);
    } else if (!decoded_data.manufacturer_name.empty()) {
      it->second->name_ = std::move(decoded_data.manufacturer_name);
    } else {
      it->second->hasNodeName_ = 0;
      LOG(VERBOSE, "Could not figure out node name from SNIP response. '%s'",
          payload.c_str());
    }
    if (it->second->hasNodeName_) {
      if (std::find(output_->entry_names.begin(), output_->entry_names.end(),
                    &it->second->name_) != output_->entry_names.end()) {
        notify_ui();
      }
    }
  }

  /// Formats a given iterator into the resultset of the 
  void add_node_to_resultset(const NodeCacheMap::iterator& it) {
    if (it->second->name_.empty()) {
      // Format the node MAC address.
      uint8_t idbuf[6];
      openlcb::node_id_to_data(it->first, idbuf);  // convert to big-endian
      it->second->name_ = mac_to_string(idbuf);
    }
    output_->entry_names.push_back(&it->second->name_);
  }

  void update_ui_output(unsigned& flags, openlcb::NodeID& refill_min_node, openlcb::NodeID& refill_max_node) {
    flags = 0;
    refill_min_node = kMinNode;
    refill_max_node = kMaxNode;
    LOG(VERBOSE, "tgt = %u", (unsigned)targetNodeId_ & 0xffffu);
    auto it = trainNodes_.nodes_.lower_bound(targetNodeId_);
    if (it == trainNodes_.nodes_.end() || it->first != targetNodeId_) {
      flags |= FLAGS_TARGET_NOT_FOUND;
    }
    LOG(VERBOSE, "it = %u", (unsigned)it->first & 0xffffu);
    auto itt = it;
    // Look around towards the top.
    if (!try_move_iterator(-resultsBeforeTarget_, itt)) {
      // ran out of results on the top.
      if (trainNodes_.resultsClippedAtTop_) {
        // we should have a loading line now.
        flags |= FLAGS_CLIPPED_AT_TOP | FLAGS_NEED_REFILL_CACHE;
      }
    } else {
      // Found enough results upwards. Do we have enough left in the cache
      // though?
      auto itp = itt;
      if (try_move_iterator(-scrollPrefetchSize_, itp)) {
        // if we need to refill for the bottom, we should start from this node.
        refill_min_node = itp->first;
      } else {
        // we didn't find prefetch count of nodes on the top.
        if (trainNodes_.resultsClippedAtTop_) {
          flags |= FLAGS_NEED_REFILL_CACHE;
        }
      }
    }
    if (itt != trainNodes_.nodes_.end()) {
      LOG(VERBOSE, "itt = %u", (unsigned)itt->first & 0xffffu);
      topNodeId_ = itt->first;
    } else {
      LOG(VERBOSE, "itt = empty");
      // the resultset is probably empty
      topNodeId_ = 0;
    }
    // Look around towards the bottom.
    auto itb = it;
    if (!try_move_iterator(+resultsAfterTarget_ + 1, itb)) {
      // ran out of results on the bottom.
      if (trainNodes_.resultsClippedAtBottom_) {
        // we should have a loading line now.
        flags |= FLAGS_CLIPPED_AT_BOTTOM | FLAGS_NEED_REFILL_CACHE;
      }
    } else {
      // Found enough results downwards. Do we have enough left in the cache
      // though?
      auto itp = itb;
      if (try_move_iterator(+scrollPrefetchSize_, itp)) {
        // if we need to refill for the top, we should start from this node.
        refill_max_node = (--itp)->first;
      } else {
        if (trainNodes_.resultsClippedAtBottom_) {
          flags |= FLAGS_NEED_REFILL_CACHE;
        }
      }
    }
    if (itb != trainNodes_.nodes_.end()) {
      LOG(VERBOSE, "itb = %u", (unsigned)itb->first & 0xffffu);
    } else {
      LOG(VERBOSE, "itb = end");
    }
    
    // Updates first_result_offset.
    find_selection_offset(&trainNodes_);
    output_->entry_names.clear();
    while (itt != itb) {
      add_node_to_resultset(itt);
      ++itt;
    }
    /*
    auto it = trainNodes_.nodes_.lower_bound(topNodeId_);
    output_->entry_names.clear();
    output_->entry_names.reserve(nodesToShow_);
    for (unsigned res = 0; res < nodesToShow_ && it != trainNodes_.nodes_.end();
         ++res, ++it) {
      add_node_to_resultset(it);
      }*/
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
    while (count > 0 && it != trainNodes_.nodes_.end()) {
      ++it;
      --count;
    }
    while (count < 0 && it != trainNodes_.nodes_.begin()) {
      --it;
      ++count;
    }
    return (count == 0);
  }

  void notify_ui() {
    lastOutputRefresh_ = os_get_time_monotonic();
    uiNotifiable_->notify();
  }

  openlcb::MessageHandler::GenericHandler snipResponseHandler_{
      this, &TrainNodeInfoCache::handle_snip_response};

  /// Local node. All outgoing traffic will originate from this node.
  openlcb::Node* node_;
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
  openlcb::NodeID minResult_;
  /// Constrains the results we accept to the cache.
  openlcb::NodeID maxResult_;

  /// Node ID of the first visible node in the output.
  openlcb::NodeID topNodeId_{0};

  /// Node ID of the target node in the output.
  openlcb::NodeID targetNodeId_{0};
  
  /// 1 if we need to start another search.
  uint16_t needSearch_ : 1;
  /// 1 if there is something in the SNIP cache refreshed.
  uint16_t newSnipData_ : 1;
  /// 1 if the sorted list of results has changed.
  uint16_t resultSetChanged_ : 1;
  /// 1 if we are waiting for refilling the cache
  uint16_t pendingSearch_ : 1;
  /// 1 if we are scrolling down.
  //uint16_t 
  /// 1 if scrolling down should allow leaving a partial screen output. Allows
  /// scrolling all the way until only one line is visible on the screen.
  uint16_t enablePartialScroll_ : 1;
  /// How many lines are there on the display, i.e. how many entries should we
  /// render into the output_ vector.
  uint16_t nodesToShow_ : 3;
  /// How many entries we keep in memory before we start clipping.
  uint16_t cacheMaxSize_ : 6;
  /// How many entries we should keep ahead or behind when redoing the search
  /// due to scrolling. Ideally about 1/3rd of the cacheMaxSize_ value.
  uint16_t scrollPrefetchSize_ : 6;
  /// How many results should we render before the target node.
  uint16_t resultsBeforeTarget_ : 4;
  /// How many results should we render after the target node.
  uint16_t resultsAfterTarget_ : 4;

  /// A repeatable notifiable that will be called to refresh the UI.
  Notifiable* uiNotifiable_;
  /// Iterator for running through the cache and sending lookup requests.
  openlcb::NodeID lookupIt_;

  /// os_time of when we last changed the output.
  long long lastOutputRefresh_;

  /// The currently displayed search results.
  ResultList trainNodes_;
  /// When we start a new search, we pre-populate the trainNodes_ structure
  /// from results of the previous search. This reduces network traffic.
  NodeCacheMap previousCache_;
};
}

#endif  // _BRACZ_COMMANDSTATION_TRAINNODEINFOCACHE_HXX_
