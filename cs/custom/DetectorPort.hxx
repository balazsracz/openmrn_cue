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

#include "openlcb/CallbackEventHandler.hxx"
#include "hardware.hxx"
#include "custom/DetectorPortConfig.hxx"
#include "utils/StoredBitSet.hxx"
#include "utils/ConfigUpdateListener.hxx"

extern const Gpio* const enable_ptrs[];

namespace bracz_custom {

namespace sp = std::placeholders;

class DetectorOptions : public DefaultConfigUpdateListener {
 public:
  DetectorOptions(const DetectorTuningOptions& opts) : opts_(opts) {}

  uint16_t initStraggleDelay_;
  uint16_t initStaticDelay_;

  uint16_t turnonRetryDelay_;
  uint16_t shortRetryDelay_;

  uint8_t turnonRetryCount_;
  uint8_t isInitialized_{0}; // 1 when data is loaded.
  
  uint16_t sensorOffDelay_;

  
 private:
  UpdateAction apply_configuration(int fd, bool initial_load,
                                   BarrierNotifiable* done) override {
    AutoNotify an(done);
    initStraggleDelay_ = opts_.init_straggle_delay().read(fd);
    if (initStraggleDelay_ == 0xffff) {
      factory_reset(fd);
      initStraggleDelay_ = opts_.init_straggle_delay().read(fd);
    }
    initStaticDelay_ = opts_.init_static_delay().read(fd);
    turnonRetryCount_ = opts_.turnon_fast_retry_count().read(fd);
    turnonRetryDelay_ = opts_.turnon_fast_retry_delay().read(fd);

    shortRetryDelay_ = opts_.short_retry_delay().read(fd);

    sensorOffDelay_ = opts_.sensor_off_delay().read(fd);
    isInitialized_ = 1;
    return UPDATED;
  }

  void factory_reset(int fd) override {
    CDI_FACTORY_RESET(opts_.sensor_off_delay);
    CDI_FACTORY_RESET(opts_.init_straggle_delay);
    CDI_FACTORY_RESET(opts_.init_static_delay);
    CDI_FACTORY_RESET(opts_.turnon_fast_retry_count);
    CDI_FACTORY_RESET(opts_.turnon_fast_retry_delay);
    CDI_FACTORY_RESET(opts_.short_retry_delay);
  }

 private:
  const DetectorTuningOptions opts_;

  DISALLOW_COPY_AND_ASSIGN(DetectorOptions);
};

extern StoredBitSet* const g_gpio_stored_bit_set;

/// TODO:
/// - add readout of event IDs from config
/// - add config itself
/// - register the event IDs
class DetectorPort : public StateFlowBase {
 public:
  DetectorPort(openlcb::Node* node, uint8_t channel, int config_fd,
               const DetectorPortConfig& cfg, const DetectorOptions& opts)
      : StateFlowBase(node->iface()),
        channel_(channel),
        killedOutputDueToOvercurrent_(0),
        sensedOccupancy_(0),
        networkOvercurrent_(0),
        networkOccupancy_(0),
        commandedEnable_(0),
        networkEnable_(0),
        networkOccupancyKnown_(0),
        networkOvercurrentKnown_(0),
        seenOccupancyOn_(0),
        seenOccupancyOff_(0),
        isWaitingForLoop_(0),
        turnonTryCount_(0),
        node_(node),
        opts_(opts),
        eventHandler_(
            node, std::bind(&DetectorPort::event_report, this, sp::_1, sp::_2,
                            sp::_3),
            std::bind(&DetectorPort::get_event_state, this, sp::_1, sp::_2)) {
    start_flow(STATE(init_start));
    eventOccupancyOn_ = cfg.occupancy().occ_on().read(config_fd);
    eventOccupancyOff_ = cfg.occupancy().occ_off().read(config_fd);
    eventOvercurrentOn_ = cfg.overcurrent().over_on().read(config_fd);
    eventOvercurrentOff_ = cfg.overcurrent().over_off().read(config_fd);
    eventEnableOn_ = cfg.enable().enable_on().read(config_fd);
    eventEnableOff_ = cfg.enable().enable_off().read(config_fd);

    eventHandler_.add_entry(
        eventOccupancyOn_,
        OCCUPANCY_BIT | ARG_ON | openlcb::CallbackEventHandler::IS_PRODUCER);
    eventHandler_.add_entry(
        eventOccupancyOff_,
        OCCUPANCY_BIT | openlcb::CallbackEventHandler::IS_PRODUCER);

    eventHandler_.add_entry(
        eventOvercurrentOn_,
        OVERCURRENT_BIT | ARG_ON | openlcb::CallbackEventHandler::IS_PRODUCER);
    eventHandler_.add_entry(
        eventOvercurrentOff_,
        OVERCURRENT_BIT | openlcb::CallbackEventHandler::IS_PRODUCER);

    eventHandler_.add_entry(
        eventEnableOn_,
        ENABLE_BIT | ARG_ON | openlcb::CallbackEventHandler::IS_CONSUMER);
    eventHandler_.add_entry(
        eventEnableOff_,
        ENABLE_BIT | openlcb::CallbackEventHandler::IS_CONSUMER);
  }

