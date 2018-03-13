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

#include "openlcb/SimpleStack.hxx"
#include "openlcb/ConfiguredConsumer.hxx"
#include "openlcb/MultiConfiguredConsumer.hxx"
#include "openlcb/ConfiguredProducer.hxx"
#include "openlcb/CallbackEventHandler.hxx"

#include "freertos_drivers/ti/TivaGPIO.hxx"
#include "freertos_drivers/common/BlinkerGPIO.hxx"
#include "freertos_drivers/common/PersistentGPIO.hxx"
#include "config.hxx"
#include "hardware.hxx"
#include "TivaPWM.hxx"

// These preprocessor symbols are used to select which physical connections
// will be enabled in the main(). See @ref appl_main below.
#define SNIFF_ON_SERIAL
//#define SNIFF_ON_USB
//#define HAVE_PHYSICAL_CAN_PORT

// Changes the default behavior by adding a newline after each gridconnect
// packet. Makes it easier for debugging the raw device.
OVERRIDE_CONST(gc_generate_newlines, 1);
// Specifies how much RAM (in bytes) we allocate to the stack of the main
// thread. Useful tuning parameter in case the application runs out of memory.
OVERRIDE_CONST(main_thread_stack_size, 2500);

// Specifies the 48-bit OpenLCB node identifier. This must be unique for every
// hardware manufactured, so in production this should be replaced by some
// easily incrementable method.
extern const openlcb::NodeID NODE_ID = 0x0501010114F9ULL;

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
extern const char *const openlcb::CONFIG_FILENAME = "/dev/eeprom";
// The size of the memory space to export over the above device.
extern const size_t openlcb::CONFIG_FILE_SIZE =
    cfg.seg().size() + cfg.seg().offset();
static_assert(openlcb::CONFIG_FILE_SIZE <= 300, "Need to adjust eeprom size");
// The SNIP user-changeable information in also stored in the above eeprom
// device. In general this could come from different eeprom segments, but it is
// simpler to keep them together.
extern const char *const openlcb::SNIP_DYNAMIC_FILENAME =
    openlcb::CONFIG_FILENAME;
 
constexpr openlcb::EventId EVBASE = 0x0501010114F90000;

//openlcb::GPIOBit rset();
QuiesceDebouncer::Options opts(3);
typedef openlcb::PolledProducer<QuiesceDebouncer, openlcb::GPIOBit> BtnProducer;

BtnProducer pset(opts, stack.node(), EVBASE, EVBASE + 1, RSET_Pin::instance()); 
BtnProducer b1(opts, stack.node(), EVBASE+4, EVBASE + 5, RB1_Pin::instance());
BtnProducer b2(opts, stack.node(), EVBASE+6, EVBASE + 7, RB2_Pin::instance());
BtnProducer b3(opts, stack.node(), EVBASE+8, EVBASE + 9, RB3_Pin::instance());
BtnProducer b4(opts, stack.node(), EVBASE+10, EVBASE + 11, RB4_Pin::instance());
BtnProducer b5(opts, stack.node(), EVBASE+12, EVBASE + 13, RB5_Pin::instance());
BtnProducer b6(opts, stack.node(), EVBASE+14, EVBASE + 15, RB6_Pin::instance());
BtnProducer b7(opts, stack.node(), EVBASE+16, EVBASE + 17, RB7_Pin::instance());

//openlcb::GPIOBit rled_bit(stack.node(), EVBASE, EVBASE + 1, RLED_Pin::instance());

//openlcb::BitEventConsumer light_consumer(&rled_bit);

extern TivaPWM servo_pwm;

constexpr unsigned servo_min = 80000000 / 1000 * 1.5;
constexpr unsigned servo_max = 80000000 / 1000 * 2.5;

constexpr unsigned servo_med1 = 80000000 / 1000 * 1.9;
constexpr unsigned servo_med2 = 80000000 / 1000 * 2.3;

class PeriodicAction : public StateFlowBase {
 public:
  PeriodicAction() : StateFlowBase(stack.service()) {
    is_sleeping_ = 0;
    is_waiting_ = 1;
  }

