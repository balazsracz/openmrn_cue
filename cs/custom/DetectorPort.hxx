/** \copyright
 * Copyright (c) 2015, Balazs Racz
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
 * \file DetectorPort.hxx
 *
 * Business logic for one port of the railcom+occupancy detector.
 *
 * @author Balazs Racz
 * @date 19 Dec 2015
 */

#ifndef _BRACZ_CUSTOM_DETECTORPORT_HXX_
#define _BRACZ_CUSTOM_DETECTORPORT_HXX_

#include <functional>

#include "nmranet/CallbackEventHandler.hxx"
#include "hardware.hxx"
#include "custom/DetectorPortConfig.hxx"

extern const Gpio* const enable_ptrs[];

namespace bracz_custom {

namespace sp = std::placeholders;

/// TODO:
/// - add readout of event IDs from config
/// - add config itself
/// - register the event IDs
class DetectorPort : public StateFlowBase {
 public:
  DetectorPort(nmranet::Node* node, uint8_t channel, int config_fd,
               const DetectorPortConfig& cfg)
    : StateFlowBase(node->iface()),
        channel_(channel),
        killedOutputDueToOvercurrent_(0),
        sensedOccupancy_(0),
        networkOvercurrent_(0),
        networkOccupancy_(0),
        commandedEnable_(0),
        networkEnable_(0),
        isInitialTurnon_(0),
        reqEnable_(0),
        turnonTryCount_(0),
        node_(node),
        eventHandler_(
            node,
            std::bind(&DetectorPort::event_report, this, sp::_1, sp::_2, sp::_3),
            std::bind(&DetectorPort::get_event_state, this, sp::_1, sp::_2)) {
    start_flow(STATE(init_wait));
    eventOccupancyOn_ = cfg.occupancy().occ_on().read(config_fd);
    eventOccupancyOff_ = cfg.occupancy().occ_off().read(config_fd);
    eventOvercurrentOn_ = cfg.overcurrent().over_on().read(config_fd);
    eventOvercurrentOff_ = cfg.overcurrent().over_off().read(config_fd);
    eventEnableOn_ = cfg.enable().enable_on().read(config_fd);
    eventEnableOff_ = cfg.enable().enable_off().read(config_fd);

    eventHandler_.add_entry(
        eventOccupancyOn_,
        OCCUPANCY_BIT | ARG_ON | nmranet::CallbackEventHandler::IS_PRODUCER);
    eventHandler_.add_entry(
        eventOccupancyOff_,
        OCCUPANCY_BIT | nmranet::CallbackEventHandler::IS_PRODUCER);

    eventHandler_.add_entry(
        eventOvercurrentOn_,
        OVERCURRENT_BIT | ARG_ON | nmranet::CallbackEventHandler::IS_PRODUCER);
    eventHandler_.add_entry(
        eventOvercurrentOff_,
        OVERCURRENT_BIT | nmranet::CallbackEventHandler::IS_PRODUCER);

    eventHandler_.add_entry(
        eventEnableOn_,
        ENABLE_BIT | ARG_ON | nmranet::CallbackEventHandler::IS_CONSUMER);
    eventHandler_.add_entry(
        eventEnableOff_,
        ENABLE_BIT | nmranet::CallbackEventHandler::IS_CONSUMER);
  }

  void set_overcurrent(bool value) {
    if (value) {
      // We have a short circuit. Stop the output.
      set_output_enable(false);
      killedOutputDueToOvercurrent_ = 1;
      if (is_terminated()) {
        start_flow(STATE(test_again));
      }
    }
  }

  void set_occupancy(bool value) {
    sensedOccupancy_ = to_u(value);
    if (is_terminated()) {
      start_flow(STATE(test_again));
    }
  }

  void set_network_occupancy(bool value) {
    networkOccupancy_ = to_u(value);
    if (is_terminated()) {
      start_flow(STATE(test_again));
    }
  }