  bool is_enabled() {
    return commandedEnable_;
  }

  void set_overcurrent(bool value) {
    if (value) {
      // We have a short circuit. Stop the output.
      set_output_enable(false);
      killedOutputDueToOvercurrent_ = 1;
      wakeup_normal_state();
    }
  }

  void set_occupancy(bool value) {
    sensedOccupancy_ = to_u(value);
    if (value) {
      seenOccupancyOn_ = 1;
    } else {
      seenOccupancyOff_ = 1;
    }
    wakeup_normal_state();
  }

  void set_network_occupancy(bool value) {
    networkOccupancy_ = to_u(value);
    if (networkOccupancy_ == sensedOccupancy_) {
      // We write down this bit in case it prevents us from producing the same
      // event when the timer expires.
      g_gpio_stored_bit_set->set_bit(get_storage_base() + NETWORK_OCCUPANCY_OFS,
                                     sensedOccupancy_).lock_and_flush();

    }
    wakeup_normal_state();
  }

  void set_network_overcurrent(bool value) {
    networkOvercurrent_ = to_u(value);
    // TODO: maybe we should cancel some timer here, or attempt to go to the
    // retry immediately.
    wakeup_normal_state();
  }

  void set_network_enable(bool value) {
    networkEnable_ = to_u(value);
    g_gpio_stored_bit_set->set_bit(get_storage_base() + NETWORK_ENABLE_OFS,
                                   value).lock_and_flush();
    if (networkEnable_ && is_state(STATE(network_off))) {
      // wake up from terminal state
      notify();
    }
    if (!value) {
      // turn off
      set_output_enable(false);
    }
    wakeup_normal_state();
  }

