/** \copyright
 * Copyright (c) 2018, Balazs Racz
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
 * Main file for the io board application on the Nucleo board with the DevKit IO
 * board plugged in.
 *
 * @author Balazs Racz
 * @date 18 April 2018
 */

#define _DEFAULT_SOURCE

#include <unistd.h>

#include "os/os.h"
#include "nmranet_config.h"

#include "openlcb/SimpleStack.hxx"
#include "openlcb/MultiConfiguredConsumer.hxx"
#include "openlcb/ConfiguredProducer.hxx"
#include "openlcb/ServoConsumer.hxx"

#include "freertos_drivers/st/Stm32Gpio.hxx"
#include "freertos_drivers/common/BlinkerGPIO.hxx"
#include "freertos_drivers/common/DummyGPIO.hxx"
#include "freertos_drivers/common/MCP23017Gpio.hxx"
#include "os/MmapGpio.hxx"
#include "config.hxx"
#include "hardware.hxx"
#include "PWM.hxx"
#include "i2c.h"
#include "i2c-dev.h"

#include "logic/OlcbBindings.hxx"

// These preprocessor symbols are used to select which physical connections
// will be enabled in the main(). See @ref appl_main below.
//#define SNIFF_ON_SERIAL
//#define SNIFF_ON_USB
#define HAVE_PHYSICAL_CAN_PORT

// Changes the default behavior by adding a newline after each gridconnect
// packet. Makes it easier for debugging the raw device.
OVERRIDE_CONST(gc_generate_newlines, 1);
// Specifies how much RAM (in bytes) we allocate to the stack of the main
// thread. Useful tuning parameter in case the application runs out of memory.
OVERRIDE_CONST(main_thread_stack_size, 2500);

// Specifies the 48-bit OpenLCB node identifier. This must be unique for every
// hardware manufactured, so in production this should be replaced by some
// easily incrementable method.
extern const openlcb::NodeID NODE_ID;

// Sets up a comprehensive OpenLCB stack for a single virtual node. This stack
// contains everything needed for a usual peripheral node -- all
// CAN-bus-specific components, a virtual node, PIP, SNIP, Memory configuration
// protocol, ACDI, CDI, a bunch of memory spaces, etc.
openlcb::SimpleCanStack stack(NODE_ID);

// ConfigDef comes from config.hxx and is specific to the particular device and
// target. It defines the layout of the configuration memory space and is also
// used to generate the cdi.xml file. Here we instantiate the configuration
// layout. The argument of offset zero is ignored and will be removed later.
openlcb::ConfigDef cfg(0);
// Defines weak constants used by the stack to tell it which device contains
// the volatile configuration information. This device name appears in
// HwInit.cxx that creates the device drivers.
extern const char *const openlcb::CONFIG_FILENAME = "/ffs/eeprom";
// The size of the memory space to export over the above device.
extern const size_t openlcb::CONFIG_FILE_SIZE =
    cfg.seg().size() + cfg.seg().offset();
static_assert(openlcb::CONFIG_FILE_SIZE <= 24000, "Need to adjust eeprom size");
// The SNIP user-changeable information in also stored in the above eeprom
// device. In general this could come from different eeprom segments, but it is
// simpler to keep them together.
extern const char *const openlcb::SNIP_DYNAMIC_FILENAME =
    openlcb::CONFIG_FILENAME;


// Instantiates the actual producer and consumer objects for the given GPIO
// pins from above. The ConfiguredConsumer class takes care of most of the
// complicated setup and operation requirements. We need to give it the virtual
// node pointer, the configuration configuration from the CDI definition, and
// the hardware pin definition. The virtual node pointer comes from the stack
// object. The configuration structure comes from the CDI definition object,
// segment 'seg', in which there is a repeated group 'consumers', and we assign
// the individual entries to the individual consumers. Each consumer gets its
// own GPIO pin.
openlcb::ConfiguredConsumer consumer_green(
    stack.node(), cfg.seg().nucleo_onboard().green_led(), LED_GREEN_Pin());

// Similar syntax for the producers.
openlcb::ConfiguredProducer producer_sw1(
    stack.node(), cfg.seg().nucleo_onboard().user_btn(), SW_USER_Pin());


// The producers need to be polled repeatedly for changes and to execute the
// debouncing algorithm. This class instantiates a refreshloop and adds the two
// producers to it.
openlcb::RefreshLoop loop(
    stack.node(), {producer_sw1.polling()});

