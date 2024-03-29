/** \copyright
 * Copyright (c) 2014-2016, Balazs Racz
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
 * \file FindProtocolDefs.hxx
 *
 * Implementation of the find protocol event handler.
 *
 * @author Balazs Racz
 * @date 18 Feb 2016
 */

#ifndef _COMMANDSTATION_FINDPROTOCOLSERVER_HXX_
#define _COMMANDSTATION_FINDPROTOCOLSERVER_HXX_

#include "commandstation/AllTrainNodesInterface.hxx"
#include "commandstation/FindProtocolDefs.hxx"
#include "openlcb/Defs.hxx"
#include "openlcb/EventHandlerTemplates.hxx"
#include "openlcb/TractionTrain.hxx"

/// A loglevel to output the latency debugging commands.
#define LATENCYDEBUG VERBOSE

namespace commandstation {

class FindProtocolServer : public openlcb::SimpleEventHandler {
 public:
  FindProtocolServer(AllTrainNodesInterface *nodes) : nodes_(nodes) {
    openlcb::EventRegistry::instance()->register_handler(
        EventRegistryEntry(this, FindProtocolDefs::TRAIN_FIND_BASE,
                           USER_ARG_FIND),
        FindProtocolDefs::TRAIN_FIND_MASK);
    openlcb::EventRegistry::instance()->register_handler(
        EventRegistryEntry(this, IS_TRAIN_EVENT, USER_ARG_ISTRAIN), 0);
  }

  ~FindProtocolServer() {
    openlcb::EventRegistry::instance()->unregister_handler(this);
  }

  void handle_identify_global(const EventRegistryEntry &registry_entry,
                              EventReport *event,
                              BarrierNotifiable *done) override {
    AutoNotify an(done);

    if (event && event->dst_node) {
      // Identify addressed
      if (!service()->is_known_train_node(event->dst_node)) {
        return;
      }
      static_assert(((FindProtocolDefs::TRAIN_FIND_BASE >>
                      FindProtocolDefs::TRAIN_FIND_MASK) &
                     1) == 1,
                    "The lowermost bit of the TRAIN_FIND_BASE must be 1 or "
                    "else the event produced range encoding must be updated.");
      if (registry_entry.user_arg == USER_ARG_FIND) {
        event->event_write_helper<1>()->WriteAsync(
            event->dst_node, openlcb::Defs::MTI_PRODUCER_IDENTIFIED_RANGE,
            openlcb::WriteHelper::global(),
            openlcb::eventid_to_buffer(FindProtocolDefs::TRAIN_FIND_BASE),
            done->new_child());
      } else if (registry_entry.user_arg == USER_ARG_ISTRAIN) {
        event->event_write_helper<1>()->WriteAsync(
            event->dst_node, openlcb::Defs::MTI_PRODUCER_IDENTIFIED_UNKNOWN,
            openlcb::WriteHelper::global(),
            openlcb::eventid_to_buffer(IS_TRAIN_EVENT), done->new_child());
      }
    } else {
      // Identify global
      if (pendingGlobalIdentify_ ||
          registry_entry.user_arg == USER_ARG_ISTRAIN) {
        // We have not started processing the global identify yet. Swallow this
        // one. We only need one global identify call, so we skip the ISTRAIN
        // registrations.
        return;
      }
      // We do a synchronous alloc here but there isn't a much better choice.
      auto *b = flow_.alloc();
      // Can't do this -- see handleidentifyproducer.
      // b->set_done(done->new_child());
      pendingGlobalIdentify_ = true;
      b->data()->reset(REQUEST_GLOBAL_IDENTIFY, event->src_node);
      flow_.send(b);
    }
  }

  void handle_identify_producer(const EventRegistryEntry &registry_entry,
                                EventReport *event,
                                BarrierNotifiable *done) override {
    AutoNotify an(done);

    auto *b = flow_.alloc();
    // This would be nice in that we would prevent allocating more buffers
    // while the previous request is serviced. However, thereby we also block
    // the progress on the event handler flow, which will cause deadlocks,
    // since servicing the request involves sending events out that will be
    // looped back.
    //
    // b->set_done(done->new_child());
    b->data()->reset(event->event, event->src_node);
    if (event->event == IS_TRAIN_EVENT) {
      pendingIsTrain_ = true;
    }
    flow_.send(b);
  };

  // For testing.
  bool is_idle() { return flow_.is_waiting(); }

