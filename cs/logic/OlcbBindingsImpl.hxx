/** \copyright
 * Copyright (c) 2019, Balazs Racz
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
 * \file OlcbBindingsImpl.hxx
 *
 * Internal implementation classes of external variables for OpenLCB bindings
 * purposes.
 *
 * @author Balazs Racz
 * @date 25 June 2019
 */

#ifndef _LOGIC_OLCBBINDINGSIMPL_HXX_
#define _LOGIC_OLCBBINDINGSIMPL_HXX_

#include "logic/OlcbBindings.hxx"

#include "openlcb/EventHandlerTemplates.hxx"

namespace logic {

struct BindingsHelper {
};

class OlcbBoolVariable : public Variable, private openlcb::BitEventInterface {
 public:
  OlcbBoolVariable(uint64_t event_on, uint64_t event_off,
                   OlcbVariableFactory *parent)
      : BitEventInterface(event_on, event_off),
        state_(false),
        state_known_(false),
        parent_(parent) {
    // This will ensure that all local variables have replied to this query
    // before continuing. 
    parent_->helper_.set_wait_for_local_loopback(true);
    pc_.SendQueryProducer(&parent_->helper_, parent_->bn_.reset(&parent_->sn_));
    parent_->sn_.wait_for_notification();
    pc_.SendQueryConsumer(&parent_->helper_, parent_->bn_.reset(&parent_->sn_));
    parent_->sn_.wait_for_notification();
  }

  int max_state() override {
    // boolean: can be 0 or 1.
    return 1;
  }

  int read(const VariableFactory* parent, unsigned arg) override {
    return state_ ? 1 : 0;
  }
  
  void write(const VariableFactory *parent, unsigned arg, int value) override {
    bool need_update = !state_known_;
    state_known_ = true;
    if (state_ && !value) need_update = true;
    if (!state_ && value) need_update = true;
    state_ = value ? true : false;
    if (need_update) {
      parent_->helper_.set_wait_for_local_loopback(true);
      pc_.SendEventReport(&parent_->helper_, parent_->bn_.reset(&parent_->sn_));
      parent_->sn_.wait_for_notification();
    }
  }

 private:
  openlcb::Node *node() override
  {
    return parent_->node_;
  }
  openlcb::EventState get_current_state() override
  {
    if (!state_known_) return openlcb::EventState::UNKNOWN;
    return state_ ? openlcb::EventState::VALID : openlcb::EventState::INVALID;
  }
  void set_state(bool new_value) override
  {
    state_known_ = true;
    state_ = new_value;
  }

  /// The current state of the variable as seen by the internal
  /// processing. (0=false, 1=true). This state is exported to the network iff
  /// state_known_ == 1.
  uint8_t state_ : 1;
  /// if 0, network queries will return UNKNOWN. If 1, network queries will
  /// return the state as defined by the state_ variable.
  uint8_t state_known_ : 1;
  /// Pointer to the Olcb variable factory that owns this. externally owned.
  OlcbVariableFactory* parent_;
  /// Implementation of the producer-consumer event handler.
  openlcb::BitEventPC pc_{this};
};


class OlcbIntVariable : public Variable, private openlcb::SimpleEventHandler {
 public:
  int max_state() override {
    // boolean: can be 0 or 1.
    return num_states_ - 1;
  }

  int read(const VariableFactory* parent, unsigned arg) override {
    return state_;
  }
  
  void write(const VariableFactory *parent, unsigned arg, int value) override {
    if (value < 0 || value >= num_states_) {
      // ignore bad writes.
      parent_->report_access_error();
      return;
    }
    bool need_update = !state_known_;
    state_known_ = true;
    if (((int)state_) != value) need_update = true;
    state_ = value;
    if (need_update) {
      parent_->helper_.set_wait_for_local_loopback(true);
      parent_->helper_.WriteAsync(
          node(), openlcb::Defs::MTI_EVENT_REPORT,
          openlcb::WriteHelper::global(),
          openlcb::eventid_to_buffer(event_base_ + state_),
          parent_->bn_.reset(&parent_->sn_));
      parent_->sn_.wait_for_notification();
    }
  }

  /// @return true if the state is known (recovered from the network or set).
  bool is_known() {
    return state_known_;
  }
  
  /// Constructor.
  /// @param event_base is the event ID that is equivalent to state 0.
  /// @param num_states is the number of consecutive event IDs that are
  /// meaningful starting with event_base.
  /// @param parent is the variable factory that owns *this.
  OlcbIntVariable(uint64_t event_base, uint8_t num_states,
                  OlcbVariableFactory* parent)
      : event_base_(event_base),
        state_known_(false),
        state_(0),
        num_states_(num_states),
        parent_(parent) {
    uint64_t reg_event = event_base_;
    unsigned mask = openlcb::EventRegistry::align_mask(&reg_event, num_states_);
    openlcb::EventRegistry::instance()->register_handler(
        EventRegistryEntry(this, reg_event, 0), mask);

    StateType ofs = 0;
    while (!state_known_ && ofs < num_states_) {
      parent_->helper_.set_wait_for_local_loopback(true);
      parent_->helper_.WriteAsync(node(), openlcb::Defs::MTI_CONSUMER_IDENTIFY,
                                  openlcb::WriteHelper::global(),
                                  openlcb::eventid_to_buffer(event_base_ + ofs),
                                  parent_->bn_.reset(&parent_->sn_));
      parent_->sn_.wait_for_notification();
      ++ofs;
    }
  }

  ~OlcbIntVariable() {
    openlcb::EventRegistry::instance()->unregister_handler(this);
  }

