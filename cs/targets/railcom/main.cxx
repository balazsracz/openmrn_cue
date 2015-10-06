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
#include "nmranet/TractionDefs.hxx"
#include "dcc/RailcomHub.hxx"
#include "dcc/RailcomBroadcastDecoder.hxx"

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

nmranet::ConfiguredConsumer consumer_shadow0(stack.node(),
                                           cfg.seg().consumers().entry<1>(),
                                           STAT5_Pin());

nmranet::ConfiguredConsumer consumer_shadow1(stack.node(),
                                           cfg.seg().consumers().entry<2>(),
                                           STAT4_Pin());

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
                         uint8_t channel, unsigned channel_count)
      : dcc::RailcomHubPort(node->interface()),
        currentValues_(0),
        eventHandler_(node, event_base, &currentValues_, channel_count),
        channel_(channel) {}

  Action entry() {
    if (message()->data()->channel != channel_) return release_and_exit();
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
  uint8_t channel_;
};

FeedbackBasedOccupancy occupancy_report(stack.node(), (NODE_ID << 16) | 0x6000,
                                        0xff, RailcomDefs::CHANNEL_COUNT);

FeedbackBasedOccupancy overcurrent_report(stack.node(), (NODE_ID << 16) | 0x7000,
                                        0xfe, RailcomDefs::CHANNEL_COUNT);

class RailcomBroadcastFlow : public dcc::RailcomHubPort {
 public:
  RailcomBroadcastFlow(dcc::RailcomHubFlow* parent, nmranet::Node* node,
                       dcc::RailcomHubPort* occupancy_port,
                       dcc::RailcomHubPort* overcurrent_port,
                       dcc::RailcomHubPort* debug_port, unsigned channel_count)
      : dcc::RailcomHubPort(parent->service()),
        parent_(parent),
        node_(node),
        occupancyPort_(occupancy_port),
        overcurrentPort_(overcurrent_port),
        debugPort_(debug_port),
        size_(channel_count),
        channels_(new dcc::RailcomBroadcastDecoder[channel_count]) {
    parent_->register_port(this);
  }

  ~RailcomBroadcastFlow() {
    parent_->unregister_port(this);
    delete[] channels_;
  }

  Action entry() {
    auto channel = message()->data()->channel;
    if (channel == 0xff) {
      uint32_t new_values = message()->data()->ch1Data[0];
      for (unsigned i = 0; i < size_; ++i) {
        channels_[i].set_occupancy(new_values & (1 << i));
        auto& decoder = channels_[i];
        if (decoder.current_address() != decoder.lastAddress_) {
          // We need to send a fake railcom packet to ourselves to allow the
          // channel empty events to be generated.
          auto* b = alloc();
          b->data()->reset(0);
          b->data()->channel = i;
          send(b);
        }
      }
      if (occupancyPort_) {
        occupancyPort_->send(transfer_message());
      } else {
        release();
      }
      return exit();
    }
    if (channel == 0xfe) {
      if (overcurrentPort_) {
        overcurrentPort_->send(transfer_message());
      } else {
        release();
      }
      return exit();
    }
    if (channel >= size_ ||
        !channels_[channel].process_packet(*message()->data())) {
      if (debugPort_) {
        debugPort_->send(transfer_message());
      } else {
        release();
      }
      return exit();
    }
    auto& decoder = channels_[channel];
    if (decoder.current_address() == decoder.lastAddress_) {
      return release_and_exit();
    }
    // Now: the visible address has changed. Send event reports.
    return allocate_and_call(node_->interface()->global_message_write_flow(),
                             STATE(send_invalid));
  }