  void set_periodic(unsigned time_on_msec, unsigned time_off_msec) {
    if ((time_on_msec == time_on_msec_) && (time_off_msec == time_off_msec_)) {
       // nothing to do.
      return;
    }
    time_on_msec_ = time_on_msec;
    time_off_msec_ = time_off_msec;
    need_stop_ = 0;
    if (is_sleeping_) {
      timer_.trigger();
      is_sleeping_ = 0;
    } else if (is_waiting_) {
      start_flow(STATE(state_on));
      is_waiting_ = 0;
    }
  }

  void set_constant(bool is_on) {
    need_stop_ = 1;
    set_state(is_on);
  }
  
 private:
  Action state_on() {
    if (need_stop_) {
      is_waiting_ = 1;
      return wait();
    }
    set_state(true);
    is_sleeping_ = 1;
    return sleep_and_call(&timer_, MSEC_TO_NSEC(time_on_msec_),
                          STATE(state_off));
  }

  Action state_off() {
    if (need_stop_) {
      is_waiting_ = 1;
      return wait();
    }
    set_state(false);
    is_sleeping_ = 1;
    return sleep_and_call(&timer_, MSEC_TO_NSEC(time_off_msec_),
                          STATE(state_on));
  }

  void set_state(bool is_on) {
    if (is_on) {
      servo_pwm.set_duty(servo_med1);
    } else {
      servo_pwm.set_duty(servo_med2);
    }
  }
  
  unsigned time_on_msec_;
  unsigned time_off_msec_;
  unsigned need_stop_ : 1;
  unsigned is_waiting_ : 1;
  unsigned is_sleeping_ : 1;
  StateFlowTimer timer_{this};
} g_periodic_action;


// The producers need to be polled repeatedly for changes and to execute the
// debouncing algorithm. This class instantiates a refreshloop and adds the two
// producers to it.
openlcb::RefreshLoop loop(
    stack.node(), {producer_sw1.polling(), producer_sw2.polling(), &pset, &b1, &b2, &b3, &b4, &b5, &b6, &b7});



void handler_cb(const openlcb::EventRegistryEntry &registry_entry,
                openlcb::EventReport *report, BarrierNotifiable *done) {
  resetblink(0);
  switch (registry_entry.user_arg) {
    case 1:
      servo_pwm.set_duty(servo_min);
      break;
    case 2:
      servo_pwm.set_duty(servo_med1);
      break;
    case 3:
      servo_pwm.set_duty(servo_med2);
      break;
    case 4:
      servo_pwm.set_duty(servo_max);
      break;

    case 5:
      g_periodic_action.set_constant(true);
      break;
    case 6:
      g_periodic_action.set_periodic(1000, 1000);
      break;
    case 7:
      g_periodic_action.set_constant(false);
      break;
      
    default:
      resetblink(0x80002);
  }
}

openlcb::CallbackEventHandler eventhandler(stack.node(), &handler_cb, nullptr);

/// Waits three seconds and turns off the green LED. Useful for detecting
/// restarts.
class StartupTimer : public Timer {
 public:
  StartupTimer() : Timer(stack.executor()->active_timers()) {
    start(SEC_TO_NSEC(3));
  }

  long long timeout() override {
    LED_GREEN_Pin::set(false);
    return DELETE;
  }
};

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[])
{
    stack.check_version_and_factory_reset(
        cfg.seg().internal_config(), openlcb::CANONICAL_VERSION, false);

    eventhandler.add_entry(EVBASE+5, 1);
    eventhandler.add_entry(EVBASE+7, 2);
    eventhandler.add_entry(EVBASE+9, 3);
    eventhandler.add_entry(EVBASE+11, 4);
    eventhandler.add_entry(EVBASE+13, 5);
    eventhandler.add_entry(EVBASE+15, 6);
    eventhandler.add_entry(EVBASE+17, 7);
    
    // Restores pin states from EEPROM.
    //PinRed.restore();
    //PinGreen.restore();
    //PinBlue.restore();
    //PinRed.write(Gpio::LOW); 
    //PinGreen.write(Gpio::LOW); 
    //PinBlue.write(Gpio::LOW);
    LED_RED_Pin::set(false);
    LED_GREEN_Pin::set(true);
    LED_BLUE_Pin::set(false);

    new StartupTimer();
    
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