 private:
  enum {
    // Send this in the event_ field if there is a global identify
    // pending. This is not a valid EventID, because the upper byte is 0.
    REQUEST_GLOBAL_IDENTIFY = 0x0001000000000000U,
    IS_TRAIN_EVENT = openlcb::TractionDefs::IS_TRAIN_EVENT,
    USER_ARG_FIND = 1,
    USER_ARG_ISTRAIN = 2,
  };
  struct Request {
    void reset(openlcb::EventId event, openlcb::NodeHandle src) {
      event_ = event;
      src_ = src;
    }
    EventId event_;
    openlcb::NodeHandle src_;
  };

  class FindProtocolFlow : public StateFlow<Buffer<Request>, QList<1> > {
   public:
    using Base = StateFlow<Buffer<Request>, QList<1> >;

    FindProtocolFlow(FindProtocolServer *parent)
        : StateFlow(parent->service()), parent_(parent) {}

    void send(Buffer<Request> *msg, unsigned prio = 0) override {
      if (message() != nullptr &&
          is_followup_request(*message()->data(), *msg->data())) {
        // We are processing a request from the same source node. We cancel
        // iterating on the current request to save time.
        cancelIteration_ = true;
      }
      Base::send(msg, prio);
    }

    /// Checks if a new incoming request should invalidate an existing request
    /// already being processed. This is a heuristic.
    ///
    /// @param current currently processed request.
    /// @param next new incoming request
    ///
    /// @return true if the currently processed request is superseded by the
    /// new request.
    ///
    bool is_followup_request(const Request &current, const Request &next) {
      if (!iface()->matching_node(current.src_, next.src_)) {
        // Different node sending the request.
        return false;
      }
      if ((current.event_ & FindProtocolDefs::ALLOCATE) != 0) {
        // Never cancel an allocate.
        return false;
      }
      if (current.event_ == next.event_) {
        // Same -- restart is a good outcome.
        return true;
      }
      // Not sure how to compare non-find event IDs.
      if (!FindProtocolDefs::is_find_event(current.event_)) {
        return false;
      }
      if (!FindProtocolDefs::is_find_event(next.event_)) {
        return false;
      }
      auto q_current = FindProtocolDefs::get_query_part(current.event_);
      auto q_next = FindProtocolDefs::get_query_part(next.event_);
      if ((q_next >> 4) == (q_current & FindProtocolDefs::QUERY_SHIFTED_MASK)) {
        // Added a digit.
        return true;
      }
      if ((q_current >> 4) == (q_next & FindProtocolDefs::QUERY_SHIFTED_MASK)) {
        // Backspaced.
        return true;
      }
      return false;
    }

    Action entry() override {
      eventId_ = message()->data()->event_;
      cancelIteration_ = false;
      if (eventId_ == REQUEST_GLOBAL_IDENTIFY) {
        isGlobal_ = true;
        if (!parent_->pendingGlobalIdentify_) {
          // Duplicate global identify, or the previous one was already handled.
          return release_and_exit();
        }
        parent_->pendingGlobalIdentify_ = false;
      } else if (eventId_ == IS_TRAIN_EVENT) {
        isGlobal_ = true;
        if (!parent_->pendingIsTrain_) {
          // Duplicate is_train, or the previous one was already handled.
          return release_and_exit();
        }
        parent_->pendingIsTrain_ = false;
      } else {
        isGlobal_ = false;
      }
      nextTrainId_ = 0;
      hasMatches_ = false;
      unsigned tm_usec = (os_get_time_monotonic() / 1000) % 100000000;
      LOG(LATENCYDEBUG, "%02d.%06d train search iterate start",
          tm_usec / 1000000, tm_usec % 1000000);
      return call_immediately(STATE(iterate));
    }

    Action iterate() {
      if (nextTrainId_ >= nodes()->size()) {
        return call_immediately(STATE(iteration_done));
      }
      if (cancelIteration_) {
        LOG(LATENCYDEBUG, "search iteration cancelled");
        return call_immediately(STATE(iteration_done));
      }
      if (isGlobal_) {
        if (eventId_ == REQUEST_GLOBAL_IDENTIFY &&
            parent_->pendingGlobalIdentify_) {
          // Another notification arrived. Start iteration from 0.
          nextTrainId_ = 0;
          parent_->pendingGlobalIdentify_ = false;
          return again();
        }
        if (eventId_ == IS_TRAIN_EVENT &&
            parent_->pendingIsTrain_) {
          // Another notification arrived. Start iteration from 0.
          nextTrainId_ = 0;
          parent_->pendingIsTrain_ = false;
          return again();
        }
        return allocate_and_call(iface()->global_message_write_flow(),
                                 STATE(send_response));
      }
      return call_immediately(STATE(try_traindb_lookup));
    }

