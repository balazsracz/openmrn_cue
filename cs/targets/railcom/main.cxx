/** \copyright
 * Copyright (c) 2013, Balazs Racz
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
 * \file main.cxx
 *
 * Main file for the io board application on the Tiva Launchpad board.
 *
 * @author Balazs Racz
 * @date 5 Jun 2015
 */

#include <functional>

#include "os/os.h"
#include "nmranet_config.h"

#include "dcc/RailcomBroadcastDecoder.hxx"
#include "dcc/RailcomHub.hxx"
#include "nmranet/ConfiguredConsumer.hxx"
#include "nmranet/ConfiguredProducer.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/MultiConfiguredConsumer.hxx"
#include "nmranet/SimpleStack.hxx"
#include "nmranet/TractionDefs.hxx"
#include "nmranet/CallbackEventHandler.hxx"

#include "config.hxx"
#include "custom/RailcomBroadcastFlow.hxx"
#include "custom/TivaDAC.hxx"
#include "custom/TivaGNDControl.hxx"
#include "freertos_drivers/common/BlinkerGPIO.hxx"
#include "freertos_drivers/ti/TivaGPIO.hxx"
#include "hardware.hxx"

extern TivaDAC<DACDefs> dac;

OVERRIDE_CONST(main_thread_stack_size, 2500);
extern const nmranet::NodeID NODE_ID = 0x050101011462ULL;
nmranet::SimpleCanStack stack(NODE_ID);

dcc::RailcomHubFlow railcom_hub(stack.service());

nmranet::ConfigDef cfg(0);

extern const char* const nmranet::CONFIG_FILENAME = "/dev/eeprom";
extern const char* const nmranet::SNIP_DYNAMIC_FILENAME =
    nmranet::CONFIG_FILENAME;
extern const size_t nmranet::CONFIG_FILE_SIZE =
    cfg.seg().size() + cfg.seg().offset();
static_assert(nmranet::CONFIG_FILE_SIZE <= 512, "Need to adjust eeprom size");

typedef BLINKER_Pin LED_RED_Pin;

nmranet::ConfiguredConsumer consumer_red(stack.node(),
                                         cfg.seg().consumers().entry<0>(),
                                         LED_RED_Pin());
nmranet::ConfiguredConsumer consumer_green(stack.node(),
                                           cfg.seg().consumers().entry<1>(),
                                           OUTPUT_EN0_Pin());
nmranet::ConfiguredConsumer consumer_blue(stack.node(),
                                          cfg.seg().consumers().entry<2>(),
                                          OUTPUT_EN1_Pin());

/*nmranet::ConfiguredConsumer consumer_shadow0(stack.node(),
                                           cfg.seg().consumers().entry<1>(),
                                           STAT5_Pin());

nmranet::ConfiguredConsumer consumer_shadow1(stack.node(),
                                           cfg.seg().consumers().entry<2>(),
                                           STAT4_Pin());*/

nmranet::ConfiguredProducer producer_sw1(stack.node(),
                                         cfg.seg().producers().entry<0>(),
                                         SW1_Pin());
nmranet::ConfiguredProducer producer_sw2(stack.node(),
                                         cfg.seg().producers().entry<1>(),
                                         SW2_Pin());

constexpr const Gpio* enable_ptrs[] = {
    OUTPUT_EN0_Pin::instance(), OUTPUT_EN1_Pin::instance(),
    OUTPUT_EN2_Pin::instance(), OUTPUT_EN3_Pin::instance(),
    OUTPUT_EN4_Pin::instance(), OUTPUT_EN5_Pin::instance(),
};

nmranet::MultiConfiguredConsumer consumer_enables(stack.node(), enable_ptrs,
                                                  ARRAYSIZE(enable_ptrs),
                                                  cfg.seg().enables());

nmranet::RefreshLoop loop(stack.node(),
                          {producer_sw1.polling(), producer_sw2.polling()});

template <class Debouncer>
class FeedbackBasedOccupancy : public dcc::RailcomHubPort {
 public:
  template <typename... Args>
  FeedbackBasedOccupancy(nmranet::Node* node, uint64_t event_base,
                         uint8_t channel, unsigned channel_count, Args... args)
      : dcc::RailcomHubPort(node->iface()),
        channel_(channel),
        currentValues_(0),
        eventHandler_(node, event_base, &currentValues_, channel_count) {
    for (unsigned i = 0; i < channel_count; ++i) {
      debouncers_.emplace_back(args...);
      debouncers_.back().initialize(false);
    }
  }

