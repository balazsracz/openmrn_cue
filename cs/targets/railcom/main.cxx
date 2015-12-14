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

#include "os/os.h"
#include "nmranet_config.h"

#include "dcc/RailcomBroadcastDecoder.hxx"
#include "dcc/RailcomHub.hxx"
#include "nmranet/ConfiguredConsumer.hxx"
#include "nmranet/ConfiguredProducer.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/SimpleStack.hxx"
#include "nmranet/TractionDefs.hxx"

#include "config.hxx"
#include "custom/TivaDAC.hxx"
#include "custom/TivaGNDControl.hxx"
#include "custom/RailcomBroadcastFlow.hxx"
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

void set_output_en(unsigned port, bool enable) {
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
          set_output_en(nextOffset_, true);
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

      unsigned* leds = stat_led_ptr();
      if (OUTPUT_EN0_Pin::get()) {
        *leds &= ~4;
      } else {
        *leds |= 4;
      }
      if (OUTPUT_EN1_Pin::get()) {
        *leds &= ~8;
      } else {
        *leds |= 8;
      }
      /**stat_led_ptr() <<= 1;
      if (*stat_led_ptr() >= (1<<6)) {
        *stat_led_ptr() = 1;
        }*/
    }
    return nullptr;
  }
} dac_thread;

void read_dac_settings(int fd, nmranet::DacSettingsConfig cfg, DacSettings* out) {
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
  set_output_en(0, false);
  set_output_en(1, false);

  // we need to enable the dcc receiving driver.
  ::open("/dev/nrz0", O_NONBLOCK | O_RDONLY);
  HubDeviceNonBlock<dcc::RailcomHubFlow> railcom_port(&railcom_hub,
                                                      "/dev/railcom");
  // occupancy info will be proxied by the broadcast decoder
  // railcom_hub.register_port(&occupancy_report);

  stack.loop_executor();
  return 0;
}
