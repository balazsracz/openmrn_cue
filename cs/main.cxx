/** \copyright
 * Copyright (c) 2012, Stuart W Baker
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
 * \file main.c
 * This file represents the entry point to a simple test program.
 *
 * @author Stuart W. Baker
 * @date 16 September 2012
 */

#define __STDC_VERSION__ 199901L

#ifdef TARGET_LPC2368
#include "LPC23xx.h"
#endif

#include "src/cs_config.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <inttypes.h>

#include "os/os.h"
#include "os/OS.hxx"
#include "os/watchdog.h"
#include "utils/pipe.hxx"
#include "if/nmranet_if.h"
#include "core/nmranet_node.h"
#include "core/nmranet_event.h"
#include "core/nmranet_datagram.h"
#include "nmranet_config.h"

#include "src/host_packet.h"
#include "src/can-queue.h"

#include "src/event_registry.hxx"
#include "src/common_event_handlers.hxx"
#include "src/automata_runner.h"
#include "src/automata_control.h"
#include "src/updater.hxx"
#include "src/timer_updater.hxx"



#if defined(TARGET_LPC1768) || defined(SECOND)
#include "src/mbed_gpio_handlers.hxx"
#endif

const char *nmranet_manufacturer = "Balazs Racz";
const char *nmranet_hardware_rev = "N/A";
const char *nmranet_software_rev = "0.1";


#ifdef TARGET_LPC11Cxx
// Memory constrained situation -- set the parameters to tight values.
const size_t main_stack_size = 900;
const int main_priority = 0;
const size_t ALIAS_POOL_SIZE = 1;
const size_t DOWNSTREAM_ALIAS_CACHE_SIZE = 1;
const size_t UPSTREAM_ALIAS_CACHE_SIZE = 1;
const size_t DATAGRAM_POOL_SIZE = 0;
const size_t CAN_RX_BUFFER_SIZE = 1;
const size_t CAN_TX_BUFFER_SIZE = 8;
const size_t SERIAL_RX_BUFFER_SIZE = 16;
const size_t SERIAL_TX_BUFFER_SIZE = 16;
const size_t DATAGRAM_THREAD_STACK_SIZE = 256;
const size_t CAN_IF_READ_THREAD_STACK_SIZE = 700;
#else
const size_t main_stack_size = 1560;
const int main_priority = 0;
const size_t ALIAS_POOL_SIZE = 2;
const size_t DOWNSTREAM_ALIAS_CACHE_SIZE = 2;
const size_t UPSTREAM_ALIAS_CACHE_SIZE = 2;
const size_t DATAGRAM_POOL_SIZE = 10;
const size_t CAN_RX_BUFFER_SIZE = 1;
const size_t CAN_TX_BUFFER_SIZE = 32;
const size_t SERIAL_RX_BUFFER_SIZE = 128;
const size_t SERIAL_TX_BUFFER_SIZE = 130;
const size_t DATAGRAM_THREAD_STACK_SIZE = 512;
const size_t CAN_IF_READ_THREAD_STACK_SIZE = 1024;
#endif

extern const unsigned long long NODE_ADDRESS;


extern "C" {
void resetblink(uint32_t pattern);
void diewith(uint32_t pattern);
extern uint32_t blinker_pattern;

#ifdef __FreeRTOS__ //TARGET_LPC11Cxx
  // This gets rid of about 50 kbytes of flash code that is unnecessarily
  // linked into the binary.
void __wrap___cxa_pure_virtual(void) {
  abort();
}

  // This removes 400 bytes of memory allocated at startup for the atexit
  // structure.
int __wrap___cxa_atexit(void) {
  return 0;
}


#endif


}


node_t node;

#ifdef __FreeRTOS__

#include "src/mbed_i2c_update.hxx"

extern SynchronousUpdater i2c_updater;
extern I2CInUpdater in_extender0;
extern I2CInUpdater in_extender1;
extern I2CInUpdater in_extender2;

#endif


void* out_blinker_thread(void*)
{
    resetblink(0);
    while (1)
    {
        usleep(500000);
        //resetblink(1);
        nmranet_event_produce(node, 0x0502010202650012ULL, EVENT_STATE_INVALID);
        nmranet_event_produce(node, 0x0502010202650012ULL, EVENT_STATE_VALID);
        usleep(500000);
        //resetblink(0);
        nmranet_event_produce(node, 0x0502010202650013ULL, EVENT_STATE_INVALID);
        nmranet_event_produce(node, 0x0502010202650013ULL, EVENT_STATE_VALID);
    }
    return NULL;
}

