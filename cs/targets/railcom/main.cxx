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

#define _DEFAULT_SOURCE // for usleep

#include <functional>

#include "os/os.h"
#include "nmranet_config.h"

#include "dcc/RailcomBroadcastDecoder.hxx"
#include "dcc/RailcomHub.hxx"
#include "openlcb/ConfiguredConsumer.hxx"
#include "openlcb/ConfiguredProducer.hxx"
#include "openlcb/EventHandlerTemplates.hxx"
#include "openlcb/MultiConfiguredConsumer.hxx"
#include "openlcb/SimpleStack.hxx"
#include "openlcb/TractionDefs.hxx"
#include "openlcb/CallbackEventHandler.hxx"

#include "config.hxx"
#include "commandstation/RailcomBroadcastFlow.hxx"
#include "custom/DetectorPort.hxx"
#include "custom/TivaDAC.hxx"
#include "custom/TivaGNDControl.hxx"
#include "freertos_drivers/common/BlinkerGPIO.hxx"
#include "freertos_drivers/ti/TivaGPIO.hxx"
#include "hardware.hxx"
#include "dcc/RailcomPortDebug.hxx"

#include "freertos_drivers/ti/TivaCpuLoad.hxx"
#include "utils/CpuDisplay.hxx"
#include "utils/stdio_logging.h"



extern TivaDAC<DACDefs> dac;

OVERRIDE_CONST(main_thread_stack_size, 2500);
extern const openlcb::NodeID __application_node_id;
openlcb::SimpleCanStack stack(__application_node_id);


/// Set this to 1 to display the CPU load using the RGB led (green/yellow/red).
#if 0

TivaCpuLoad<TivaCpuLoadDefHw> load_monitor;

extern "C" {
void timer4a_interrupt_handler(void)
{
    load_monitor.interrupt_handler(0);
}
}

CpuDisplay load_display(stack.service(), LED_RED_Pin::instance(), LED_GREEN_Pin::instance());

using HaveDccSignalPin = DummyPin;

#else
using HaveDccSignalPin = LED_GREEN_Pin;
#endif

dcc::RailcomHubFlow railcom_hub(stack.service());

openlcb::ConfigDef cfg(0);

extern const char* const openlcb::CONFIG_FILENAME = "/dev/eeprom";
extern const char* const openlcb::SNIP_DYNAMIC_FILENAME =
    openlcb::CONFIG_FILENAME;
extern const size_t openlcb::CONFIG_FILE_SIZE =
    cfg.seg().size() + cfg.seg().offset();
static_assert(openlcb::CONFIG_FILE_SIZE <= 1524, "Need to adjust eeprom size");

typedef BLINKER_Pin LED_RED_Pin;

/*openlcb::ConfiguredConsumer consumer_red(stack.node(),
                                         cfg.seg().consumers().entry<0>(),
                                         LED_RED_Pin());
openlcb::ConfiguredConsumer consumer_green(stack.node(),
                                           cfg.seg().consumers().entry<1>(),
                                           LED_GREEN_Pin());
openlcb::ConfiguredConsumer consumer_blue(stack.node(),
                                          cfg.seg().consumers().entry<2>(),
                                          LED_BLUE_Pin());*/

/*openlcb::ConfiguredConsumer consumer_shadow0(stack.node(),
                                           cfg.seg().consumers().entry<1>(),
                                           STAT5_Pin());

openlcb::ConfiguredConsumer consumer_shadow1(stack.node(),
                                           cfg.seg().consumers().entry<2>(),
                                           STAT4_Pin());*/

openlcb::ConfiguredProducer producer_sw1(stack.node(),
                                         cfg.seg().producers().entry<0>(),
                                         SW1_Pin());
openlcb::ConfiguredProducer producer_sw2(stack.node(),
                                         cfg.seg().producers().entry<1>(),
                                         SW2_Pin());

const Gpio* const enable_ptrs[] = {
    OUTPUT_EN0_Pin::instance(), OUTPUT_EN1_Pin::instance(),
    OUTPUT_EN2_Pin::instance(), OUTPUT_EN3_Pin::instance(),
    OUTPUT_EN4_Pin::instance(), OUTPUT_EN5_Pin::instance(),
};

/*openlcb::MultiConfiguredConsumer consumer_enables(stack.node(), enable_ptrs,
                                                  ARRAYSIZE(enable_ptrs),
                                                  cfg.seg().enables());*/

openlcb::RefreshLoop loop(stack.node(),
                          {producer_sw1.polling(), producer_sw2.polling()});