  Action send_invalid() {
    auto channel = message()->data()->channel;
    auto& decoder = channels_[channel];
    auto* b = get_allocation_result(node_->interface()->global_message_write_flow());
    b->data()->reset(
        nmranet::Defs::MTI_PRODUCER_IDENTIFIED_INVALID, node_->node_id(),
        nmranet::eventid_to_buffer(address_to_eventid(channel, decoder.lastAddress_)));
    b->set_done(n_.reset(this));
    node_->interface()->global_message_write_flow()->send(b);
    return wait_and_call(STATE(allocate_for_event));
  }

  Action allocate_for_event() {
    return allocate_and_call(node_->interface()->global_message_write_flow(),
                             STATE(send_event));
  }

  Action send_event() {
    auto channel = message()->data()->channel;
    auto& decoder = channels_[channel];
    auto* b = get_allocation_result(node_->interface()->global_message_write_flow());
    b->data()->reset(nmranet::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     nmranet::eventid_to_buffer(address_to_eventid(
                         channel, decoder.current_address())));
    b->set_done(n_.reset(this));
    node_->interface()->global_message_write_flow()->send(b);
    decoder.lastAddress_ = decoder.current_address();
    release();
    return wait_and_call(STATE(finish));
  }

  Action finish() { return exit(); }

 private:
  static const uint64_t FEEDBACK_EVENTID_BASE;
  uint64_t address_to_eventid(unsigned channel, uint16_t address) {
    uint64_t ret = FEEDBACK_EVENTID_BASE;
    ret |= uint64_t(channel & 0xff) << 48;
    ret |= address;
    return ret;
  }

  dcc::RailcomHubFlow* parent_;
  nmranet::Node* node_;
  dcc::RailcomHubPort* occupancyPort_;
  dcc::RailcomHubPort* overcurrentPort_;
  dcc::RailcomHubPort* debugPort_;
  unsigned size_;
  dcc::RailcomBroadcastDecoder* channels_;
  BarrierNotifiable n_;
};

const uint64_t RailcomBroadcastFlow::FEEDBACK_EVENTID_BASE =
    (0x09ULL << 56) | nmranet::TractionDefs::NODE_ID_DCC;


RailcomBroadcastFlow railcom_broadcast(&railcom_hub, stack.node(),
                                       &occupancy_report,
                                       &overcurrent_report,
                                       nullptr, 6);

bool dac_next_packet_occupancy = false;

uint8_t RailcomDefs::feedbackChannel_ = 0xff;

DacSettings dac_occupancy = { 5, 50, true };  // 1.9 mV
//DacSettings dac_occupancy = { 5, 10, true };

#if 0
DacSettings dac_overcurrent = { 1, 3, false };
#else
DacSettings dac_overcurrent = dac_occupancy;
#endif

#if 1
DacSettings dac_railcom = { 5, 10, true };  // 8.6 mV
#else
DacSettings dac_railcom = dac_occupancy;
#endif

volatile uint32_t ch0_count, ch1_count;

class DACThread : public OSThread {
 public:
  void *entry() OVERRIDE {
    const int kPeriod = 1000000;
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
      h.WriteAsync(stack.node(), nmranet::Defs::MTI_EVENT_REPORT, h.global(), nmranet::eventid_to_buffer(0x0501010100000000ULL + (((ch0_count & 0xffff) << 16) + (ch1_count & 0xffff))), &n);
      ch0_count = 0;
      ch1_count = 0;
      n.wait_for_notification();
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
  dac.set_pwm(1, 18);
  dac.set_div(true);

  OUTPUT_EN0_Pin::set(false);
  STAT5_Pin::set(false);

  STAT4_Pin::set(true);
  OUTPUT_EN1_Pin::set(false);

  STAT3_Pin::set(true);

  // we need to enable the dcc receiving driver.
  ::open("/dev/nrz0", O_NONBLOCK | O_RDONLY);
  HubDeviceNonBlock<dcc::RailcomHubFlow> railcom_port(&railcom_hub, "/dev/railcom");
  // occupancy info will be proxied by the broadcast decoder
  //railcom_hub.register_port(&occupancy_report);

  stack.loop_executor();
  return 0;
}