/*
// List of GPIO objects that will be used for the output pins. You should keep
// the constexpr declaration, because it will produce a compile error in case
// the list of pointers cannot be compiled into a compiler constant and thus
// would be placed into RAM instead of ROM.
constexpr const Gpio *const kDirectGpio[] = {
    TDRV1_Pin::instance(), TDRV2_Pin::instance(), //
    TDRV3_Pin::instance(), TDRV4_Pin::instance(), //
    TDRV5_Pin::instance(), TDRV6_Pin::instance(), //
    TDRV7_Pin::instance(), TDRV8_Pin::instance()  //
};

openlcb::MultiConfiguredConsumer direct_consumers(stack.node(), kDirectGpio,
    ARRAYSIZE(kDirectGpio), cfg.seg().direct_consumers());

// servoPwmCountPerMs defined in hardware.hxx.
// PWM* servo_channels[] defined in HwInit.cxx
openlcb::ServoConsumer srv0(
    stack.node(), cfg.seg().servo_consumers().entry<0>(),
    servoPwmCountPerMs, servo_channels[0]);
openlcb::ServoConsumer srv1(
    stack.node(), cfg.seg().servo_consumers().entry<1>(),
    servoPwmCountPerMs, servo_channels[1]);
openlcb::ServoConsumer srv2(
    stack.node(), cfg.seg().servo_consumers().entry<2>(),
    servoPwmCountPerMs, servo_channels[2]);
openlcb::ServoConsumer srv3(
    stack.node(), cfg.seg().servo_consumers().entry<3>(),
    servoPwmCountPerMs, servo_channels[3]);

*/

class FactoryResetHelper : public DefaultConfigUpdateListener {
public:
    UpdateAction apply_configuration(int fd, bool initial_load,
                                     BarrierNotifiable *done) OVERRIDE {
        AutoNotify n(done);
        return UPDATED;
    }

    void factory_reset(int fd) override
    {
        cfg.userinfo().name().write(fd, "Nucleo Logic board");
        cfg.userinfo().description().write(
            fd, "Logic board.");
    }
} factory_reset_helper;


#if NUM_EXTBOARDS > 0

Executor<1> io_executor("io_thread", 1, 1300);
Service io_service(&io_executor);

MCP23017 exp0(&io_executor, 0, 0, 0);
MCP23017 exp1(&io_executor, 0, 0, 1);

constexpr const MCP23017Gpio IOEXT0_A0(&exp0, MCP23017::PORTA, 0);
constexpr const MCP23017Gpio IOEXT0_A1(&exp0, MCP23017::PORTA, 1);
constexpr const MCP23017Gpio IOEXT0_A2(&exp0, MCP23017::PORTA, 2);
constexpr const MCP23017Gpio IOEXT0_A3(&exp0, MCP23017::PORTA, 3);
constexpr const MCP23017Gpio IOEXT0_A4(&exp0, MCP23017::PORTA, 4);
constexpr const MCP23017Gpio IOEXT0_A5(&exp0, MCP23017::PORTA, 5);
constexpr const MCP23017Gpio IOEXT0_A6(&exp0, MCP23017::PORTA, 6);
constexpr const MCP23017Gpio IOEXT0_A7(&exp0, MCP23017::PORTA, 7);

constexpr const MCP23017Gpio IOEXT0_B0(&exp0, MCP23017::PORTB, 0);
constexpr const MCP23017Gpio IOEXT0_B1(&exp0, MCP23017::PORTB, 1);
constexpr const MCP23017Gpio IOEXT0_B2(&exp0, MCP23017::PORTB, 2);
constexpr const MCP23017Gpio IOEXT0_B3(&exp0, MCP23017::PORTB, 3);
constexpr const MCP23017Gpio IOEXT0_B4(&exp0, MCP23017::PORTB, 4);
constexpr const MCP23017Gpio IOEXT0_B5(&exp0, MCP23017::PORTB, 5);
constexpr const MCP23017Gpio IOEXT0_B6(&exp0, MCP23017::PORTB, 6);
constexpr const MCP23017Gpio IOEXT0_B7(&exp0, MCP23017::PORTB, 7);

