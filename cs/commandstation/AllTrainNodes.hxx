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

#ifndef _BRACZ_COMMANDSTATION_ALLTRAINNODES_HXX_
#define _BRACZ_COMMANDSTATION_ALLTRAINNODES_HXX_

#include <memory>
#include <vector>

#include "nmranet/SimpleInfoProtocol.hxx"
#include "commandstation/TrainDb.hxx"

namespace nmranet {
class Node;
class TrainService;
class TrainImpl;
class MemoryConfigHandler;
}

namespace commandstation {
class AllTrainNodes {
 public:
  AllTrainNodes(TrainDb* db, nmranet::TrainService* traction_service,
                nmranet::SimpleInfoFlow* info_flow,
                nmranet::MemoryConfigHandler* memory_config);
  ~AllTrainNodes();

  // Used for debugging purposes
  nmranet::TrainImpl* get_train_impl(int id);

  /// Returns a traindb entry or nullptr if the id is too high.
  const const_loco_db_t* get_traindb_entry(int id);

  /// Returns a node id or 0 if the id is not known to be a train.
  nmranet::NodeID get_train_node_id(int id);

  // Creates a new train node based on the given address and drive mode.
  nmranet::NodeID allocate_node(DccMode drive_type, int address);

  size_t size() { return trains_.size(); }

 private:
  // ==== Interface for children ====
  struct Impl;

  /// A child can look up if a local node is actually a Train node. If so, the
  /// Impl structure will be returned. If the node is not known (or not a train
  /// node maintained by this object), we return nullptr.
  Impl* find_node(nmranet::Node* node);

  /// Helper function to create lok objects. Adds a new Impl structure to
  /// impl_.
  Impl* create_impl(int train_id, DccMode mode, int address);

  // Externally owned.
  TrainDb* db_;
  nmranet::TrainService* tractionService_;
  nmranet::MemoryConfigHandler* memoryConfigService_;

  /// All train nodes that we know about.
  std::vector<Impl*> trains_;

  // Implementation objects that we carry for various protocols.
  class TrainSnipHandler;
  friend class TrainSnipHandler;
  std::unique_ptr<TrainSnipHandler> snipHandler_;

  class TrainPipHandler;
  friend class TrainPipHandler;
  std::unique_ptr<TrainPipHandler> pipHandler_;
  
  class TrainFDISpace;
  friend class TrainFDISpace;
  std::unique_ptr<TrainFDISpace> fdiSpace_;
};

}  // namespace commandstation

#endif /* _BRACZ_COMMANDSTATION_ALLTRAINNODES_HXX_ */