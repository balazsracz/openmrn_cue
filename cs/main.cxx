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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <inttypes.h>

#include "os/os.h"
#include "os/OS.hxx"
#include "if/nmranet_if.h"
#include "core/nmranet_node.h"
#include "core/nmranet_event.h"
#include "core/nmranet_datagram.h"
#include "nmranet_config.h"

#include "host_packet.h"
#include "can-queue.h"

#include "event_registry.hxx"
#include "common_event_handlers.hxx"

const char *nmranet_manufacturer = "Balazs Racz";
const char *nmranet_hardware_rev = "N/A";
const char *nmranet_software_rev = "0.1";
const size_t main_stack_size = 2560;
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

extern "C" {
void resetblink(uint32_t pattern);
void diewith(uint32_t pattern);
extern uint32_t blinker_pattern;
}

node_t node;

void* out_blinker_thread(void*) {
  resetblink(0);
  while (1) {
    usleep(200000);
    //resetblink(1);
    nmranet_event_produce(node, 0x0502010202650012ULL, EVENT_STATE_INVALID);
    nmranet_event_produce(node, 0x0502010202650012ULL, EVENT_STATE_VALID);
    usleep(200000);
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

BlinkerToggleEventHandler led_blinker(0x0502010202650012ULL, 0x0502010202650013ULL);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[]) {
  PacketQueue::initialize("/dev/serUSB0");
  int fd = open("/dev/can0", O_RDWR);
  ASSERT(fd >= 0);
  dcc_can_init(fd);

  NMRAnetIF *nmranet_if;

  nmranet_if = nmranet_can_if_init(0x02010d000000ULL, "/dev/can1", read, write);

  if (nmranet_if == NULL)
  {
    diewith(0x8002CCCA);  // 3-3-1
  }

  node = nmranet_node_create(0x02010d000001ULL, nmranet_if, "Virtual Node", NULL);
  nmranet_node_user_description(node, "Test Node");

  nmranet_event_consumer(node, 0x0502010202000000ULL, EVENT_STATE_INVALID);
  nmranet_event_consumer(node, 0x0502010202650013ULL, EVENT_STATE_INVALID);
  nmranet_event_consumer(node, 0x0502010202650012ULL, EVENT_STATE_INVALID);
  nmranet_event_consumer(node, 0x05020102a8650013ULL, EVENT_STATE_INVALID);
  nmranet_event_consumer(node, 0x05020102a8650012ULL, EVENT_STATE_INVALID);
  nmranet_event_producer(node, 0x0502010202000000ULL, EVENT_STATE_INVALID);
  nmranet_event_producer(node, 0x0502010202650012ULL, EVENT_STATE_INVALID);
  nmranet_event_producer(node, 0x0502010202650013ULL, EVENT_STATE_VALID);
  nmranet_node_initialized(node);

  os_thread_create(NULL, "out_blinker", 0, 800, out_blinker_thread, NULL);

  for (;;) {
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

  return 0;
}