EventRegistry registry;

class BlinkerToggleEventHandler : public MemoryToggleEventHandler<uint32_t> {
public:
    BlinkerToggleEventHandler(uint64_t event_on, uint64_t event_off)
        : MemoryToggleEventHandler<uint32_t>(
            event_on, event_off, 1, &blinker_pattern) {}

    virtual void HandleEvent(uint64_t event) {
        MemoryToggleEventHandler<uint32_t>::HandleEvent(event);
        resetblink(blinker_pattern);
    }
};

#if defined(TARGET_LPC2368)

#ifndef SECOND
BlinkerToggleEventHandler led_blinker(BRACZ_LAYOUT | 0x203c,
                                      BRACZ_LAYOUT | 0x203d);
#else
BlinkerToggleEventHandler led_blinker(BRACZ_LAYOUT | 0x2040,
                                      BRACZ_LAYOUT | 0x2041);

#endif
#endif // lpc 2368



#ifdef TARGET_LPC1768
BlinkerToggleEventHandler led_blinker(BRACZ_LAYOUT | 0x2050,
                                      BRACZ_LAYOUT | 0x2051);

MbedGPIOListener led_l2(0x0502010202650140ULL,
                        0x0502010202650141ULL,
                        LED2);
MbedGPIOListener led_l3(0x0502010202650160ULL,
                        0x0502010202650161ULL,
                        LED3);
#endif


#ifdef SECOND
MbedGPIOListener led_1(BRACZ_LAYOUT | 0x2030,
                       BRACZ_LAYOUT | 0x2031,
                       P0_23);
MbedGPIOListener led_2(BRACZ_LAYOUT | 0x2032,
                       BRACZ_LAYOUT | 0x2033,
                       P0_24);
MbedGPIOListener led_3(BRACZ_LAYOUT | 0x2034,
                       BRACZ_LAYOUT | 0x2035,
                       P0_25);
MbedGPIOListener led_4(BRACZ_LAYOUT | 0x2036,
                       BRACZ_LAYOUT | 0x2037,
                       P0_26);
MbedGPIOListener led_5(BRACZ_LAYOUT | 0x2038,
                       BRACZ_LAYOUT | 0x2039,
                       P1_30);
MbedGPIOListener led_6(BRACZ_LAYOUT | 0x203a,
                       BRACZ_LAYOUT | 0x203b,
                       P1_31);
#endif

#ifdef TARGET_LPC11Cxx
BlinkerToggleEventHandler led_blinker(BRACZ_LAYOUT | 0x2050,
                                      BRACZ_LAYOUT | 0x2051);

MemoryBitSetEventHandler l1(BRACZ_LAYOUT | 0x2500,
                            get_state_byte(1, OFS_IOA),
                            16);

MemoryBitSetEventHandler l2(BRACZ_LAYOUT | 0x2600,
                            get_state_byte(2, OFS_IOA),
                            16);
#endif

#if defined(TARGET_LPC2368) && defined(SECOND)
MemoryBitSetEventHandler l1(BRACZ_LAYOUT | 0x2100,
                            get_state_byte(1, OFS_IOA),
                            16);

MemoryBitSetEventHandler l2(BRACZ_LAYOUT | 0x2200,
                            get_state_byte(2, OFS_IOA),
                            16);

MemoryBitSetEventHandler l3(BRACZ_LAYOUT | 0x2300,
                            get_state_byte(3, OFS_IOA),
                            16);
#endif



#if defined(TARGET_LPC2368) || defined(TARGET_LPC1768)
DECLARE_PIPE(can_pipe0);
DECLARE_PIPE(can_pipe1);
#endif

#ifdef SNIFFER
DEFINE_PIPE(gc_pipe, 1);

#include "utils/gc_pipe.hxx"
#endif