  void handle_event_report(const openlcb::EventRegistryEntry &entry,
                           openlcb::EventReport *event,
                           BarrierNotifiable *done) override {
    AutoNotify an(done);
    if (decode_event(event->event, &state_)) {
      state_known_ = true;
    }
  }

  void handle_identify_consumer(
      const openlcb::EventRegistryEntry &registry_entry,
      openlcb::EventReport *event, BarrierNotifiable *done) override {
    AutoNotify an(done);
    StateType st;
    if (!decode_event(event->event, &st)) {
      return;
    }
    openlcb::EventState est =
        state_known_ ? (st == state_ ? openlcb::EventState::VALID
                                     : openlcb::EventState::INVALID)
                     : openlcb::EventState::UNKNOWN;
    openlcb::Defs::MTI mti = openlcb::Defs::MTI_CONSUMER_IDENTIFIED_VALID + est;
    event->event_write_helper<1>()->WriteAsync(
        node(), mti, openlcb::WriteHelper::global(),
        openlcb::eventid_to_buffer(event->event), done->new_child());
    if (state_known_ && st != state_) {
      event->event_write_helper<2>()->WriteAsync(
          node(), openlcb::Defs::MTI_CONSUMER_IDENTIFIED_VALID,
          openlcb::WriteHelper::global(),
          openlcb::eventid_to_buffer(event_base_ + state_), done->new_child());
    }
  };

  void handle_identify_producer(
      const openlcb::EventRegistryEntry &registry_entry,
      openlcb::EventReport *event, BarrierNotifiable *done) override {
    AutoNotify an(done);
    StateType st;
    if (!decode_event(event->event, &st)) {
      return;
    }
    openlcb::EventState est =
        state_known_ ? (st == state_ ? openlcb::EventState::VALID
                                     : openlcb::EventState::INVALID)
                     : openlcb::EventState::UNKNOWN;
    openlcb::Defs::MTI mti = openlcb::Defs::MTI_PRODUCER_IDENTIFIED_VALID + est;
    event->event_write_helper<1>()->WriteAsync(
        node(), mti, openlcb::WriteHelper::global(),
        openlcb::eventid_to_buffer(event->event), done->new_child());
    if (state_known_ && st != state_) {
      event->event_write_helper<2>()->WriteAsync(
          node(), openlcb::Defs::MTI_PRODUCER_IDENTIFIED_VALID,
          openlcb::WriteHelper::global(),
          openlcb::eventid_to_buffer(event_base_ + state_), done->new_child());
    }
  };

  void handle_consumer_identified(
      const openlcb::EventRegistryEntry &registry_entry,
      openlcb::EventReport *event, BarrierNotifiable *done) override {
    AutoNotify an(done);
    if (state_known_) return;
    if (event->state != openlcb::EventState::VALID) return;
    if (decode_event(event->event, &state_)) {
      state_known_ = true;
    }
  }

  void handle_producer_identified(
      const openlcb::EventRegistryEntry &registry_entry,
      openlcb::EventReport *event, BarrierNotifiable *done) override {
    AutoNotify an(done);
    if (state_known_) return;
    if (event->state != openlcb::EventState::VALID) return;
    if (decode_event(event->event, &state_)) {
      state_known_ = true;
    }
  }
  
  void handle_identify_global(const openlcb::EventRegistryEntry &registry_entry,
                              openlcb::EventReport *event,
                              BarrierNotifiable *done) override {
    LOG(INFO, "handle identify global");
    AutoNotify an(done);
    if (event->dst_node && event->dst_node != node()) {
      return;
    }
    auto range = openlcb::EncodeRange(event_base_, num_states_);
    event->event_write_helper<1>()->WriteAsync(
        node(), openlcb::Defs::MTI_CONSUMER_IDENTIFIED_RANGE,
        openlcb::WriteHelper::global(), openlcb::eventid_to_buffer(range),
        done->new_child());
    event->event_write_helper<2>()->WriteAsync(
        node(), openlcb::Defs::MTI_PRODUCER_IDENTIFIED_RANGE,
        openlcb::WriteHelper::global(), openlcb::eventid_to_buffer(range),
        done->new_child());
    if (state_known_) {
      event->event_write_helper<3>()->WriteAsync(
          node(), openlcb::Defs::MTI_PRODUCER_IDENTIFIED_VALID,
          openlcb::WriteHelper::global(),
          openlcb::eventid_to_buffer(event_base_ + state_), done->new_child());
      event->event_write_helper<4>()->WriteAsync(
          node(), openlcb::Defs::MTI_CONSUMER_IDENTIFIED_VALID,
          openlcb::WriteHelper::global(),
          openlcb::eventid_to_buffer(event_base_ + state_), done->new_child());
    }
  };

 private:
  typedef uint8_t StateType;
  
  /// @return the node pointer on which this variable is exported.
  openlcb::Node *node()
  {
    return parent_->node();
  }

  /// Test if a given event ID is interesting for us.
  /// @param event_id event coming form the network.
  /// @param st will be set to the state it means.
  /// @return true if it is an interesting event (st will be set), false if it
  /// is not an event for us (st will be uninitialized).
  bool decode_event(uint64_t event_id, StateType* st) {
    if (event_id < event_base_) return false;
    if (event_id >= event_base_ + num_states_) return false;
    *st = event_id - event_base_;
    return true;
  }

  uint64_t event_base_;
  uint8_t state_known_ : 1;
  StateType state_;
  /// Number of different states. Largest event valid is event_base_ +
  /// num_states_ - 1.
  StateType num_states_;
  /// Pointer to the Olcb variable factory that owns this. externally owned.
  OlcbVariableFactory* parent_;
};

} // namespace logic

#endif // _LOGIC_OLCBBINDINGSIMPL_HXX_
