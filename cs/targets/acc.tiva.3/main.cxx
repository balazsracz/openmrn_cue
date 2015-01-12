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
#include "nmranet/IfCan.hxx"
#include "nmranet/Defs.hxx"
#include "nmranet/AliasAllocator.hxx"
#include "nmranet/EventService.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/EventBitProducer.hxx"
#include "nmranet/RefreshLoop.hxx"
#include "nmranet/DatagramCan.hxx"
#include "nmranet/MemoryConfig.hxx"
#include "utils/Debouncer.hxx"
#include "nmranet/DefaultNode.hxx"
#include "driverlib/gpio.h"
#include "inc/hw_memmap.h"

#define ENABLE_TIVA_SIGNAL_DRIVER
#include "custom/TivaSignalPacket.hxx"
#include "custom/SignalLoop.hxx"
#include "dcc/RailCom.hxx"
#include "dcc/Packet.hxx"
#include "hardware.hxx"
#include "TivaNRZ.hxx"
#include "dcc/Receiver.hxx"
#include "SimpleLog.hxx"

NO_THREAD nt;
Executor<5> g_executor(nt);
Service g_service(&g_executor);
CanHubFlow can_hub0(&g_service);

extern const nmranet::NodeID NODE_ID;
// const nmranet::NodeID NODE_ID = 0x05010101144aULL;

// OVERRIDE_CONST(can_tx_buffer_size, 2);
// OVERRIDE_CONST(can_rx_buffer_size, 1);

OVERRIDE_CONST(main_thread_stack_size, 2048);

nmranet::IfCan g_if_can(&g_executor, &can_hub0, 3, 3, 2);
static nmranet::AddAliasAllocator _alias_allocator(NODE_ID, &g_if_can);
nmranet::DefaultNode g_node(&g_if_can, NODE_ID);
nmranet::EventService g_event_service(&g_if_can);
nmranet::CanDatagramService g_datagram_service(&g_if_can, 2, 2);
nmranet::MemoryConfigHandler g_memory_config_handler(&g_datagram_service,
                                                     &g_node, 1);

namespace nmranet {
Pool* const g_incoming_datagram_allocator = mainBufferPool;
}

/*bracz_custom::TivaSignalPacketSender g_signalbus(&g_service, 15625, UART1_BASE,
                                                 INT_RESOLVE(INT_UART1_, 0));
*/


static const uint64_t R_EVENT_ID =
    0x0501010114FF2000ULL | ((NODE_ID & 0xf) << 8);

class TivaGPIOConsumer : public nmranet::BitEventInterface,
                         public nmranet::BitEventConsumer {
 public:
  TivaGPIOConsumer(uint64_t event_on, uint64_t event_off, uint32_t port,
                   uint8_t pin)
      : BitEventInterface(event_on, event_off),
        BitEventConsumer(this),
        memory_(reinterpret_cast<uint8_t*>(port + (pin << 2))) {}

  bool GetCurrentState() OVERRIDE { return (*memory_) ? true : false; }
  void SetState(bool new_value) OVERRIDE {
    if (new_value) {
      *memory_ = 0xff;
    } else {
      *memory_ = 0;
    }
  }

  nmranet::Node* node() OVERRIDE { return &g_node; }

 private:
  volatile uint8_t* memory_;
};

class LoggingBit : public nmranet::BitEventInterface {
 public:
  LoggingBit(uint64_t event_on, uint64_t event_off, const char* name)
      : BitEventInterface(event_on, event_off), name_(name), state_(false) {}

  virtual bool GetCurrentState() { return state_; }
  virtual void SetState(bool new_value) {
    state_ = new_value;
// HASSERT(0);
#ifdef __linux__
    LOG(INFO, "bit %s set to %d", name_, state_);
#else
    resetblink(state_ ? 1 : 0);
#endif
  }

  virtual nmranet::Node* node() { return &g_node; }

 private:
  const char* name_;
  bool state_;
};

LoggingBit led_red_bit(R_EVENT_ID + 0, R_EVENT_ID + 1, nullptr);
nmranet::BitEventConsumer red_consumer(&led_red_bit);
TivaGPIOConsumer led_yellow(R_EVENT_ID + 2, R_EVENT_ID + 3, GPIO_PORTB_BASE,
                            GPIO_PIN_0);
TivaGPIOConsumer led_green(R_EVENT_ID + 4, R_EVENT_ID + 5, GPIO_PORTD_BASE,
                           GPIO_PIN_5);