extern "C" {
void appl_hw_init(void) __attribute__ ((weak));
void appl_hw_init(void) {
}

extern insn_t automata_code[];
}

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[])
{
  appl_hw_init();

#if defined(TARGET_LPC2368) || defined(TARGET_LPC1768)
    start_watchdog(5000);
    add_watchdog_reset_timer(500);
#endif

#if defined(TARGET_LPC2368)
#ifdef SECOND
    can_pipe0.AddPhysicalDeviceToPipe("/dev/can0", "can0_rx_thread", 512);
#else
    PacketQueue::initialize("/dev/serUSB0");
    can_pipe0.AddPhysicalDeviceToPipe("/dev/can1", "can0_rx_thread", 512);
    can_pipe1.AddPhysicalDeviceToPipe("/dev/can0", "can1_rx_thread", 512);
#endif
#endif

#if defined(TARGET_LPC1768)
    //PacketQueue::initialize("/dev/serUSB0");
    // Uncomment the next 1 line to transmit to hardware CANbus.
    //can_pipe0.AddPhysicalDeviceToPipe("/dev/can0", "can0_rx_thread", 512);
    //can_pipe1.AddPhysicalDeviceToPipe("/dev/can0", "can1_rx_thread", 512);
    
#endif

#ifdef SNIFFER
    GCAdapterBase::CreateGridConnectAdapter(&gc_pipe, &can_pipe0, false);
    //gc_pipe.AddPhysicalDeviceToPipe("/dev/stdout", "gc_parse_thread", 512);
#endif

    //can_pipe0.AddPhysicalDeviceToPipe("/dev/can1", "can1_rx_thread", 512);


    NMRAnetIF *nmranet_if;

#if defined(TARGET_LPC2368) || defined(__linux__)
#ifndef SECOND
    int fd = open("/dev/canp1v0", O_RDWR);
    ASSERT(fd >= 0);
    dcc_can_init(fd);
#endif
#endif


#if defined(TARGET_LPC2368) || defined(__linux__) || defined(TARGET_LPC1768)
    nmranet_if = nmranet_can_if_init(NODE_ADDRESS, "/dev/canp0v1", read, write);
#elif defined(TARGET_LPC11Cxx)
    nmranet_if = nmranet_can_if_init(NODE_ADDRESS, "/dev/can0", read, write);
#else
    #error define where to open the can bus.
#endif



    if (nmranet_if == NULL)
    {
#ifdef __FreeRTOS__
        diewith(BLINK_DIE_STARTUP);  // 3-3-2
#else
        exit(1);
#endif
    }

    node = nmranet_node_create(NODE_ADDRESS, nmranet_if, "Virtual Node", NULL);
    nmranet_node_user_description(node, "Test Node");

#if defined(TARGET_LPC11Cxx)

    // Consume one led and one relay per board.
    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2500, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2501, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2502, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2503, EVENT_STATE_INVALID);

    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x251c, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x251d, EVENT_STATE_INVALID);

    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2600, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2601, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2602, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2603, EVENT_STATE_INVALID);

    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x261c, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x261d, EVENT_STATE_INVALID);

    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2520, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2521, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2522, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2523, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2524, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2525, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2526, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2527, EVENT_STATE_INVALID);

    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2620, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2621, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2622, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2623, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2624, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2625, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2626, EVENT_STATE_INVALID);
    nmranet_event_producer(node, BRACZ_LAYOUT | 0x2627, EVENT_STATE_INVALID);

#elif defined(TARGET_LPC1768)

    nmranet_event_consumer(node, 0x0502010202650013ULL, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, 0x0502010202650012ULL, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, 0x0502010202650300ULL, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, 0x0502010202650301ULL, EVENT_STATE_INVALID);

    nmranet_event_consumer(node, 0x0502010202650040ULL, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, 0x0502010202650041ULL, EVENT_STATE_INVALID);
    //nmranet_event_consumer(node, 0x0502010202650042ULL, EVENT_STATE_INVALID);
    //nmranet_event_consumer(node, 0x0502010202650043ULL, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, 0x0502010202650060ULL, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, 0x0502010202650061ULL, EVENT_STATE_INVALID);
    //    nmranet_event_consumer(node, 0x0502010202650062ULL, EVENT_STATE_INVALID);
    //nmranet_event_consumer(node, 0x0502010202650063ULL, EVENT_STATE_INVALID);
    /*    nmranet_event_producer(node, 0x0502010202000000ULL, EVENT_STATE_INVALID);
    nmranet_event_producer(node, 0x0502010202650012ULL, EVENT_STATE_INVALID);
    nmranet_event_producer(node, 0x0502010202650013ULL, EVENT_STATE_VALID);*/


