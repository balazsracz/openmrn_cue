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
#include "utils/Debouncer.hxx"
#include "nmranet/DefaultNode.hxx"
#include "driverlib/gpio.h"
#include "inc/hw_memmap.h"

#define ENABLE_TIVA_SIGNAL_DRIVER
#include "custom/TivaSignalPacket.hxx"
#include "custom/SignalLoop.hxx"

NO_THREAD nt;
Executor<1> g_executor(nt);
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

bracz_custom::TivaSignalPacketSender g_signalbus(&g_service, 15625, UART1_BASE,
                                                 INT_RESOLVE(INT_UART1_, 0));

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

TivaGPIOConsumer rel1(R_EVENT_ID + 17, R_EVENT_ID + 16, GPIO_PORTC_BASE,
                      GPIO_PIN_4);
TivaGPIOConsumer rel2(R_EVENT_ID + 19, R_EVENT_ID + 18, GPIO_PORTC_BASE,
                      GPIO_PIN_5);
TivaGPIOConsumer rel3(R_EVENT_ID + 21, R_EVENT_ID + 20, GPIO_PORTG_BASE,
                      GPIO_PIN_5);
TivaGPIOConsumer rel4(R_EVENT_ID + 23, R_EVENT_ID + 22, GPIO_PORTF_BASE,
                      GPIO_PIN_3);

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

bracz_custom::SignalLoop signal_loop(&g_signalbus, &g_node,
                                     (R_EVENT_ID & ~0xffffff) |
                                         ((R_EVENT_ID & 0xff00) << 8),
                                     NUM_SIGNALS);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[]) {
  HubDeviceNonBlock<CanHubFlow> can0_port(&can_hub0, "/dev/can0");
  // Bootstraps the alias allocation process.
  g_if_can.alias_allocator()->send(g_if_can.alias_allocator()->alloc());

  g_executor.thread_body();
  return 0;
}