constexpr unsigned DELAY_LOG_COUNT = 512;
volatile int delay_log[DELAY_LOG_COUNT];
unsigned next_delay = 0;
uint32_t feedback_sample_overflow_count = 0;
std::vector<CountingDebouncer> RailcomDefs::debouncers_;
volatile bool RailcomDefs::enableOvercurrent_ = false;

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
    // This code measures the interrupt to processing latency. We get a timer
    // counter in the ch2Data, and we compare that with the current count value
    // of the same timer.
    int current_tick = RailcomDefs::get_timer_tick();
    int start_tick = 0;
    memcpy(&start_tick, entry->data()->ch2Data, 4);
    start_tick -= current_tick;
    delay_log[next_delay++] = start_tick;
    if (next_delay >= DELAY_LOG_COUNT) next_delay = 0;
  }

 private:
  std::function<void(unsigned, bool)> updateFunction_;
  std::vector<Debouncer> debouncers_;
  uint8_t channel_;
};


class DirectRailcomOccupancyDecoder : public dcc::RailcomHubPortInterface {
 public:
  /// Creates an occupancy decoder port.
  ///
  /// @param channel is the railcom channe to listen to. Usually 0xff for
  /// occupancy or 0xfe for overcurrent.
  /// @param channel_count tells how many bits are available to check.
  /// @param update_function will be called with the channel number and the
  /// channel's value.
  DirectRailcomOccupancyDecoder(uint8_t channel, unsigned channel_count,
                          std::function<void(unsigned, bool)> update_function)
      : updateFunction_(std::move(update_function)), channel_(channel), count_(channel_count) {
    
  }

  void send(Buffer<dcc::RailcomHubData>* entry, unsigned prio) override {
    AutoReleaseBuffer<dcc::RailcomHubData> rb(entry);
    if (entry->data()->channel != channel_) return;
    uint8_t new_payload = entry->data()->ch1Data[0];
    for (unsigned i = 0; i < count_; ++i) {
      bool new_state_input = new_payload & (1 << i);
      // Need to produce new value to network.
      updateFunction_(i, new_state_input);
    }
    int current_tick = RailcomDefs::get_timer_tick();
    int start_tick = 0;
    memcpy(&start_tick, entry->data()->ch2Data, 4);
    start_tick -= current_tick;
    delay_log[next_delay++] = start_tick;
    if (next_delay >= DELAY_LOG_COUNT) next_delay = 0;
  }

 private:
  std::function<void(unsigned, bool)> updateFunction_;
  uint8_t channel_;
  uint8_t count_;
};



extern unsigned* stat_led_ptr();
extern volatile uint32_t ch0_count, ch1_count, sample_count;
extern DacSettings dac_occupancy, dac_overcurrent, dac_railcom;

