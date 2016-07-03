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
 * \file FindTrainNode.hxx
 *
 * A class that helps finding a train on the network.
 *
 * @author Balazs Racz
 * @date 7 Feb 2016
 */

#ifndef _BRACZ_COMMANDSTATION_FINDTRAINNODE_HXX_
#define _BRACZ_COMMANDSTATION_FINDTRAINNODE_HXX_

#include "commandstation/TrainDb.hxx"
#include "commandstation/AllTrainNodes.hxx"
#include "commandstation/FindProtocolDefs.hxx"
#include "executor/StateFlow.hxx"
#include "nmranet/If.hxx"
#include "nmranet/Node.hxx"

namespace commandstation {

struct FindTrainNodeRequest {
  /** Requests finding a train node with a given address or cab number.
      DccMode should be set to the expected drive type (bits 0..2) if the
      train node needs to be freshly allocated. Set the bit OLCBUSER if the
      expected drive type is not known or don't care. Set the
      DCC_LONG_ADDRESS bit if the inbound address should be interpreted as a
      long address (even if it is a small number). */
  void reset(DccMode type, int address) {
    this->address = address;
    this->type = type;
  }

  DccMode type;
  int address;

  BarrierNotifiable done;

  int resultCode;
  nmranet::NodeID nodeId;
};

class FindTrainNode : public StateFlow<Buffer<FindTrainNodeRequest>, QList<1>> {
 public:
  /// @param node is a local node from which queries can be sent out to the
  /// network. @param train_db is the allocated local train store.
  FindTrainNode(nmranet::Node* node, TrainDb* train_db,
                AllTrainNodes* local_train_nodes)
      : StateFlow<Buffer<FindTrainNodeRequest>, QList<1>>(node->iface()),
        trainDb_(train_db),
        allTrainNodes_(local_train_nodes),
        node_(node) {}

 private:
  Action entry() override {
    return call_immediately(STATE(try_find_in_db));
  }

  /** Looks through all nodes that exist in the train DB and tries to find one
   * with the given legacy address. */
  Action try_find_in_db() {
    for (unsigned train_id = 0; train_id < allTrainNodes_->size(); ++train_id) {
      auto e = allTrainNodes_->get_traindb_entry(train_id);
      if (!e.get() || input()->address != e->get_legacy_address()) {
        continue;
      }
      // TODO: check drive mode.
      return return_ok(allTrainNodes_->get_train_node_id(train_id));
    }
    return call_immediately(STATE(try_find_on_openlcb));
  }

  Action try_find_on_openlcb() {
    return allocate_and_call(node_->iface()->global_message_write_flow(), STATE(send_find_query)); 
  }

  Action send_find_query() {
    auto *b = get_allocation_result(node_->iface()->global_message_write_flow());
    b->set_done(bn_.reset(this));

    uint64_t event = FindProtocolDefs::address_to_query(input()->address, false, OLCBUSER);

    b->data()->reset(nmranet::Defs::MTI_PRODUCER_IDENTIFY, node_->node_id(),
                     nmranet::eventid_to_buffer(event));
    node_->iface()->global_message_write_flow()->send(b);
    /// @todo: wait for incoming evet replies.
    return call_immediately(STATE(allocate_new_train));
  }

  /** Asks the AllTrainNodes structure to allocate a new train node. */
  Action allocate_new_train() {
    nmranet::NodeID n =
        allTrainNodes_->allocate_node(input()->type, input()->address);
    return return_ok(n);
  }

  Action return_ok(nmranet::NodeID nodeId) {
    input()->nodeId = nodeId;
    return return_with_error(0);
  }

  Action return_with_error(int error) {
    if (error) {
      input()->nodeId = 0;
    }
    input()->resultCode = error;
    return_buffer();
    return exit();
  }

  FindTrainNodeRequest* input() { return message()->data(); }

  nmranet::If* iface() {
    // We know that the service pointer is the node's interface from the
    // constructor.
    return static_cast<nmranet::If*>(service());
  }

  BarrierNotifiable bn_;
  TrainDb* trainDb_;
  AllTrainNodes* allTrainNodes_;
  nmranet::Node* node_;
};

}  // namespace commandstation

#endif  // _BRACZ_COMMANDSTATION_FINDTRAINNODE_HXX_