  void set_network_overcurrent(bool value) {
    networkOvercurrent_ = to_u(value);
    if (is_terminated()) {
      start_flow(STATE(test_again));
    }
  }

  void set_network_enable(bool value) {
    if (value && !networkEnable_) {
      // turn on
      networkEnable_ = 1;
      reqEnable_ = 1;
      if (is_terminated()) {
        start_flow(STATE(test_again));
      }
    }
    if (!value && networkEnable_) {
      // turn off
      networkEnable_ = 0;
      set_output_enable(false);
    }
  }

 private:
  void event_report(const nmranet::EventRegistryEntry& registry_entry,
                    nmranet::EventReport* report, BarrierNotifiable* done) {
    uint32_t user_arg = registry_entry.user_arg;
    switch (user_arg & MASK_FOR_BIT) {
      case ENABLE_BIT:
        set_network_enable(user_arg & ARG_ON);
        return;
      case OVERCURRENT_BIT:
        set_network_overcurrent(user_arg & ARG_ON);
        return;
      case OCCUPANCY_BIT:
        set_network_occupancy(user_arg & ARG_ON);
        return;
    };
  }

  nmranet::EventState get_event_state(
      const nmranet::EventRegistryEntry& registry_entry,
      nmranet::EventReport* report) {
    nmranet::EventState st = nmranet::EventState::UNKNOWN;
    switch (registry_entry.user_arg & MASK_FOR_BIT) {
      case ENABLE_BIT:
        st = nmranet::to_event_state(networkEnable_);
        break;
      case OVERCURRENT_BIT:
        st = nmranet::to_event_state(killedOutputDueToOvercurrent_);
        break;
      case OCCUPANCY_BIT:
        st = nmranet::to_event_state(networkOccupancy_);
        break;
    };
    if (!(registry_entry.user_arg & ARG_ON)) {
      st = invert_event_state(st);
    }
    return nmranet::EventState::VALID;
  }

  /// Defines what we use the user_args bits for the registered event handlers.
  enum EventArgs {
    ENABLE_BIT = 1,
    OCCUPANCY_BIT = 2,
    OVERCURRENT_BIT = 3,
    MASK_FOR_BIT = 3,
    ARG_ON = 16,
  };

  /// How much do we wait between retrying shorted outputs. This also defines
  /// how long we wait after enabling an output to see if it shorted.
  static const int SHORT_RETRY_WAIT_MSEC = 300;
  /// Number of times we try to enable an output and find it shorted before
  /// declaring the output shorted.
  static const int SHORT_RETRY_COUNT = 3;

  unsigned to_u(bool b) { return b ? 1 : 0; }

  /// @param value is true if the output should be provided with power, false
  /// if the output should have no power.
  void set_output_enable(bool value) {
    commandedEnable_ = to_u(value);
    enable_ptrs[channel_]->write(!value);  // inverted
    if (value) {
      killedOutputDueToOvercurrent_ = to_u(false);
    }
  }

  Action init_wait() {
    isInitialTurnon_ = 1;
    return sleep_and_call(&timer_, MSEC_TO_NSEC(100 * channel_),
                          STATE(init_try_turnon));
  }

  Action init_try_turnon() {
    turnonTryCount_ = 0;
    networkEnable_ = 1;
    return call_immediately(STATE(try_turnon));
  }

  Action try_turnon() {
    if (!networkEnable_) {
      // Someone turned off the output in the meantime.
      set_output_enable(false);  // this should be redundant
      return call_immediately(STATE(test_again));
    }
    set_output_enable(true);
    return sleep_and_call(&timer_, MSEC_TO_NSEC(SHORT_RETRY_WAIT_MSEC),
                          STATE(eval_turnon));
  }

