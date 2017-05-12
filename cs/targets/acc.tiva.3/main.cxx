/** \copyright
 * Copyright (c) 2014, Balazs Racz
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
 * Applicaiton main file for the "production" universal accessory decoder.
 *
 * @author Balazs Racz
 * @date 12 Jul 2014
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "os/os.h"
#include "utils/GridConnectHub.hxx"
#include "utils/blinker.h"
#include "executor/Executor.hxx"
#include "can_frame.h"
#include "nmranet_config.h"

#include "utils/GcTcpHub.hxx"
#include "utils/HubDevice.hxx"
#include "utils/HubDeviceNonBlock.hxx"
#include "utils/HubDeviceSelect.hxx"
#include "dcc/RailcomBroadcastDecoder.hxx"
#include "openlcb/IfCan.hxx"
#include "openlcb/Defs.hxx"
#include "openlcb/AliasAllocator.hxx"
#include "openlcb/EventService.hxx"
#include "openlcb/EventHandlerTemplates.hxx"
#include "openlcb/PolledProducer.hxx"
#include "openlcb/RefreshLoop.hxx"
#include "openlcb/DatagramCan.hxx"
#include "openlcb/MemoryConfig.hxx"
#include "openlcb/NodeInitializeFlow.hxx"
#include "openlcb/TractionDefs.hxx"
#include "utils/Debouncer.hxx"
#include "openlcb/DefaultNode.hxx"
#include "driverlib/gpio.h"
#include "inc/hw_memmap.h"

#define ENABLE_TIVA_SIGNAL_DRIVER
#define TIVADCC_TIVA

#include "custom/TivaSignalPacket.hxx"
#include "custom/SignalLoop.hxx"
#include "custom/SignalServer.hxx"
#include "dcc/RailCom.hxx"
#include "dcc/Packet.hxx"
#include "hardware.hxx"
#include "TivaNRZ.hxx"
#include "dcc/Receiver.hxx"
#include "SimpleLog.hxx"
#include "custom/WiiChuckThrottle.hxx"
#include "custom/WiiChuckReader.hxx"
#include "custom/TivaGPIOConsumer.hxx"
#include "custom/TivaGPIOProducerBit.hxx"
#include "custom/LoggingBit.hxx"
#include "openlcb/DccDebugFlow.hxx"
#include "openlcb/SimpleNodeInfoMockUserFile.hxx"


extern const openlcb::NodeID NODE_ID;
openlcb::SimpleCanStack stack(NODE_ID);

openlcb::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char *const openlcb::SNIP_DYNAMIC_FILENAME = openlcb::MockSNIPUserFile::snip_user_file_path;

OVERRIDE_CONST(main_thread_stack_size, 2048);

#ifdef USE_WII_CHUCK
bracz_custom::WiiChuckThrottle wii_throttle(stack.node(), 0x060100000000ULL | 51);
#endif

bracz_custom::TivaSignalPacketSender g_signalbus(stack.service(), 15625,
                                                 SYSCTL_PERIPH_UART1,
                                                 UART1_BASE,
                                                 INT_RESOLVE(INT_UART1_, 0));

static const uint64_t R_EVENT_ID =
    0x0501010114FF2000ULL | ((NODE_ID & 0xf) << 8);

LoggingBit led_red_bit(R_EVENT_ID + 0, R_EVENT_ID + 1, nullptr);
openlcb::BitEventConsumer red_consumer(&led_red_bit);
TivaGPIOConsumer led_yellow(R_EVENT_ID + 2, R_EVENT_ID + 3,
                            LED_YELLOW_Pin::GPIO_BASE,
                            LED_YELLOW_Pin::GPIO_PIN);
TivaGPIOConsumer led_green(R_EVENT_ID + 4, R_EVENT_ID + 5,
                           LED_GREEN_Pin::GPIO_BASE, LED_GREEN_Pin::GPIO_PIN);
TivaGPIOConsumer led_blue(R_EVENT_ID + 6, R_EVENT_ID + 7,
                          LED_BLUE_Pin::GPIO_BASE, LED_BLUE_Pin::GPIO_PIN);
TivaGPIOConsumer led_bluesw(R_EVENT_ID + 8, R_EVENT_ID + 9,
                            LED_BLUE_SW_Pin::GPIO_BASE,
                            LED_BLUE_SW_Pin::GPIO_PIN);
TivaGPIOConsumer led_goldsw(R_EVENT_ID + 10, R_EVENT_ID + 11,
                            LED_GOLD_SW_Pin::GPIO_BASE,
                            LED_GOLD_SW_Pin::GPIO_PIN);

#ifdef HAVE_RELAYS
TivaGPIOConsumer rel1(R_EVENT_ID + 17, R_EVENT_ID + 16, REL0_Pin::GPIO_BASE, REL0_Pin::GPIO_PIN);
TivaGPIOConsumer rel2(R_EVENT_ID + 19, R_EVENT_ID + 18, REL1_Pin::GPIO_BASE, REL1_Pin::GPIO_PIN);
TivaGPIOConsumer rel3(R_EVENT_ID + 21, R_EVENT_ID + 20, REL2_Pin::GPIO_BASE, REL2_Pin::GPIO_PIN);
TivaGPIOConsumer rel4(R_EVENT_ID + 23, R_EVENT_ID + 22, REL3_Pin::GPIO_BASE, REL3_Pin::GPIO_PIN);
#endif

TivaGPIOConsumer out0(R_EVENT_ID + 32 + 0, R_EVENT_ID + 33 + 0, OUT0_Pin::GPIO_BASE, OUT0_Pin::GPIO_PIN);
TivaGPIOConsumer out1(R_EVENT_ID + 32 + 2, R_EVENT_ID + 33 + 2, OUT1_Pin::GPIO_BASE, OUT1_Pin::GPIO_PIN);
TivaGPIOConsumer out2(R_EVENT_ID + 32 + 4, R_EVENT_ID + 33 + 4, OUT2_Pin::GPIO_BASE, OUT2_Pin::GPIO_PIN);
TivaGPIOConsumer out3(R_EVENT_ID + 32 + 6, R_EVENT_ID + 33 + 6, OUT3_Pin::GPIO_BASE, OUT3_Pin::GPIO_PIN);
TivaGPIOConsumer out4(R_EVENT_ID + 32 + 8, R_EVENT_ID + 33 + 8, OUT4_Pin::GPIO_BASE, OUT4_Pin::GPIO_PIN);
TivaGPIOConsumer out5(R_EVENT_ID + 32 + 10, R_EVENT_ID + 33 + 10,
                      OUT5_Pin::GPIO_BASE, OUT5_Pin::GPIO_PIN);
TivaGPIOConsumer out6(R_EVENT_ID + 32 + 12, R_EVENT_ID + 33 + 12,
                      OUT6_Pin::GPIO_BASE, OUT6_Pin::GPIO_PIN);
TivaGPIOConsumer out7(R_EVENT_ID + 32 + 14, R_EVENT_ID + 33 + 14,
                      OUT7_Pin::GPIO_BASE, OUT7_Pin::GPIO_PIN);

typedef openlcb::PolledProducer<QuiesceDebouncer, TivaGPIOProducerBit>
    TivaGPIOProducer;
QuiesceDebouncer::Options opts(8);

TivaGPIOProducer in0(opts, R_EVENT_ID + 48 + 0, R_EVENT_ID + 49 + 0,
                     IN0_Pin::GPIO_BASE, IN0_Pin::GPIO_PIN);
TivaGPIOProducer in1(opts, R_EVENT_ID + 48 + 2, R_EVENT_ID + 49 + 2,
                     IN1_Pin::GPIO_BASE, IN1_Pin::GPIO_PIN);
TivaGPIOProducer in2(opts, R_EVENT_ID + 48 + 4, R_EVENT_ID + 49 + 4,
                     IN2_Pin::GPIO_BASE, IN2_Pin::GPIO_PIN);
TivaGPIOProducer in3(opts, R_EVENT_ID + 48 + 6, R_EVENT_ID + 49 + 6,
                     IN3_Pin::GPIO_BASE, IN3_Pin::GPIO_PIN);
TivaGPIOProducer in4(opts, R_EVENT_ID + 48 + 8, R_EVENT_ID + 49 + 8,
                     IN4_Pin::GPIO_BASE, IN4_Pin::GPIO_PIN);
TivaGPIOProducer in5(opts, R_EVENT_ID + 48 + 10, R_EVENT_ID + 49 + 10,
                     IN5_Pin::GPIO_BASE, IN5_Pin::GPIO_PIN);
TivaGPIOProducer in6(opts, R_EVENT_ID + 48 + 12, R_EVENT_ID + 49 + 12,
                     IN6_Pin::GPIO_BASE, IN6_Pin::GPIO_PIN);
TivaGPIOProducer in7(opts, R_EVENT_ID + 48 + 14, R_EVENT_ID + 49 + 14,
                     IN7_Pin::GPIO_BASE, IN7_Pin::GPIO_PIN);

openlcb::RefreshLoop loop(stack.node(),
                          {&in0, &in1, &in2, &in3, &in4, &in5, &in6, &in7});

#define NUM_SIGNALS 32

bracz_custom::SignalLoop signal_loop(&g_signalbus, stack.node(),
                                     (R_EVENT_ID & ~0xffffff) |
                                     ((R_EVENT_ID & 0xff00) << 8),
                                     NUM_SIGNALS);

bracz_custom::SignalServer signal_server(stack.dg_service(), stack.node(),
                                         &g_signalbus);

openlcb::DccPacketDebugFlow g_packet_debug_flow(stack.node());
openlcb::DccDebugDecodeFlow* g_decode_flow;

typedef StructContainer<dcc::Feedback> RailcomHubContainer;
typedef HubContainer<RailcomHubContainer> RailcomHubData;
typedef FlowInterface<Buffer<RailcomHubData>> RailcomHubPortInterface;
typedef StateFlow<Buffer<RailcomHubData>, QList<1>> RailcomHubPort;
typedef GenericHubFlow<RailcomHubData> RailcomHubFlow;

RailcomHubFlow railcom_hub(stack.service());

namespace openlcb {
class RailcomProxy : public RailcomHubPort {
 public:
  RailcomProxy(RailcomHubFlow* parent, Node* node,
               RailcomHubPort* occupancy_port)
      : RailcomHubPort(parent->service()),
        parent_(parent),
        node_(node),
        occupancyPort_(occupancy_port) {
    // parent_->register_port(this);
  }

  ~RailcomProxy() {  // parent_->unregister_port(this);
  }

  Action entry() {
    if (message()->data()->channel == 0xff && occupancyPort_) {
      occupancyPort_->send(transfer_message());
      return exit();
    }
    if (message()->data()->ch1Size) {
      return allocate_and_call(node_->iface()->global_message_write_flow(),
                               STATE(ch1_msg_allocated));
    } else {
      return call_immediately(STATE(maybe_send_ch2));
    }
  }

  Action ch1_msg_allocated() {
    auto* b =
        get_allocation_result(node_->iface()->global_message_write_flow());

    b->data()->reset(
        static_cast<openlcb::Defs::MTI>(openlcb::Defs::MTI_XPRESSNET + 3),
        node_->node_id(), string());
    b->data()->payload.push_back(message()->data()->channel | 0x10);
    b->data()->payload.append((char*)message()->data()->ch1Data,
                              message()->data()->ch1Size);
    node_->iface()->global_message_write_flow()->send(b);

    return call_immediately(STATE(maybe_send_ch2));
  }

  Action maybe_send_ch2() {
    if (message()->data()->ch2Size) {
      return allocate_and_call(node_->iface()->global_message_write_flow(),
                               STATE(ch2_msg_allocated));
    } else {
      return release_and_exit();
    }
  }

  Action ch2_msg_allocated() {
    auto* b =
        get_allocation_result(node_->iface()->global_message_write_flow());

    b->data()->reset(
        static_cast<openlcb::Defs::MTI>(openlcb::Defs::MTI_XPRESSNET + 4),
        node_->node_id(), string());
    b->data()->payload.push_back(message()->data()->channel | 0x20);
    b->data()->payload.append((char*)message()->data()->ch2Data,
                              message()->data()->ch2Size);
    node_->iface()->global_message_write_flow()->send(b);

    return release_and_exit();
  }

  RailcomHubFlow* parent_;
  Node* node_;
  RailcomHubPort* occupancyPort_;
};

class RailcomBroadcastFlow : public RailcomHubPort {
 public:
  RailcomBroadcastFlow(RailcomHubFlow* parent, Node* node,
                       RailcomHubPort* occupancy_port,
                       RailcomHubPort* debug_port, unsigned channel_count)
      : RailcomHubPort(parent->service()),
        parent_(parent),
        node_(node),
        occupancyPort_(occupancy_port),
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
    if (channel == 0xff && occupancyPort_) {
      occupancyPort_->send(transfer_message());
      return exit();
    }
    if (channel >= size_ ||
        !channels_[channel].process_packet(*message()->data())) {
      debugPort_->send(transfer_message());
      return exit();
    }
    auto& decoder = channels_[channel];
    if (decoder.current_address() == decoder.lastAddress_) {
      return release_and_exit();
    }
    // Now: the visible address has changed. Send event reports.
    return allocate_and_call(node_->iface()->global_message_write_flow(),
                             STATE(send_invalid));
  }

  Action send_invalid() {
    auto channel = message()->data()->channel;
    auto& decoder = channels_[channel];
    auto* b = get_allocation_result(node_->iface()->global_message_write_flow());
    b->data()->reset(
        Defs::MTI_PRODUCER_IDENTIFIED_INVALID, node_->node_id(),
        eventid_to_buffer(address_to_eventid(channel, decoder.lastAddress_)));
    b->set_done(n_.reset(this));
    node_->iface()->global_message_write_flow()->send(b);
    return wait_and_call(STATE(allocate_for_event));
  }

  Action allocate_for_event() {
    return allocate_and_call(node_->iface()->global_message_write_flow(),
                             STATE(send_event));
  }

  Action send_event() {
    auto channel = message()->data()->channel;
    auto& decoder = channels_[channel];
    auto* b = get_allocation_result(node_->iface()->global_message_write_flow());
    b->data()->reset(Defs::MTI_EVENT_REPORT, node_->node_id(),
                     eventid_to_buffer(address_to_eventid(
                         channel, decoder.current_address())));
    b->set_done(n_.reset(this));
    node_->iface()->global_message_write_flow()->send(b);
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

  RailcomHubFlow* parent_;
  Node* node_;
  RailcomHubPort* occupancyPort_;
  RailcomHubPort* debugPort_;
  unsigned size_;
  dcc::RailcomBroadcastDecoder* channels_;
  BarrierNotifiable n_;
};

const uint64_t RailcomBroadcastFlow::FEEDBACK_EVENTID_BASE =
  (0x09ULL << 56) | TractionDefs::NODE_ID_DCC;


class FeedbackBasedOccupancy : public RailcomHubPort {
 public:
  FeedbackBasedOccupancy(Node* node, uint64_t event_base,
                         unsigned channel_count)
      : RailcomHubPort(node->iface()),
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
  BitRangeEventPC eventHandler_;
  WriteHelper h_;
  BarrierNotifiable n_;
};
}
openlcb::FeedbackBasedOccupancy railcom_occupancy(stack.node(), R_EVENT_ID + 24, 4);
openlcb::RailcomProxy railcom_proxy(&railcom_hub, stack.node(), &railcom_occupancy);
// TODO(balazs.racz) reenable this
//openlcb::RailcomBroadcastFlow railcom_broadcast(&railcom_hub, &g_node,
//                                                &railcom_occupancy,
//                                                &railcom_proxy, 4);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[]) {
  LED_GREEN_Pin::set(false);
  LED_YELLOW_Pin::set(false);
  LED_BLUE_Pin::set(false);
  stack.add_can_port_select("/dev/can0");
#ifdef HAVE_RAILCOM
  // we need to enable the dcc receiving driver.
  ::open("/dev/nrz0", O_NONBLOCK | O_RDONLY);
  HubDeviceNonBlock<RailcomHubFlow> railcom_port(&railcom_hub, "/dev/railcom");
  // os_thread_create(nullptr, "railcom_reader", 1, 800, &railcom_uart_thread,
  //                 &r1);
#endif
#ifdef USE_WII_CHUCK
  bracz_custom::WiiChuckReader wii_reader("/dev/i2c4", &wii_throttle);
  wii_reader.start();
#endif

  stack.loop_executor();
  return 0;
}