TivaGPIOConsumer led_blue(R_EVENT_ID + 6, R_EVENT_ID + 7, GPIO_PORTG_BASE,
                          GPIO_PIN_1);
TivaGPIOConsumer led_bluesw(R_EVENT_ID + 8, R_EVENT_ID + 9, GPIO_PORTB_BASE,
                            GPIO_PIN_6);
TivaGPIOConsumer led_goldsw(R_EVENT_ID + 10, R_EVENT_ID + 11, GPIO_PORTB_BASE,
                            GPIO_PIN_7);

/* TODO: check the pin numbers here

TivaGPIOConsumer rel1(R_EVENT_ID + 17, R_EVENT_ID + 16, GPIO_PORTC_BASE,
                      GPIO_PIN_4);
TivaGPIOConsumer rel2(R_EVENT_ID + 19, R_EVENT_ID + 18, GPIO_PORTC_BASE,
                      GPIO_PIN_5);
TivaGPIOConsumer rel3(R_EVENT_ID + 21, R_EVENT_ID + 20, GPIO_PORTG_BASE,
                      GPIO_PIN_5);
TivaGPIOConsumer rel4(R_EVENT_ID + 23, R_EVENT_ID + 22, GPIO_PORTF_BASE,
                      GPIO_PIN_3);
*/

TivaGPIOConsumer out0(R_EVENT_ID + 32 + 0, R_EVENT_ID + 33 + 0, GPIO_PORTE_BASE,
                      GPIO_PIN_4);  // Out0
TivaGPIOConsumer out1(R_EVENT_ID + 32 + 2, R_EVENT_ID + 33 + 2, GPIO_PORTE_BASE,
                      GPIO_PIN_5);  // Out1
TivaGPIOConsumer out2(R_EVENT_ID + 32 + 4, R_EVENT_ID + 33 + 4, GPIO_PORTD_BASE,
                      GPIO_PIN_0);  // Out2
TivaGPIOConsumer out3(R_EVENT_ID + 32 + 6, R_EVENT_ID + 33 + 6, GPIO_PORTD_BASE,
                      GPIO_PIN_1);  // Out3
TivaGPIOConsumer out4(R_EVENT_ID + 32 + 8, R_EVENT_ID + 33 + 8, GPIO_PORTD_BASE,
                      GPIO_PIN_2);  // Out4
TivaGPIOConsumer out5(R_EVENT_ID + 32 + 10, R_EVENT_ID + 33 + 10,
                      GPIO_PORTD_BASE, GPIO_PIN_3);  // Out5
TivaGPIOConsumer out6(R_EVENT_ID + 32 + 12, R_EVENT_ID + 33 + 12,
                      GPIO_PORTE_BASE, GPIO_PIN_3);  // Out6
TivaGPIOConsumer out7(R_EVENT_ID + 32 + 14, R_EVENT_ID + 33 + 14,
                      GPIO_PORTE_BASE, GPIO_PIN_2);  // Out7

class TivaGPIOProducerBit : public nmranet::BitEventInterface {
 public:
  TivaGPIOProducerBit(uint64_t event_on, uint64_t event_off, uint32_t port_base,
                      uint8_t port_bit)
      : BitEventInterface(event_on, event_off),
        ptr_(reinterpret_cast<const uint8_t*>(port_base +
                                              (((unsigned)port_bit) << 2))) {}

  bool GetCurrentState() OVERRIDE { return *ptr_; }

  void SetState(bool new_value) OVERRIDE {
    DIE("cannot set state of input producer");
  }

  nmranet::Node* node() OVERRIDE { return &g_node; }

 private:
  const uint8_t* ptr_;
};

typedef nmranet::PolledProducer<QuiesceDebouncer, TivaGPIOProducerBit>
    TivaGPIOProducer;
QuiesceDebouncer::Options opts(8);

TivaGPIOProducer in0(opts, R_EVENT_ID + 48 + 0, R_EVENT_ID + 49 + 0,
                     GPIO_PORTA_BASE, GPIO_PIN_0);
TivaGPIOProducer in1(opts, R_EVENT_ID + 48 + 2, R_EVENT_ID + 49 + 2,
                     GPIO_PORTA_BASE, GPIO_PIN_1);
TivaGPIOProducer in2(opts, R_EVENT_ID + 48 + 4, R_EVENT_ID + 49 + 4,
                     GPIO_PORTA_BASE, GPIO_PIN_2);