#elif defined(TARGET_LPC2368)

    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2040, EVENT_STATE_INVALID);
    nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2041, EVENT_STATE_INVALID);


    // Bits produced and consumed by the automata are automatically exported.

#ifdef SECOND
    for (int c = 0x30; c<=0x3d; c++) {
      nmranet_event_consumer(node, BRACZ_LAYOUT | 0x2000 | c, EVENT_STATE_INVALID);
    }

    for (int b = 0x21; b <= 0x23; ++b) {
      static const uint8_t produce[] = { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27 };
      static const uint8_t consume[] = { 0, 1, 2, 3, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28 ,29, 30, 31 };
      
      for (int p : produce) {
        nmranet_event_producer(node, BRACZ_LAYOUT | (b << 8) | p, EVENT_STATE_INVALID);
      }
      for (int p : consume) {
        nmranet_event_consumer(node, BRACZ_LAYOUT | (b << 8) | p, EVENT_STATE_INVALID);
      }
    }
#endif  // Second

#elif defined(__linux__)

    // Bits produced and consumed by the automata are automatically exported.

#else
#error define your event producers/consumers
#endif


#if defined(TARGET_LPC1768)
    MbedGPIOProducer input_pin1(
        node, 0x0502010202650300ULL, 0x0502010202650301ULL, p15, PullUp);
#endif

    nmranet_node_initialized(node);


#if defined(TARGET_LPC1768)
    // It is important to start this process only after the node_initialized
    // has returned, or else the program deadlocks. See
    // https://github.com/bakerstu/openmrn/issues/9
    TimerUpdater upd(MSEC_TO_NSEC(50), {&input_pin1});
#endif


#if defined(TARGET_LPC2368) || defined(TARGET_LPC11Cxx)
    MemoryBitSetProducer produce_i2c0(node, BRACZ_LAYOUT | 0x2120);
    MemoryBitSetProducer produce_i2c1(node, BRACZ_LAYOUT | 0x2220);

    in_extender0.RegisterListener(&produce_i2c0);
    in_extender1.RegisterListener(&produce_i2c1);
#if defined(TARGET_LPC2368)
    MemoryBitSetProducer produce_i2c2(node, BRACZ_LAYOUT | 0x2320);
    in_extender2.RegisterListener(&produce_i2c2);
#endif
#endif


    //os_thread_create(NULL, "out_blinker", 0, 800, out_blinker_thread, NULL);


#if defined(TARGET_LPC1768) || (defined(TARGET_LPC2368) && !defined(SECOND)) || defined(__linux__)
    AutomataRunner runner(node, automata_code);
    resume_all_automata();
#endif

#if defined(TARGET_LPC2368)
    i2c_updater.RunInNewThread("i2c_update", 0, 1024);
#endif

    resetblink(0);

#ifdef __FreeRTOS__
    for (;;)
    {

#if defined(TARGET_LPC11Cxx)
      // This takes a bit of time (blocking).
      i2c_updater.Step();
#else
      nmranet_node_wait(node, MSEC_TO_NSEC(300));
#endif

      // These are quick.
      for (size_t i = nmranet_event_pending(node); i > 0; i--) {
        node_handle_t node_handle;
        uint64_t event = nmranet_event_consume(node, &node_handle);
        registry.HandleEvent(event);
      }
      for (size_t i = nmranet_datagram_pending(node); i > 0; i--) {
        datagram_t datagram = nmranet_datagram_consume(node);
        // We release unknown datagrams iwthout processing them.
        nmranet_datagram_release(datagram);
      }
    }
#else

    for (;;)
    {
        int result = nmranet_node_wait(node, MSEC_TO_NSEC(300));

        if (result) {
            for (size_t i = nmranet_event_pending(node); i > 0; i--) {
                node_handle_t node_handle;
                uint64_t event = nmranet_event_consume(node, &node_handle);
                registry.HandleEvent(event);
            }
            for (size_t i = nmranet_datagram_pending(node); i > 0; i--) {
                datagram_t datagram = nmranet_datagram_consume(node);
                // We release unknown datagrams iwthout processing them.
                nmranet_datagram_release(datagram);
            }
        }
    }


#endif



    return 0;
}