  Action entry() {
    if (message()->data()->channel != channel_) return release_and_exit();
    newPayload_ = message()->data()->ch1Data[0];
    nextOffset_ = 0;
    release();
    return call_immediately(STATE(consume_bit));
  }

  Action consume_bit() {
    while (nextOffset_ < eventHandler_.size()) {
      auto& debouncer = debouncers_[nextOffset_];
      bool last_state_deb = debouncer.current_state();
      bool last_state_network = eventHandler_.Get(nextOffset_);
      if (last_state_network != last_state_deb) {
        debouncer.override(last_state_network);
      }
      bool new_state_input = newPayload_ & (1 << nextOffset_);
      if (debouncer.update_state(new_state_input)) {
        // Need to produce new value to network.
        eventHandler_.Set(nextOffset_, debouncer.current_state(), &h_,
                          n_.reset(this));
        return wait_and_call(STATE(step_bit));
      }
      nextOffset_++;
    }
    return exit();
  }

  Action step_bit() {
    nextOffset_++;
    return call_immediately(STATE(consume_bit));
  }

 private:
  std::vector<Debouncer> debouncers_;
  uint8_t newPayload_;  //< Packed data that came with the input.
  uint8_t nextOffset_;  //< Which index we are evaluating now.
  uint8_t channel_;
  uint32_t currentValues_;
  nmranet::BitRangeEventPC eventHandler_;
  nmranet::WriteHelper h_;
  BarrierNotifiable n_;
};

void set_output_disable(unsigned port, bool enable) {
  extern unsigned* stat_led_ptr();
  unsigned* leds = stat_led_ptr();
  if (enable) {
    *leds &= ~(1 << port);
  } else {
    (*leds |= (1 << port));
  }
  switch (port) {
    case 0:
      return OUTPUT_EN0_Pin::set(enable);
    case 1:
      return OUTPUT_EN1_Pin::set(enable);
    case 2:
      return OUTPUT_EN2_Pin::set(enable);
    case 3:
      return OUTPUT_EN3_Pin::set(enable);
    case 4:
      return OUTPUT_EN4_Pin::set(enable);
    case 5:
      return OUTPUT_EN5_Pin::set(enable);
  }
  DIE("Setting unknown output_en port.");
}

namespace sp = std::placeholders;