TivaGPIOProducer in3(opts, R_EVENT_ID + 48 + 6, R_EVENT_ID + 49 + 6,
                     GPIO_PORTA_BASE, GPIO_PIN_3);
TivaGPIOProducer in4(opts, R_EVENT_ID + 48 + 8, R_EVENT_ID + 49 + 8,
                     GPIO_PORTA_BASE, GPIO_PIN_4);
TivaGPIOProducer in5(opts, R_EVENT_ID + 48 + 10, R_EVENT_ID + 49 + 10,
                     GPIO_PORTA_BASE, GPIO_PIN_5);
TivaGPIOProducer in6(opts, R_EVENT_ID + 48 + 12, R_EVENT_ID + 49 + 12,
                     GPIO_PORTA_BASE, GPIO_PIN_6);
TivaGPIOProducer in7(opts, R_EVENT_ID + 48 + 14, R_EVENT_ID + 49 + 14,
                     GPIO_PORTA_BASE, GPIO_PIN_7);

nmranet::RefreshLoop loop(&g_node,
                          {&in0, &in1, &in2, &in3, &in4, &in5, &in6, &in7});

#define NUM_SIGNALS 32

/*bracz_custom::SignalLoop signal_loop(&g_signalbus, &g_node,
                                     (R_EVENT_ID & ~0xffffff) |
                                         ((R_EVENT_ID & 0xff00) << 8),
                                         NUM_SIGNALS);*/
struct RailcomReaderParams {
  const char* file_path;
  uint32_t base;
  char packetbuf[8];
  int len;
  nmranet::WriteHelper h;
  SyncNotifiable n;
};

void* railcom_uart_thread(void* arg) {
  auto* opts = static_cast<RailcomReaderParams*>(arg);

  int async_fd = ::open(opts->file_path, O_NONBLOCK | O_RDONLY);
  int sync_fd = ::open(opts->file_path, O_RDONLY);

  MAP_UARTConfigSetExpClk(
      opts->base, configCPU_CLOCK_HZ, 250000,
      UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);

  HASSERT(async_fd > 0);
  HASSERT(sync_fd > 0);
  nmranet::WriteHelper::payload_type p;
  while (true) {
    int count = 0;
    p.clear();
    count += ::read(sync_fd, opts->packetbuf, 1);
    Debug::RailcomUartReceived::set(true);
    if (dcc::railcom_decode[(int)opts->packetbuf[0]] == dcc::RailcomDefs::INV) {
      // continue;
    }
    usleep(1000);
    count += ::read(async_fd, opts->packetbuf + 1, 7);
    HASSERT(count >= 1);
    // disables the uart until the next packet is here.
    HWREG(UART2_BASE + UART_O_CTL) &= ~UART_CTL_RXE;
    Debug::RailcomUartReceived::set(false);

    p.assign(opts->packetbuf, count);
    opts->h.WriteAsync(&g_node, nmranet::Defs::MTI_XPRESSNET, opts->h.global(),
                       p, &opts->n);
    opts->n.wait_for_notification();
  }
  return nullptr;
}

RailcomReaderParams r1{"/dev/ser2", UART2_BASE};

class DccPacketDebugFlow : public StateFlow<Buffer<dcc::Packet>, QList<1>> {
 public:
  DccPacketDebugFlow(nmranet::Node* node)
      : StateFlow<Buffer<dcc::Packet>, QList<1>>(
            node->interface()->dispatcher()->service()),
        node_(node) {}

 private:
  Action entry() override {
    Debug::DccPacketDelay::set(false);
    return allocate_and_call(node_->interface()->global_message_write_flow(),
                             STATE(msg_allocated));
  }

  Action msg_allocated() {
    auto* b =
        get_allocation_result(node_->interface()->global_message_write_flow());

    b->data()->reset(
        static_cast<nmranet::Defs::MTI>(nmranet::Defs::MTI_XPRESSNET + 1),
        node_->node_id(),
        string((char*)message()->data()->payload, message()->data()->dlc));
    node_->interface()->global_message_write_flow()->send(b);
    return release_and_exit();
  }

  nmranet::Node* node_;
} g_packet_debug_flow(&g_node);

class DccDecodeFlow : public dcc::DccDecodeFlow {
 public:
  DccDecodeFlow() : dcc::DccDecodeFlow(&g_service, "/dev/nrz0") {}