    /// This state attempts to look up the entry in the train database, and
    /// performs asynchronous waits and re-tries according to the contract of
    /// AllTrainNodesInterface.
    Action try_traindb_lookup() {
      bn_.reset(this);
      bn_.new_child();
      static uint32_t last_read = 0;
      auto db_entry = nodes()->get_traindb_entry(nextTrainId_, &bn_);
      unsigned tm_usec = (os_get_time_monotonic() / 1000) % 100000000;
      uint32_t counter = 0;
#if (LATENCYDEBUG == ALWAYS) && !defined(GTEST)
      counter = 0; //spiffsReadCount;
#endif
      if (!bn_.abort_if_almost_done()) {
        bn_.notify();
        last_read = counter;
        LOG(LATENCYDEBUG, "%02d.%06d lookup %d wait", tm_usec / 1000000,
            tm_usec % 1000000, nextTrainId_);
        // Repeats this state after the notification arrives.
        return wait();
      }
      // Call completed inline.
      // How many spiffs read operations were executed.
      uint32_t cnt = counter - last_read;
      LOG(LATENCYDEBUG, "%02d.%06d lookup %d done %" PRIu32, tm_usec / 1000000,
          tm_usec % 1000000, nextTrainId_, cnt);
      if (!db_entry) return call_immediately(STATE(next_iterate));
      if (FindProtocolDefs::match_query_to_node(eventId_, db_entry.get())) {
        hasMatches_ = true;
        return allocate_and_call(iface()->global_message_write_flow(),
                                 STATE(send_response));
      }
      return yield_and_call(STATE(next_iterate));
    }

    Action send_response() {
      auto *b = get_allocation_result(iface()->global_message_write_flow());
      b->set_done(bn_.reset(this));
      if (eventId_ == REQUEST_GLOBAL_IDENTIFY) {
        auto node_id = nodes()->get_train_node_id(nextTrainId_);
        b->data()->reset(
            openlcb::Defs::MTI_PRODUCER_IDENTIFIED_RANGE, node_id,
            openlcb::eventid_to_buffer(FindProtocolDefs::TRAIN_FIND_BASE));
        // send is_train event too.
        auto *bb = iface()->global_message_write_flow()->alloc();
        bb->data()->reset(openlcb::Defs::MTI_PRODUCER_IDENTIFIED_UNKNOWN,
                          node_id, openlcb::eventid_to_buffer(IS_TRAIN_EVENT));
        bb->set_done(bn_.new_child());
        bb->data()->set_flag_dst(openlcb::GenMessage::WAIT_FOR_LOCAL_LOOPBACK);
        iface()->global_message_write_flow()->send(bb);
      } else if (eventId_ == IS_TRAIN_EVENT) {
        b->data()->reset(openlcb::Defs::MTI_PRODUCER_IDENTIFIED_UNKNOWN,
                         nodes()->get_train_node_id(nextTrainId_),
                         openlcb::eventid_to_buffer(eventId_));
      } else {
        b->data()->reset(openlcb::Defs::MTI_PRODUCER_IDENTIFIED_VALID,
                         nodes()->get_train_node_id(nextTrainId_),
                         openlcb::eventid_to_buffer(eventId_));
      }
      b->data()->set_flag_dst(openlcb::GenMessage::WAIT_FOR_LOCAL_LOOPBACK);
      iface()->global_message_write_flow()->send(b);

      return wait_and_call(STATE(next_iterate));
    }

    Action next_iterate() {
      ++nextTrainId_;
      return call_immediately(STATE(iterate));
    }

    Action iteration_done() {
      unsigned tm_usec = (os_get_time_monotonic() / 1000) % 100000000;
      LOG(LATENCYDEBUG, "%02d.%06d train search iterate done",
          tm_usec / 1000000, tm_usec % 1000000);
      if (!hasMatches_ && !isGlobal_ &&
          (eventId_ & FindProtocolDefs::ALLOCATE)) {
        // TODO: we should wait some time, maybe 200 msec for any responses
        // from other nodes, possibly a deadrail train node, before we actually
        // allocate a new train node.
        DccMode mode;
        unsigned address = FindProtocolDefs::query_to_address(eventId_, &mode);
        newNodeId_ = nodes()->allocate_node(mode, address);
        if (!newNodeId_) {
          LOG(WARNING, "Decided to allocate node but failed. type=%d addr=%d",
              (int)mode, (int)address);
          return release_and_exit();
        }
        return call_immediately(STATE(wait_for_new_node));
      }
      return release_and_exit();
    }