  Action eval_turnon() {
    if (killedOutputDueToOvercurrent_) {
      // Turnon failed. try again.
      if (turnonTryCount_ < SHORT_RETRY_COUNT) {
        turnonTryCount_++;
        return call_immediately(STATE(try_turnon));
      }
      // Turnon failed, no more tries left.
      if (!networkOvercurrent_ || isInitialTurnon_) {
        networkOvercurrent_ = to_u(true);
        isInitialTurnon_ = 0;
        return produce_event(eventOvercurrentOn_);
      }
      // Network already knows we are in overcurrent -- Do nothing.
      return call_immediately(STATE(test_again));
    }
    // now: not overcurrent -- we're successfully on.
    if (networkOvercurrent_ || isInitialTurnon_) {
      networkOvercurrent_ = to_u(false);
      isInitialTurnon_ = 0;
      return produce_event(eventOvercurrentOff_);
    }
    return call_immediately(STATE(test_again));
  }

  // Evaluates that the network-visible states are the same as the locally
  // sensed state.
  Action test_again() {
    if (commandedEnable_) {
      turnonTryCount_ = 0;
      // If we have track power, we produce occupancy events.
      if (networkOccupancy_ != sensedOccupancy_) {
        networkOccupancy_ = sensedOccupancy_;
        if (sensedOccupancy_) {
          return produce_event(eventOccupancyOn_);
        } else {
          return produce_event(eventOccupancyOff_);
        }
      }
    }
    if (reqEnable_) {
      reqEnable_ = 0;
      return call_immediately(STATE(init_try_turnon));
    }
    if (networkEnable_ && !turnonTryCount_ && killedOutputDueToOvercurrent_) {
      return sleep_and_call(&timer_, MSEC_TO_NSEC(SHORT_RETRY_WAIT_MSEC),
                            STATE(eval_turnon));
    }
    if (isInitialTurnon_) {
      isInitialTurnon_ = 0;
    }
    return set_terminated();
  }

  Action produce_event(nmranet::EventId event_id) {
    h_.WriteAsync(node_, nmranet::Defs::MTI_EVENT_REPORT,
                  nmranet::WriteHelper::global(),
                  nmranet::eventid_to_buffer(event_id), n_.reset(this));
    return wait_and_call(STATE(test_again));
  }

  StateFlowTimer timer_{this};
  uint8_t channel_;
  /// set-once when overcurrent is detected.
  uint8_t killedOutputDueToOvercurrent_ : 1;
  uint8_t sensedOccupancy_ : 1;     //< Current (debounced) sensor read
  uint8_t networkOvercurrent_ : 1;  //< Current network state
  uint8_t networkOccupancy_ : 1;    //< Current network state
  uint8_t commandedEnable_ : 1;     //< Current output pin state
  uint8_t networkEnable_ : 1;       //< Current network state
  uint8_t isInitialTurnon_ : 1;     //< true only for the first turnon try.
  uint8_t
      reqEnable_ : 1;  //< 1 if a network request came in to enable the output.

  /*
  uint8_t reqOccupancyOn_ : 1;      //< produce event occ ON
  uint8_t reqOccupancyOff_ : 1;      //< produce event occ OFF
  uint8_t reqOvercurrentOn_ : 1;      //< produce event overcurrent ON
  uint8_t reqOvercurrentOff_ : 1;      //< produce event overcurrent OFF
  */

  /// how many tries we had yet for turning on output.
  uint8_t turnonTryCount_ : 3;
  nmranet::Node* node_;
  nmranet::EventId eventOccupancyOn_;
  nmranet::EventId eventOccupancyOff_;
  nmranet::EventId eventOvercurrentOn_;
  nmranet::EventId eventOvercurrentOff_;
  nmranet::EventId eventEnableOn_;
  nmranet::EventId eventEnableOff_;
  nmranet::WriteHelper h_;
  BarrierNotifiable n_;
  nmranet::CallbackEventHandler eventHandler_;
};

}  // namespace bracz_custom

#endif  // _BRACZ_CUSTOM_DETECTORPORT_HXX_