 private:
  void dcc_packet_finished(const uint8_t* payload, size_t len) override {
    auto* b = g_packet_debug_flow.alloc();
    b->data()->dlc = len;
    memcpy(b->data()->payload, payload, len);
    g_packet_debug_flow.send(b);
  }

  void mm_packet_finished(const uint8_t* payload, size_t len) override {
    auto* b = g_packet_debug_flow.alloc();
    b->data()->dlc = len;
    memcpy(b->data()->payload, payload, len);
    b->data()->payload[0] |= 0xFC;
    g_packet_debug_flow.send(b);
  }

  void debug_data(uint32_t value) override {
    value /= (configCPU_CLOCK_HZ / 1000000);
    log(decoder_.state());
    log(value);
  }

  void log(uint8_t value) {
    dbuffer[ptr] = value;
    ++ptr;
    if (ptr >= sizeof(dbuffer)) ptr = 0;
  }
  uint8_t dbuffer[1024];
  uint16_t ptr = 0;
};

DccDecodeFlow* g_decode_flow;

typedef StructContainer<dcc::Feedback> RailcomHubContainer;
typedef HubContainer<RailcomHubContainer> RailcomHubData;
typedef FlowInterface<Buffer<RailcomHubData>> RailcomHubPortInterface;
typedef StateFlow<Buffer<RailcomHubData>, QList<1>> RailcomHubPort;
typedef GenericHubFlow<RailcomHubData> RailcomHubFlow;

RailcomHubFlow railcom_hub(&g_service);

namespace nmranet {
class RailcomProxy : public RailcomHubPort {
 public:
  RailcomProxy(RailcomHubFlow* parent, Node* node)
      : RailcomHubPort(parent->service()), parent_(parent), node_(node) {
    parent_->register_port(this);
  }

  ~RailcomProxy() { parent_->unregister_port(this); }

  Action entry() {
    if (message()->data()->ch1Size) {
      return allocate_and_call(node_->interface()->global_message_write_flow(),
                               STATE(ch1_msg_allocated));
    } else {
      return call_immediately(STATE(maybe_send_ch2));
    }
  }

  Action ch1_msg_allocated() {
    auto* b =
        get_allocation_result(node_->interface()->global_message_write_flow());

    b->data()->reset(
        static_cast<nmranet::Defs::MTI>(nmranet::Defs::MTI_XPRESSNET + 3),
        node_->node_id(), string());
    b->data()->payload.push_back(message()->data()->channel | 0x10);
    b->data()->payload.append((char*)message()->data()->ch1Data, message()->data()->ch1Size);
    node_->interface()->global_message_write_flow()->send(b);

    return call_immediately(STATE(maybe_send_ch2));
  }

  Action maybe_send_ch2() {
    if (message()->data()->ch2Size) {
      return allocate_and_call(node_->interface()->global_message_write_flow(),
                               STATE(ch2_msg_allocated));
    } else {
      return release_and_exit();
    }
  }

  Action ch2_msg_allocated() {
    auto* b =
        get_allocation_result(node_->interface()->global_message_write_flow());

    b->data()->reset(
        static_cast<nmranet::Defs::MTI>(nmranet::Defs::MTI_XPRESSNET + 4),
        node_->node_id(), string());
    b->data()->payload.push_back(message()->data()->channel | 0x20);
    b->data()->payload.append((char*)message()->data()->ch2Data, message()->data()->ch2Size);
    node_->interface()->global_message_write_flow()->send(b);

    return release_and_exit();
  }

  RailcomHubFlow* parent_;
  Node* node_;
};
}

nmranet::RailcomProxy railcom_proxy(&railcom_hub, &g_node);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[]) {
  LED_GREEN_Pin::set(false);
  LED_YELLOW_Pin::set(false);
  LED_BLUE_Pin::set(false);
  // we need to enable the dcc receiving driver.
  ::open("/dev/nrz0", O_NONBLOCK | O_RDONLY);
  HubDeviceNonBlock<CanHubFlow> can0_port(&can_hub0, "/dev/can0");
  HubDeviceNonBlock<RailcomHubFlow> railcom_port(&railcom_hub, "/dev/railcom");
  // Bootstraps the alias allocation process.
  g_if_can.alias_allocator()->send(g_if_can.alias_allocator()->alloc());

  //os_thread_create(nullptr, "railcom_reader", 1, 800, &railcom_uart_thread,
  //                 &r1);

  //g_decode_flow = new DccDecodeFlow();
  g_executor.thread_body();
  return 0;
}