 private:
  void event_report(const openlcb::EventRegistryEntry& registry_entry,
                    openlcb::EventReport* report, BarrierNotifiable* done) {
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

  openlcb::EventState get_event_state(
      const openlcb::EventRegistryEntry& registry_entry,
      openlcb::EventReport* report) {
    openlcb::EventState st = openlcb::EventState::UNKNOWN;
    switch (registry_entry.user_arg & MASK_FOR_BIT) {
      case ENABLE_BIT:
        st = openlcb::to_event_state(networkEnable_);
        break;
      case OVERCURRENT_BIT:
        if (networkOvercurrentKnown_) {
          /// @TODO: this should look at a different bit here, probably the
          /// stored state ofthe overcurrent bit.
          st = openlcb::to_event_state(killedOutputDueToOvercurrent_);
        }
        break;
      case OCCUPANCY_BIT:
        st = openlcb::to_event_state(networkOccupancy_);
        break;
    };
    if (!(registry_entry.user_arg & ARG_ON)) {
      st = invert_event_state(st);
    }
    return st;
  }

  /// Defines what we use the user_args bits for the registered event handlers.
  enum EventArgs {
    ENABLE_BIT = 1,
    OCCUPANCY_BIT = 2,
    OVERCURRENT_BIT = 3,
    MASK_FOR_BIT = 3,
    ARG_ON = 16,
  };

  // Converts a bool to an unsigned:1 type.
  unsigned to_u(bool b) { return b ? 1 : 0; }

  // We need to persistently store the following information per port:
  //
  // - what is the last state of the occupancy information. This is important
  //   to keep when we lose track power and/or turn off the track.
  //
  // - is the network state of the port OFF ? (e.g. it has been turned off by
  //   an event by someone else.)
  //
  // - if we seem to be going into a short circuit problem with this port
  //   (though not 100% sure how we should be using that information). Maybe
  //   also when we have disabled this port due to overcurrent.

  /// @param value is true if the output should be provided with power, false
  /// if the output should have no power.
  void set_output_enable(bool value) {
    commandedEnable_ = to_u(value);
    enable_ptrs[channel_]->write(!value);  // inverted
    if (value) {
      killedOutputDueToOvercurrent_ = to_u(false);
    }
  }

  Action init_start() {
    networkEnable_ = to_u(g_gpio_stored_bit_set->get_bit(get_storage_base() + NETWORK_ENABLE_OFS));
    networkOccupancy_ = to_u(g_gpio_stored_bit_set->get_bit(get_storage_base() + NETWORK_OCCUPANCY_OFS));
    networkOccupancyKnown_ = 1;
    return call_immediately(STATE(init_wait));
  }
  
  Action init_wait() {
    // Waits for the parallel running flow that loads the default settings from
    // EEPROM.
    if (!opts_.isInitialized_) {
      return sleep_and_call(&timer_, MSEC_TO_NSEC(5), STATE(init_wait));
    }
    // TODO: make the decision here whether we will turn on this port initially
    // or rather not because it's either persistently turned off or we we know
    // there is a pending short. Also load the saved state of whether the
    // output was turned off. For a pending short move after the turnon delay
    // to the short_wait_for_retry state.
    
    

    if (!networkEnable_) {
      set_output_enable(false);
      return call_immediately(STATE(set_network_off));
    }
    int init_delay_msec = int(opts_.initStaticDelay_) +
                          int(channel_) * int(opts_.initStraggleDelay_);
    return sleep_and_call(&timer_, MSEC_TO_NSEC(init_delay_msec),
                          STATE(init_try_turnon));
  }

  Action init_try_turnon() {
    turnonTryCount_ = 0;
    return call_immediately(STATE(try_turnon));
  }

  Action try_turnon() {
    if (!networkEnable_) {
      // Someone turned off the output in the meantime.
      return call_immediately(STATE(set_network_off));
    }
    set_output_enable(true);
    return sleep_and_call(&timer_, MSEC_TO_NSEC(opts_.turnonRetryDelay_),
                          STATE(eval_turnon));
  }

  Action eval_turnon() {
    if (!networkEnable_) {
      // Someone turned off the output in the meantime.
      return call_immediately(STATE(set_network_off));
    }
    if (killedOutputDueToOvercurrent_) {
      // TODO: check this part again.
      // Turnon failed. try again.
      if (turnonTryCount_ < opts_.turnonRetryCount_) {
        turnonTryCount_++;
        return call_immediately(STATE(try_turnon));
      }
      // Turnon failed, no more tries left.
      return call_immediately(STATE(produce_shorted));
    }
    // now: not overcurrent -- we're successfully on.
    return call_immediately(STATE(produce_not_shorted));
  }

  Action set_network_off() {
    set_output_enable(false);  // this should be redundant
    return wait_and_call(STATE(network_off));
  }

  Action network_off() {
    if (!networkEnable_) {
      // This is a terminal state.
      return wait();
    }
    // Turn back on event came.
    return call_immediately(STATE(init_try_turnon));
  }

  Action produce_shorted() {
    auto n = STATE(short_wait_for_retry);
    if (!networkOvercurrent_ || !networkOvercurrentKnown_) {
      networkOvercurrent_ = 1;
      networkOvercurrentKnown_ = 1;
      return produce_event(eventOvercurrentOn_, n);
    } else {
      return call_immediately(n);
    }
  }

  Action produce_not_shorted() {
    auto n = STATE(eval_normal_state);
    if (networkOvercurrent_ || !networkOvercurrentKnown_) {
      networkOvercurrent_ = 0;
      networkOvercurrentKnown_ = 1;
      return produce_event(eventOvercurrentOff_, n);
    } else {
      return call_immediately(n);
    }
  }

  Action short_wait_for_retry() {
    return sleep_and_call(&timer_, MSEC_TO_NSEC(opts_.shortRetryDelay_),
                          STATE(try_turnon));
  }

  // When the track is normally on, and we're doing occupancy sensing, this
  // state checks if we need to send out any event or start any timer, else
  // drops into a wait state.
  Action eval_normal_state() {
    if (killedOutputDueToOvercurrent_) {
      turnonTryCount_ = 1;  // already seen a short once
      return sleep_and_call(&timer_, MSEC_TO_NSEC(opts_.turnonRetryDelay_),
                            STATE(try_turnon));
    }
    if (!networkEnable_) {
      // Someone turned off the output in the meantime.
      return call_immediately(STATE(set_network_off));
    }
    if (!networkOccupancyKnown_) {
      return call_immediately(STATE(produce_occupancy));
    }
    if (networkOccupancy_ != sensedOccupancy_) {
      // We have a change in occupancy.
      seenOccupancyOff_ = 0;
      seenOccupancyOn_ = 0;
      if (sensedOccupancy_) {
        // No delay in going to occupied.
        return call_immediately(STATE(produce_occupancy));
      } else {
        // Block release delay / debouncer.
        //
        // TODO: this is not good if the debounce timer is very long, because
        // it prevents short circuit retries from operating.
        return sleep_and_call(&timer_, MSEC_TO_NSEC(opts_.sensorOffDelay_),
                              STATE(sensor_debounce_done));
      }
    }
    isWaitingForLoop_ = 1;
    return wait_and_call(STATE(delay_normal));
  }

  // An interruptible wait state for the "normal" eval loop. When
  // isWaitingForLoop_ == 1 then notify() will run a single evaluation loop.
  Action delay_normal() {
    // will be reset by the caller of notify too
    isWaitingForLoop_ = 0;
    return call_immediately(STATE(eval_normal_state));
  }

  void wakeup_normal_state() {
    if (isWaitingForLoop_) {
      isWaitingForLoop_ = 0;
      notify();
    }
  }

  // Sends off the occupancy event.
  Action produce_occupancy() {
    networkOccupancyKnown_ = 1;
    networkOccupancy_ = sensedOccupancy_;
    g_gpio_stored_bit_set->set_bit(get_storage_base() + NETWORK_OCCUPANCY_OFS,
                                   sensedOccupancy_).lock_and_flush();
    return produce_event(
        sensedOccupancy_ ? eventOccupancyOn_ : eventOccupancyOff_,
        STATE(eval_normal_state));
  }

  // When going inactive, this is called after the inactivity period expires.
  Action sensor_debounce_done() {
    if (killedOutputDueToOvercurrent_) {
      turnonTryCount_ = 1;  // already seen a short once
      return sleep_and_call(&timer_, MSEC_TO_NSEC(opts_.turnonRetryDelay_),
                            STATE(try_turnon));
    }
    if (!networkEnable_) {
      // Someone turned off the output in the meantime.
      return call_immediately(STATE(set_network_off));
    }

    if (seenOccupancyOn_) {
      // failed the inactivity timer test. Do not produce the event.
      return call_immediately(STATE(eval_normal_state));
    }

    return call_immediately(STATE(produce_occupancy));
  }

  Action produce_event(openlcb::EventId event_id, Callback c) {
    h_.WriteAsync(node_, openlcb::Defs::MTI_EVENT_REPORT,
                  openlcb::WriteHelper::global(),
                  openlcb::eventid_to_buffer(event_id), n_.reset(this));
    return wait_and_call(c);
  }

  enum StorageConstants {
    /// The first bit in the storedbitset that belongs to DetectorPorts.
    CHANNEL_0_OFS = 0,
    /// How many bits we have reserved per channel.
    BITS_PER_CHANNEL = 6,
    /// This bit is true if the output port should be enabled (by the event
    /// consumer).
    NETWORK_ENABLE_OFS = 0,
    /// Whether the track is occupied, according to the last time it was
    /// enabled.
    NETWORK_OCCUPANCY_OFS = 1,
    /// Last known state of the overcurrent bit. TODO: implement
    NETWORK_OVERCURRENT_OFS = 2,
  };

  /// @return the bit 
  unsigned get_storage_base() {
    return CHANNEL_0_OFS + (channel_ * BITS_PER_CHANNEL);
  }
  
  StateFlowTimer timer_{this};
  uint8_t channel_;
  /// set-once when overcurrent is detected.
  uint8_t killedOutputDueToOvercurrent_ : 1;
  uint8_t sensedOccupancy_ : 1;          //< Current (debounced) sensor read
  uint8_t networkOvercurrent_ : 1;       //< Current network state
  uint8_t networkOccupancy_ : 1;         //< Current network state
  uint8_t commandedEnable_ : 1;          //< Current output pin state
  uint8_t networkEnable_ : 1;            //< Current input network state
  uint8_t networkOccupancyKnown_ : 1;    //< 1 if we've already produced it
  uint8_t networkOvercurrentKnown_ : 1;  //< 1 if we've already produced it
  /// sticky bit set to 1 when sensed occupancy switches to true
  uint8_t seenOccupancyOn_ : 1;
  /// sticky bit set to 1 when sensed occupancy switches to false
  uint8_t seenOccupancyOff_ : 1;
  /// 1 if the state flow is waiting in delay_normal state and can be notified
  /// to wake up.
  uint8_t isWaitingForLoop_ : 1;

  /*
  uint8_t reqOccupancyOn_ : 1;      //< produce event occ ON
  uint8_t reqOccupancyOff_ : 1;      //< produce event occ OFF
  uint8_t reqOvercurrentOn_ : 1;      //< produce event overcurrent ON
  uint8_t reqOvercurrentOff_ : 1;      //< produce event overcurrent OFF
  */

  /// how many tries we had yet for turning on output.
  uint8_t turnonTryCount_ : 3;
  openlcb::Node* node_;
  openlcb::EventId eventOccupancyOn_;
  openlcb::EventId eventOccupancyOff_;
  openlcb::EventId eventOvercurrentOn_;
  openlcb::EventId eventOvercurrentOff_;
  openlcb::EventId eventEnableOn_;
  openlcb::EventId eventEnableOff_;
  openlcb::WriteHelper h_;
  BarrierNotifiable n_;
  const DetectorOptions& opts_;
  openlcb::CallbackEventHandler eventHandler_;
};

}  // namespace bracz_custom

#endif  // _BRACZ_CUSTOM_DETECTORPORT_HXX_
