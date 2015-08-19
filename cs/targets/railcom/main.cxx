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

#include "nmranet/SimpleStack.hxx"
#include "nmranet/ConfiguredConsumer.hxx"
#include "nmranet/ConfiguredProducer.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "dcc/RailcomHub.hxx"

#include "freertos_drivers/ti/TivaGPIO.hxx"
#include "freertos_drivers/common/BlinkerGPIO.hxx"
#include "config.hxx"
#include "hardware.hxx"
#include "custom/TivaGNDControl.hxx"
#include "custom/TivaDAC.hxx"

extern TivaDAC<DACDefs> dac;

OVERRIDE_CONST(main_thread_stack_size, 2500);
extern const nmranet::NodeID NODE_ID = 0x050101011462ULL;
nmranet::SimpleCanStack stack(NODE_ID);

dcc::RailcomHubFlow railcom_hub(stack.service());

nmranet::ConfigDef cfg(0);

extern const char *const nmranet::CONFIG_FILENAME = "/dev/eeprom";
extern const char *const nmranet::SNIP_DYNAMIC_FILENAME =
    nmranet::CONFIG_FILENAME;
extern const size_t nmranet::CONFIG_FILE_SIZE =
    cfg.seg().size() + cfg.seg().offset();
static_assert(nmranet::CONFIG_FILE_SIZE <= 256, "Need to adjust eeprom size");

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

nmranet::ConfiguredProducer producer_sw1(stack.node(),
                                         cfg.seg().producers().entry<0>(),
                                         SW1_Pin());
nmranet::ConfiguredProducer producer_sw2(stack.node(),
                                         cfg.seg().producers().entry<1>(),
                                         SW2_Pin());

nmranet::RefreshLoop loop(stack.node(),
                          {producer_sw1.polling(), producer_sw2.polling()});



class FeedbackBasedOccupancy : public dcc::RailcomHubPort {
 public:
  FeedbackBasedOccupancy(nmranet::Node* node, uint64_t event_base,
                         unsigned channel_count)
      : dcc::RailcomHubPort(node->interface()),
        currentValues_(0),
        eventHandler_(node, event_base, &currentValues_, channel_count) {}

  Action entry() {
    if (message()->data()->channel != 0xff) return release_and_exit();
    uint32_t new_values = message()->data()->ch1Data[0];
    release();
    if (new_values == currentValues_) return exit();
    unsigned bit = 1;
    unsigned ofs = 0;
    uint32_t diff = new_values ^ currentValues_;
    while (bit && ((diff & bit) == 0)) {
      bit <<= 1;
      ofs++;
    }
    eventHandler_.Set(ofs, (new_values & bit), &h_, n_.reset(this));
    return wait_and_call(STATE(set_done));
  }

  Action set_done() { return exit(); }

 private:
  uint32_t currentValues_;
  nmranet::BitRangeEventPC eventHandler_;
  nmranet::WriteHelper h_;
  BarrierNotifiable n_;
};

FeedbackBasedOccupancy occupancy_report(stack.node(), (NODE_ID << 16) | 0x6000,
                                        RailcomDefs::CHANNEL_COUNT);

class DACThread : public OSThread {
 public:
  void *entry() OVERRIDE {
    const int kPeriod = 1000000;
    while (true) {
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
      //LED_GREEN_Pin::set(false);
      //OUTPUT_EN0_Pin::set(false);
      usleep(kPeriod);
      //LED_GREEN_Pin::set(true);
      // OUTPUT_EN0_Pin::set(true);
    }
    return nullptr;
  }
} dac_thread;

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[]) {
  stack.add_can_port_select("/dev/can0");
  dac_thread.start("dac", 0, 600);
  dac.set_pwm(1, 10);
  dac.set_div(true);

  // we need to enable the dcc receiving driver.
  ::open("/dev/nrz0", O_NONBLOCK | O_RDONLY);
  HubDeviceNonBlock<dcc::RailcomHubFlow> railcom_port(&railcom_hub, "/dev/railcom");
  railcom_hub.register_port(&occupancy_report);

  stack.loop_executor();
  return 0;
}