    /// Yields until the new node is initialized and we are allowed to send
    /// traffic out from it.
    Action wait_for_new_node() {
      openlcb::Node *n = iface()->lookup_local_node(newNodeId_);
      HASSERT(n);
      if (n->is_initialized()) {
        return call_immediately(STATE(new_node_reply));
      } else {
        return sleep_and_call(&timer_, MSEC_TO_NSEC(1), STATE(wait_for_new_node));
      }
    }

    Action new_node_reply() {
      return allocate_and_call(iface()->global_message_write_flow(),
                               STATE(send_new_node_response));
    }

    Action send_new_node_response() {
      auto *b = get_allocation_result(iface()->global_message_write_flow());
      b->data()->reset(openlcb::Defs::MTI_PRODUCER_IDENTIFIED_VALID, newNodeId_,
                       openlcb::eventid_to_buffer(eventId_));
      iface()->global_message_write_flow()->send(b);
      return release_and_exit();
    }

   private:
    AllTrainNodesInterface *nodes() { return parent_->nodes(); }

    openlcb::If *iface() { return parent_->iface(); }

    openlcb::EventId eventId_;
    union {
      unsigned nextTrainId_;
      openlcb::NodeID newNodeId_;
    };
    BarrierNotifiable bn_;
    /// True if we found any matches during the iteration.
    bool hasMatches_ : 1;
    /// True if the current iteration has to touch every node.
    bool isGlobal_ : 1;
    /// A new request from the same node has arrived, let's cancel the current
    /// one.
    bool cancelIteration_ : 1;
    FindProtocolServer *parent_;
    StateFlowTimer timer_{this};
  };

  /// @return the openlcb interface to which the train nodes (and the traction
  /// service) are bound.
  openlcb::If *iface() { return service()->iface(); }

  /// @return the openlcb Traction Service.
  openlcb::TrainService *service() { return nodes()->train_service(); }

  /// @return the AllTrainNodes instance.
  AllTrainNodesInterface *nodes() { return nodes_; }

  /// Pointer to the AllTrainNodes instance. Externally owned.
  AllTrainNodesInterface *nodes_;
  /// Set to true when a global identify message is received. When a global
  /// identify starts processing, it shall be set to false. If a global
  /// identify request arrives with no pendingGlobalIdentify_, that is a
  /// duplicate request that can be ignored.
  uint8_t pendingGlobalIdentify_{false};
  /// Same as pendingGlobalIdentify_ for the IS_TRAIN event producer.
  uint8_t pendingIsTrain_{false};

  FindProtocolFlow flow_{this};
};

class SingleNodeFindProtocolServer : public openlcb::SimpleEventHandler {
 public:
  using Node = openlcb::Node;

  SingleNodeFindProtocolServer(Node *node, TrainDbEntry *db_entry)
      : node_(node), dbEntry_(db_entry) {
    openlcb::EventRegistry::instance()->register_handler(
        EventRegistryEntry(this, FindProtocolDefs::TRAIN_FIND_BASE),
        FindProtocolDefs::TRAIN_FIND_MASK);
  }

  ~SingleNodeFindProtocolServer() {
    openlcb::EventRegistry::instance()->unregister_handler(this);
  }

  void handle_identify_global(const EventRegistryEntry &registry_entry,
                              EventReport *event,
                              BarrierNotifiable *done) override {
    AutoNotify an(done);

    if (event && event->dst_node) {
      // Identify addressed
      if (event->dst_node != node_) return;
    }

    static_assert(((FindProtocolDefs::TRAIN_FIND_BASE >>
                    FindProtocolDefs::TRAIN_FIND_MASK) &
                   1) == 1,
                  "The lowermost bit of the TRAIN_FIND_BASE must be 1 or "
                  "else the event produced range encoding must be updated.");
    event->event_write_helper<1>()->WriteAsync(
        event->dst_node, openlcb::Defs::MTI_PRODUCER_IDENTIFIED_RANGE,
        openlcb::WriteHelper::global(),
        openlcb::eventid_to_buffer(FindProtocolDefs::TRAIN_FIND_BASE),
        done->new_child());
  }

  void handle_identify_producer(const EventRegistryEntry &registry_entry,
                                EventReport *event,
                                BarrierNotifiable *done) override {
    AutoNotify an(done);

    if (FindProtocolDefs::match_query_to_node(event->event, dbEntry_)) {
      event->event_write_helper<1>()->WriteAsync(
          node_, openlcb::Defs::MTI_PRODUCER_IDENTIFIED_VALID,
          openlcb::WriteHelper::global(),
          openlcb::eventid_to_buffer(event->event),
          done->new_child());
    }
  };

 private:
  Node* node_;
  TrainDbEntry* dbEntry_;
};

}  // namespace commandstation

#endif  // _COMMANDSTATION_FINDPROTOCOLSERVER_HXX_
