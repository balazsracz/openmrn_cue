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

#include "nmranet/If.hxx"
#include "commandstation/FindTrainNode.hxx"

namespace commandstation {

struct TrainNodeCacheOutput {
  std::vector<std::string*> entry_names;
};

class TrainNodeInfoCache {
 public:
  TrainNodeInfoCache(nmranet::Node* node, RemoteFindTrainNode* find_client,
                     TrainNodeCacheOutput* output)
      : node_(node), findClient_(find_client), output_(output) {}

  void reset_search(BufferPtr<RemoteFindTrainNodeRequest> params) {
    searchParams_ = std::move(params);
    minResult_ = kMinNode;
    maxResult_ = kMaxNode;
    previousCache_ = std::move(trainNodes_);
    trainNodes_.clear();
    start_search();
  }

 private:
  struct TrainNodeInfo {
    TrainNodeInfo() {}
    TrainNodeInfo(TrainNodeInfo&& other) 
        : name_(std::move(other.name_)) {}
    TrainNodeInfo& operator=(TrainNodeInfo&& other) {
      name_ = std::move(other.name_);
      return *this;
    }
    string name_;
  };

  typedef std::map<nmranet::NodeID, TrainNodeInfo> NodeCacheMap;

  static constexpr unsigned kCacheMaxSize = 16;
  static constexpr nmranet::NodeID kMinNode = 0;
  static constexpr nmranet::NodeID kMaxNode = 0xFFFFFFFFFFFF;

  void start_search() {
    auto* b = findClient_->alloc();
    b->data()->reset(*searchParams_->data(),
                     std::bind(&TrainNodeInfoCache::result_callback, this,
                               std::placeholders::_1, std::placeholders::_2));
  }

  void result_callback(nmranet::EventState state, nmranet::NodeID node) {
    if (trainNodes_.find(node) != trainNodes_.end()) {
      // We already have this node, good.
      return;
    }
    if ((node < minResult_) || (node > maxResult_)) {
      // Outside of the search range -- ignore.
      return;
    }
    auto& node_state = trainNodes_[node];
    // See if we have stored data
    auto it = previousCache_.find(node);
    if (it != previousCache_.end()) {
      node_state = std::move(it->second);
      previousCache_.erase(it);
    }

    // Check if we need to evict something from the cache.
    while (trainNodes_.size() > kCacheMaxSize) {
      if (scrolling_down()) {
        // delete from the end
        trainNodes_.erase(--trainNodes_.end());
      } else {
        // delete from the beginning
        trainNodes_.erase(trainNodes_.begin());
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

  /// Local node. All outgoing traffic will originate from this node.
  nmranet::Node* node_;
  /// Helper class for executing train search commands.
  RemoteFindTrainNode* findClient_;
  /// We will be writing the output (some number of lines of trains) to this
  /// location.
  TrainNodeCacheOutput* output_;
  /// Arguments of the current search.
  BufferPtr<RemoteFindTrainNodeRequest> searchParams_;

  /// Constrains the results we accept to the cache.
  nmranet::NodeID minResult_;
  /// Constrains the results we accept to the cache.
  nmranet::NodeID maxResult_;

  /// Internal data structure about the found nodes.
  NodeCacheMap trainNodes_;
  /// When we start a new search, we pre-populate the trainNodes_ structure
  /// form results of the previous search. This reduces network traffic.
  NodeCacheMap previousCache_;
};
}

#endif  // _BRACZ_COMMANDSTATION_TRAINNODEINFOCACHE_HXX_