/// TODO:
/// - add readout of event IDs from config
/// - add config itself
/// - register the event IDs
class PortLogic : public StateFlowBase {
 public:
  PortLogic(Node* node, uint8_t channel, int config_fd)
      : StateFlowBase(stack.service()),
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
            std::bind(&PortLogic::event_report, this, sp::_1, sp::_2, sp::_3),
            std::bind(&PortLogic::get_event_state, this, sp::_1, sp::_2)) {
    start_flow(STATE(init_wait));
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
      set_output_enable(false); // this should be redundant
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
  uint8_t reqEnable_ : 1;  //< 1 if a network request came in to enable the output.

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

template <class Debouncer>
class RailcomOccupancyDecoder : public dcc::RailcomHubPortInterface {
 public:
  /// Creates an occupancy decoder port.
  ///
  /// @param channel is the railcom channe to listen to. Usually 0xff for
  /// occupancy or 0xfe for overcurrent.
  /// @param channel_count tells how many bits are available to check.
  /// @param update_function will be called with the channel number and the
  /// channel's (debounced) value whenever the debounced value changes.
  /// @param args... are forwarded to the debouncer.
  template <typename... Args>
  RailcomOccupancyDecoder(uint8_t channel, unsigned channel_count,
                          std::function<void(unsigned, bool)> update_function,
                          Args... args)
      : updateFunction_(std::move(update_function)), channel_(channel) {
    for (unsigned i = 0; i < channel_count; ++i) {
      debouncers_.emplace_back(args...);
      debouncers_.back().initialize(false);
    }
  }

  void send(Buffer<dcc::RailcomHubData>* entry, unsigned prio) override {
    AutoReleaseBuffer<dcc::RailcomHubData> rb(entry);
    if (entry->data()->channel != channel_) return;
    uint8_t new_payload = entry->data()->ch1Data[0];
    for (unsigned i = 0; i < debouncers_.size(); ++i) {
      bool new_state_input = new_payload & (1 << i);
      auto& debouncer = debouncers_[i];
      if (debouncer.update_state(new_state_input)) {
        // Need to produce new value to network.
        updateFunction_(i, debouncer.current_state());
      }
    }
  }

 private:
  std::function<void(unsigned, bool)> updateFunction_;
  std::vector<Debouncer> debouncers_;
  uint8_t channel_;
};

template <class Debouncer>
class OvercurrentFlow : public dcc::RailcomHubPort {
 public:
  template <typename... Args>
  OvercurrentFlow(nmranet::Node* node, uint64_t event_base, uint8_t channel,
                  unsigned channel_count, Args... args)
      : dcc::RailcomHubPort(node->iface()),
        channel_(channel),
        currentValues_(0),
        eventHandler_(node, event_base, &currentValues_, channel_count) {
    for (unsigned i = 0; i < channel_count; ++i) {
      debouncers_.emplace_back(args...);
      debouncers_.back().initialize(false);
    }
  }

  Action entry() {
    if (message()->data()->channel != channel_) return release_and_exit();
    newPayload_ = message()->data()->ch1Data[0];
    nextOffset_ = 0;
    release();
    return call_immediately(STATE(consume_bit));
  }

  Action consume_bit() {
    while (nextOffset_ < eventHandler_.size()) {
      auto& debouncer = debouncers_[nextOffset_];
      bool last_state_deb = debouncer.current_state();
      bool last_state_network = eventHandler_.Get(nextOffset_);
      if (last_state_network != last_state_deb) {
        debouncer.override(last_state_network);
      }
      bool new_state_input = newPayload_ & (1 << nextOffset_);
      if (debouncer.update_state(new_state_input)) {
        // Need to produce new value to network.
        if (debouncer.current_state()) {
          // Overcurrent detected. Turn off output port.
          // OUTPUT_EN is inverted.
          set_output_disable(nextOffset_, true);
        }
        eventHandler_.Set(nextOffset_, debouncer.current_state(), &h_,
                          n_.reset(this));
        return wait_and_call(STATE(step_bit));
      }
      nextOffset_++;
    }
    return exit();
  }

  Action step_bit() {
    nextOffset_++;
    return call_immediately(STATE(consume_bit));
  }

 private:
  std::vector<Debouncer> debouncers_;
  uint8_t newPayload_;  //< Packed data that came with the input.
  uint8_t nextOffset_;  //< Which index we are evaluating now.
  uint8_t channel_;
  uint32_t currentValues_;
  nmranet::BitRangeEventPC eventHandler_;
  nmranet::WriteHelper h_;
  BarrierNotifiable n_;
};

uint8_t dac_next_packet_mode = 0;

uint8_t RailcomDefs::feedbackChannel_ = 0xff;

DacSettings dac_occupancy = {5, 50, true};  // 1.9 mV
// DacSettings dac_occupancy = { 5, 10, true };

#if 1
DacSettings dac_overcurrent = {5, 20, false};
#else
DacSettings dac_overcurrent = dac_occupancy;
#endif

#if 1
DacSettings dac_railcom = {5, 10, true};  // 8.6 mV
#else
DacSettings dac_railcom = dac_occupancy;
#endif

volatile uint32_t ch0_count, ch1_count, sample_count;

extern unsigned* stat_led_ptr();

class DACThread : public OSThread {
 public:
  void* entry() OVERRIDE {
    const int kPeriod = 300000;
    unsigned startup = 0;
    usleep(kPeriod * 2);
    while (true) {
      nmranet::WriteHelper h;
      /*      usleep(kPeriod);
      dac.set_div(true);
      STAT1_Pin::set(false);
      LED_GREEN_Pin::set(false);
      dac.set_pwm(3, 10);
      usleep(kPeriod);
      dac.set_div(false);
      STAT1_Pin::set(true);
      LED_GREEN_Pin::set(true);
      dac.set_pwm(7, 10);*/

      usleep(kPeriod);
      SyncNotifiable n;
      h.WriteAsync(
          stack.node(), nmranet::Defs::MTI_EVENT_REPORT, h.global(),
          nmranet::eventid_to_buffer(
              0xFE00000000000000ULL | ((sample_count & 0xffffULL) << 32) |
              ((ch0_count & 0xffff) << 16) | (ch1_count & 0xffff)),
          &n);
      ch0_count = 0;
      ch1_count = 0;
      sample_count = 0;
      n.wait_for_notification();

      if (startup < 6) {
        set_output_disable(startup, false);
        ++startup;
      }

      volatile unsigned* leds = stat_led_ptr();
      for (int i = 0; i < 6; ++i) {
        // enable pins are inverted
        if (enable_ptrs[i]->is_set()) {
          *leds &= ~(1 << i);
        } else {
          *leds |= (1 << i);
        }
      }
    }
    return nullptr;
  }
} dac_thread;

void read_dac_settings(int fd, nmranet::DacSettingsConfig cfg,
                       DacSettings* out) {
  uint16_t n = cfg.nominator().read(fd);
  uint16_t d = cfg.denom().read(fd);
  uint8_t div = cfg.divide().read(fd) ? true : false;
  if (n >= d || !(div == 0 || div == 1)) {
    // corrupted data. overwrite with default
    cfg.nominator().write(fd, out->nominator);
    cfg.denom().write(fd, out->denominator);
    cfg.divide().write(fd, out->divide ? 1 : 0);
    return;
  }
  out->nominator = n;
  out->denominator = d;
  out->divide = div;
}

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[]) {
  int fd = ::open(nmranet::CONFIG_FILENAME, O_RDWR);
  HASSERT(fd >= 0);
  read_dac_settings(fd, cfg.seg().current().dac_occupancy(), &dac_occupancy);
  read_dac_settings(fd, cfg.seg().current().dac_overcurrent(),
                    &dac_overcurrent);
  read_dac_settings(fd, cfg.seg().current().dac_railcom(), &dac_railcom);

  // default: 30, 10
  CountingDebouncer::Options occupancy_debouncer_opts{
      cfg.seg().current().occupancy().count_total().read(fd),
      cfg.seg().current().occupancy().min_active().read(fd)};

  static FeedbackBasedOccupancy<CountingDebouncer> occupancy_report(
      stack.node(), (NODE_ID << 16) | 0x6000, 0xff, RailcomDefs::CHANNEL_COUNT,
      occupancy_debouncer_opts);

  // default: 20 10
  CountingDebouncer::Options overcurrent_debouncer_opts{
      cfg.seg().current().overcurrent().count_total().read(fd),
      cfg.seg().current().overcurrent().min_active().read(fd)};

  static OvercurrentFlow<CountingDebouncer> overcurrent_report(
      stack.node(), (NODE_ID << 16) | 0x7000, 0xfe, RailcomDefs::CHANNEL_COUNT,
      overcurrent_debouncer_opts);

  static RailcomBroadcastFlow railcom_broadcast(
      &railcom_hub, stack.node(), &occupancy_report, &overcurrent_report,
      nullptr, 6);

  stack.add_can_port_select("/dev/can0");
  dac_thread.start("dac", 0, 600);
  dac.set_pwm(1, 18);
  dac.set_div(true);

  /*CHARLIE0_Pin::instance()->set_direction(Gpio::Direction::INPUT);
  CHARLIE1_Pin::instance()->set_direction(Gpio::Direction::INPUT);
  CHARLIE2_Pin::instance()->set_direction(Gpio::Direction::OUTPUT);
  CHARLIE2_Pin::set(false);*/

  *stat_led_ptr() = 0;
  set_output_disable(0, false);
  set_output_disable(1, false);

  // we need to enable the dcc receiving driver.
  ::open("/dev/nrz0", O_NONBLOCK | O_RDONLY);
  HubDeviceNonBlock<dcc::RailcomHubFlow> railcom_port(&railcom_hub,
                                                      "/dev/railcom");
  // occupancy info will be proxied by the broadcast decoder
  // railcom_hub.register_port(&occupancy_report);

  stack.loop_executor();
  return 0;
}