class DACThread : public OSThread {
 public:
  void* entry() OVERRIDE {
    const int kPeriod = 300000;
    usleep(kPeriod * 2);
    while (true) {
      openlcb::WriteHelper h;
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
      bool send = false;
      openlcb::EventId ev = 0xFE00000000000000ULL |
                            ((sample_count & 0xffffULL) << 32) |
                            ((ch0_count & 0xffff) << 16) | (ch1_count & 0xffff);
      ch0_count = 0;
      ch1_count = 0;
      sample_count = 0;

      if (send) {
        h.WriteAsync(stack.node(), openlcb::Defs::MTI_EVENT_REPORT, h.global(),
                     openlcb::eventid_to_buffer(ev), &n);
        n.wait_for_notification();
      }
      LED_BLUE_Pin::set(false);

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

// Enable this if you want all DCC packets to be printed to the debug USB.
#if 0

#include "dcc/DccDebug.hxx"
#include "utils/StringPrintf.hxx"
#include "utils/FdUtils.hxx"

#define HAVE_DECODER_THREAD

class DecoderThread : public OSThread
{
public:
    DecoderThread()
    {
    }

    void *entry() override
    {
        setblink(0);
        int fd = ::open("/dev/nrz0", O_RDONLY);
        HASSERT(fd >= 0);
        // int wfd = ::open("/dev/serUSB0", O_RDWR);
        int wfd = ::open("/dev/ser0", O_RDWR);
        HASSERT(wfd >= 0);
        string msg = "Hello worlddd!";
        ::write(wfd, msg.data(), msg.size());

        int cnt = 0;
        while (1)
        {
            DCCPacket packet_data;
            int sret = ::read(fd, &packet_data, sizeof(packet_data));
            HASSERT(sret == sizeof(packet_data));
#if 1
            long long t = os_get_time_monotonic();
            string txt = StringPrintf("\n%02d.%06d %04d ",
                (unsigned)((t / 1000000000) % 100),
                (unsigned)((t / 1000) % 1000000), cnt % 10000);
            ::write(wfd, txt.data(), txt.size());
            txt = dcc::packet_to_string(packet_data);
            FdUtils::repeated_write(wfd, txt.data(), txt.size());
            //::write(wfd, txt.data(), txt.size());
            // we purposefully do not check the return value, because if there
            // was not enough space in the serial write buffer, we need to throw
            // away data.
#endif
            ++cnt;
            resetblink((cnt >> 3) & 1);
        }
    }

} dcc_test_thread;

#endif

bracz_custom::DetectorPort* pports = nullptr;

class WatchForDccSignal : public StateFlowBase {
 public:
  WatchForDccSignal() : StateFlowBase(stack.service()) {
    start_flow(STATE(sleep_signal));
  }

 private:
  Action sleep_signal() {
    return sleep_and_call(&timer_, MSEC_TO_NSEC(5), STATE(evaluate));
  }

  Action evaluate() {
    unsigned current_sample_count = DCCDecode::sampleCount_;
    if (current_sample_count >= lastSampleCount_) {
      unsigned diff = current_sample_count - lastSampleCount_;
      constexpr auto num_channel = cfg.seg().detectors().num_repeats();
      if (diff >= SAMPLE_THRESHOLD) {
        HaveDccSignalPin::set(true);
        for (unsigned i = 0; i < num_channel; ++i) {
          pports[i].set_have_dcc_signal(true);
        }
      } else {
        for (unsigned i = 0; i < num_channel; ++i) {
          pports[i].set_have_dcc_signal(false);
        }
        HaveDccSignalPin::set(false);
      }
    }

    lastSampleCount_ = current_sample_count;
    return call_immediately(STATE(sleep_signal));
  }

  /// How many samples we should be seeing in a sleep period in order to know
  /// that DCC signal is active. A sleep period is 5 msec, of which we can have
  /// 1.5 msec as marklin preamble then 272 usec per bit, which gives 25 cap
  /// events. This won't work for zero stretching DCC though.
  static constexpr unsigned SAMPLE_THRESHOLD = 20;
  /// last seen value of the counter of all samples of the DCC signal.
  unsigned lastSampleCount_{0};
  StateFlowTimer timer_{this};
} g_wait_for_dcc_signal;

// Sets the text fields in the config file upon factory reset.
class FactoryResetHelper : public DefaultConfigUpdateListener {
public:
    UpdateAction apply_configuration(int fd, bool initial_load,
                                     BarrierNotifiable *done) OVERRIDE {
        AutoNotify n(done);

        for (unsigned i = 0; i < 6; ++i) {
          const auto& grp = cfg.seg().detectors().entry(i);
          string pname = grp.name().read(fd);
          maybe_update_string(fd, grp.occupancy().name(), pname + " occupancy");
          maybe_update_string(fd, grp.overcurrent().name(), pname + " short");
          maybe_update_string(fd, grp.enable().name(), pname + " enable");
        }

        read_dac_settings(fd, cfg.dev().current().dac_occupancy(),
                          &dac_occupancy);
        read_dac_settings(fd, cfg.dev().current().dac_overcurrent(),
                          &dac_overcurrent);
        read_dac_settings(fd, cfg.dev().current().dac_railcom(), &dac_railcom);

        return UPDATED;
    }

    void factory_reset(int fd) override {
      cfg.userinfo().name().write(fd, "RailCom Power Manager");
      cfg.userinfo().description().write(
          fd, "Tiva 123 + RailCom + short circuit detector.");
      /// @todo add name to input and button
      // cfg.seg().in
      for (unsigned i = 0; i < 6; ++i) {
        cfg.seg().detectors().entry(i).name().write(
            fd, string("Port ") + integer_to_string(i + 1));
      }
      cfg.dev().current().occupancy().count_total().write(fd, 30);
      cfg.dev().current().occupancy().min_active().write(fd, 10);
      cfg.dev().current().overcurrent().count_total().write(fd, 20);
      cfg.dev().current().overcurrent().min_active().write(fd, 10);

      dac_overcurrent = {5, 20, false};
      dac_railcom = {5, 10, true};  // 8.6 mV
      dac_occupancy = {5, 50, true};  // 1.9 mV
      // This will write back the default settings.
      cfg.dev().current().dac_occupancy().divide().write(fd, 255);
      cfg.dev().current().dac_overcurrent().divide().write(fd, 255);
      cfg.dev().current().dac_railcom().divide().write(fd, 255);
      read_dac_settings(fd, cfg.dev().current().dac_occupancy(),
                        &dac_occupancy);
      read_dac_settings(fd, cfg.dev().current().dac_overcurrent(),
                        &dac_overcurrent);
      read_dac_settings(fd, cfg.dev().current().dac_railcom(), &dac_railcom);
    }

   private:
    void read_dac_settings(int fd, const openlcb::DacSettingsConfig &cfg,
                           DacSettings* out) {
      uint16_t n = cfg.nominator().read(fd);
      uint16_t d = cfg.denom().read(fd);
      uint8_t div = cfg.divide().read(fd);
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

  /// Checks a value in the config storage, and if it does not match the
  /// expected, overwrites it.
  template<unsigned N>
  void maybe_update_string(int fd, const openlcb::StringConfigEntry<N>& e,
                           string value) {
    string old = e.read(fd);
    if (old != value) {
      e.write(fd, value);
    }
  }

} factory_reset_helper;

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[]) {
  LOG(ALWAYS, "hello world");

  static bracz_custom::DetectorOptions opts(cfg.dev().detector_options(), 6);
    
  LED_BLUE_Pin::set(false);
  int fd = stack.check_version_and_factory_reset(cfg.dev().internal(),
                                                 openlcb::EXPECTED_VERSION);
  HASSERT(fd >= 0);

  // default: 30, 10
  CountingDebouncer::Options occupancy_debouncer_opts{
      cfg.dev().current().occupancy().count_total().read(fd),
      cfg.dev().current().occupancy().min_active().read(fd)};

  // default: 20 10
  CountingDebouncer::Options overcurrent_debouncer_opts{
      cfg.dev().current().overcurrent().count_total().read(fd),
      cfg.dev().current().overcurrent().min_active().read(fd)};

  static bracz_custom::DetectorPort ports[6] = {
      {stack.node(), 0, fd, cfg.seg().detectors().entry<0>(), opts},
      {stack.node(), 1, fd, cfg.seg().detectors().entry<1>(), opts},
      {stack.node(), 2, fd, cfg.seg().detectors().entry<2>(), opts},
      {stack.node(), 3, fd, cfg.seg().detectors().entry<3>(), opts},
      {stack.node(), 4, fd, cfg.seg().detectors().entry<4>(), opts},
      {stack.node(), 5, fd, cfg.seg().detectors().entry<5>(), opts},
  };

  pports = ports;

  auto occupancy_proxy =
      [](unsigned ch, bool value) { pports[ch].set_occupancy(value); };
  auto overcurrent_proxy =
      [](unsigned ch, bool value) { pports[ch].set_overcurrent(value); };

  static RailcomOccupancyDecoder<CountingDebouncer> occ_decoder(
      0xff, 6, occupancy_proxy, occupancy_debouncer_opts);
  
  static DirectRailcomOccupancyDecoder over_decoder(
      0xfe, 6, overcurrent_proxy);

  RailcomDefs::setup_debouncer_opts(overcurrent_debouncer_opts);
  
  static RailcomBroadcastFlow railcom_broadcast(
      &railcom_hub, stack.node(), &occ_decoder, &over_decoder, nullptr, 6);

  stack.add_can_port_select("/dev/can0");
  dac_thread.start("dac", 0, 600);
  dac.set_pwm(1, 18);
  dac.set_div(true);

  *stat_led_ptr() = 0;

  HubDeviceNonBlock<dcc::RailcomHubFlow> railcom_port(&railcom_hub,
                                                      "/dev/railcom");
  // we need to enable the dcc receiving driver, but only AFTER the railcom
  // driver was opened.
  ::open("/dev/nrz0", O_NONBLOCK | O_RDONLY);
#ifdef HAVE_DECODER_THREAD
  dcc_test_thread.start("dcc_printer", 0, 2048);
#endif  

  DCC_IN_Pin::set_hw();

  // Uncomment this line to print all railcom packets to the LCC bus using a
  // non-standard message. This is a lot of traffic, so only useful for
  // debugging.
  //
  // openlcb::RailcomToOpenLCBDebugProxy debugproxy(&railcom_hub, stack.node(), nullptr);

  stack.loop_executor();
  return 0;
}