constexpr const MCP23017Gpio IOEXT1_A0(&exp1, MCP23017::PORTA, 0);
constexpr const MCP23017Gpio IOEXT1_A1(&exp1, MCP23017::PORTA, 1);
constexpr const MCP23017Gpio IOEXT1_A2(&exp1, MCP23017::PORTA, 2);
constexpr const MCP23017Gpio IOEXT1_A3(&exp1, MCP23017::PORTA, 3);
constexpr const MCP23017Gpio IOEXT1_A4(&exp1, MCP23017::PORTA, 4);
constexpr const MCP23017Gpio IOEXT1_A5(&exp1, MCP23017::PORTA, 5);
constexpr const MCP23017Gpio IOEXT1_A6(&exp1, MCP23017::PORTA, 6);
constexpr const MCP23017Gpio IOEXT1_A7(&exp1, MCP23017::PORTA, 7);

constexpr const MCP23017Gpio IOEXT1_B0(&exp1, MCP23017::PORTB, 0);
constexpr const MCP23017Gpio IOEXT1_B1(&exp1, MCP23017::PORTB, 1);
constexpr const MCP23017Gpio IOEXT1_B2(&exp1, MCP23017::PORTB, 2);
constexpr const MCP23017Gpio IOEXT1_B3(&exp1, MCP23017::PORTB, 3);
constexpr const MCP23017Gpio IOEXT1_B4(&exp1, MCP23017::PORTB, 4);
constexpr const MCP23017Gpio IOEXT1_B5(&exp1, MCP23017::PORTB, 5);
constexpr const MCP23017Gpio IOEXT1_B6(&exp1, MCP23017::PORTB, 6);
constexpr const MCP23017Gpio IOEXT1_B7(&exp1, MCP23017::PORTB, 7);

constexpr const Gpio *const kPortExt0[] = {
    &IOEXT0_A0, &IOEXT0_A1, &IOEXT0_A2, &IOEXT0_A3, //
    &IOEXT0_A4, &IOEXT0_A5, &IOEXT0_A6, &IOEXT0_A7, //
    &IOEXT0_B0, &IOEXT0_B1, &IOEXT0_B2, &IOEXT0_B3, //
    &IOEXT0_B4, &IOEXT0_B5, &IOEXT0_B6, &IOEXT0_B7, //
    &IOEXT1_A0, &IOEXT1_A1, &IOEXT1_A2, &IOEXT1_A3, //
    &IOEXT1_A4, &IOEXT1_A5, &IOEXT1_A6, &IOEXT1_A7, //
    &IOEXT1_B0, &IOEXT1_B1, &IOEXT1_B2, &IOEXT1_B3, //
    &IOEXT1_B4, &IOEXT1_B5, &IOEXT1_B6, &IOEXT1_B7  //
};

openlcb::MultiConfiguredPC ext0_pcs(
    stack.node(), kPortExt0, ARRAYSIZE(kPortExt0), cfg.seg().ext0_pc());

#endif // if num extboards > 0

openlcb::RefreshLoop loopab(stack.node(),
    {
#if NUM_EXTBOARDS > 0
        ext0_pcs.polling(),                           //
#endif
    });


// =============== LOGIC BLOCKS ===================
logic::OlcbVariableFactory logic_blocks(stack.node(), cfg.seg().logic());


/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[])
{
    stack.create_config_file_if_needed(cfg.seg().internal_config(),
                                       openlcb::CANONICAL_VERSION,
                                       openlcb::CONFIG_FILE_SIZE);

#if 0 &&(NUM_EXTBOARDS > 0)
    {
        int i2cfd = ::open("/dev/i2c0", O_RDWR);
        exp0.init(i2cfd);
        exp1.init(i2cfd);
    }
#endif

    // The necessary physical ports must be added to the stack.
    //
    // It is okay to enable multiple physical ports, in which case the stack
    // will behave as a bridge between them. For example enabling both the
    // physical CAN port and the USB port will make this firmware act as an
    // USB-CAN adapter in addition to the producers/consumers created above.
    //
    // If a port is enabled, it must be functional or else the stack will
    // freeze waiting for that port to send the packets out.
#if defined(HAVE_PHYSICAL_CAN_PORT)
    stack.add_can_port_select("/dev/can0");
#endif
#if defined(SNIFF_ON_USB)
    stack.add_gridconnect_port("/dev/serUSB0");
#endif
#if defined(SNIFF_ON_SERIAL)
    stack.add_gridconnect_port("/dev/ser0");
#endif

    // This command donates the main thread to the operation of the
    // stack. Alternatively the stack could be started in a separate stack and
    // then application-specific business logic could be executed ion a busy
    // loop in the main thread.
    stack.loop_executor();
    return 0;
}
