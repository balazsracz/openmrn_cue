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
 * Applicaiton main file for a Signal server running on a tiva 123 launchpad.
 *
 * @author Balazs Racz
 * @date 12 Jan 2025
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

#include "utils/HubDevice.hxx"
#include "utils/HubDeviceNonBlock.hxx"
#include "utils/HubDeviceSelect.hxx"
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
#include "utils/Debouncer.hxx"
#include "openlcb/DefaultNode.hxx"
#include "driverlib/gpio.h"
#include "inc/hw_memmap.h"
#include "openlcb/SimpleNodeInfoMockUserFile.hxx"
#include "DummyGPIO.hxx"


#define ENABLE_TIVA_SIGNAL_DRIVER

namespace Debug {
typedef DummyPin DetectRepeat;
}

#include "custom/TivaSignalPacket.hxx"
#include "custom/SignalLoop.hxx"
#include "custom/SignalServer.hxx"
#include "hardware.hxx"
#include "SimpleLog.hxx"
#include "custom/TivaGPIOConsumer.hxx"
#include "custom/TivaGPIOProducerBit.hxx"
#include "custom/LoggingBit.hxx"

// These preprocessor symbols are used to select which physical connections
// will be enabled in the main(). See @ref appl_main below.
#define SNIFF_ON_SERIAL
//#define SNIFF_ON_USB
//#define HAVE_PHYSICAL_CAN_PORT

// Specifies the 48-bit OpenLCB node identifier. This must be unique for every
// hardware manufactured, so in production this should be replaced by some
// easily incrementable method.
extern const openlcb::NodeID NODE_ID = 0x050101011422ULL;

openlcb::SimpleCanStack stack(NODE_ID);

openlcb::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char *const openlcb::SNIP_DYNAMIC_FILENAME = openlcb::MockSNIPUserFile::snip_user_file_path;

OVERRIDE_CONST(main_thread_stack_size, 2048);

bracz_custom::TivaSignalPacketSender g_signalbus(stack.service(), 15625,
                                                 SYSCTL_PERIPH_UART1,
                                                 UART1_BASE,
                                                 INT_RESOLVE(INT_UART1_, 0));


static const uint64_t R_EVENT_ID =
    0x0501010114FF2000ULL | ((NODE_ID & 0xf) << 8);

LoggingBit led_red_bit(R_EVENT_ID + 0, R_EVENT_ID + 1, nullptr);
openlcb::BitEventConsumer red_consumer(&led_red_bit);

TivaGPIOConsumer led_red(R_EVENT_ID + 2, R_EVENT_ID + 3,
                         LED_RED_Pin::GPIO_BASE,
                         LED_RED_Pin::GPIO_PIN);
TivaGPIOConsumer led_green(R_EVENT_ID + 4, R_EVENT_ID + 5,
                           LED_GREEN_Pin::GPIO_BASE, LED_GREEN_Pin::GPIO_PIN);
//TivaGPIOConsumer led_blue(R_EVENT_ID + 6, R_EVENT_ID + 7,
//                          LED_BLUE_Pin::GPIO_BASE, LED_BLUE_Pin::GPIO_PIN);


#define NUM_SIGNALS 32

bracz_custom::SignalLoop signal_loop(&g_signalbus, stack.node(),
                                     (R_EVENT_ID & ~0xffffff) |
                                     ((R_EVENT_ID & 0xff00) << 8),
                                     NUM_SIGNALS);

bracz_custom::SignalServer signal_server(stack.dg_service(), stack.node(),
                                         &g_signalbus);


OVERRIDE_CONST(gc_generate_newlines, 1);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[]) {
  LED_GREEN_Pin::set(false);
  LED_RED_Pin::set(true);
  LED_BLUE_Pin::set(false);
  //stack.add_can_port_select("/dev/can0");
#if defined(HAVE_PHYSICAL_CAN_PORT)
    stack.add_can_port_select("/dev/can0");
#endif
#if defined(SNIFF_ON_USB)
    stack.add_gridconnect_port("/dev/serUSB0");
#endif
#if defined(SNIFF_ON_SERIAL)
    stack.add_gridconnect_port("/dev/ser0");
#endif

  stack.loop_executor();
  return 0;
}
