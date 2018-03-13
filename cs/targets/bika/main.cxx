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

// Configuration file access.
int config_fd = -1;

class PeriodicAction : public StateFlowBase {
 public:
  PeriodicAction() : StateFlowBase(stack.service()) {
  }

  void set_periodic(unsigned time_on_msec, unsigned time_off_msec) {
    time_on_msec_ = time_on_msec;
    time_off_msec_ = time_off_msec;
    need_stop_ = 0;
    if (is_terminated()) {
      start_flow(STATE(state_on));
      return;
    }
    timer_.ensure_triggered();
    wait_and_call(STATE(state_on));
  }

  void set_constant(bool is_on) {
    need_stop_ = 1;
    set_state(is_on);
  }
  
 private:
  Action state_on() {
    if (need_stop_) {
      return exit();
    }
    set_state(true);
    return sleep_and_call(&timer_, MSEC_TO_NSEC(time_on_msec_),
                          STATE(state_off));
  }

  Action state_off() {
    if (need_stop_) {
      return exit();
    }
    set_state(false);
    return sleep_and_call(&timer_, MSEC_TO_NSEC(time_off_msec_),
                          STATE(state_on));
  }

  void __attribute__((noinline)) set_state(bool is_on) {
    LED_BLUE_Pin::set(is_on);
    MAGNET_Pin::set(is_on);
    unsigned servo_tgt = (servo_min + servo_max) / 2;
    if (is_on) {
      if (config_fd >= 0) {
        servo_tgt = cfg.seg().servo_max().read(config_fd);
      }
    } else {
      if (config_fd >= 0) {
        servo_tgt = cfg.seg().servo_min().read(config_fd);
      }
    }
    servo_pwm.set_duty(servo_tgt);
    MAGNET_Pin::set(is_on);
  }
  
  unsigned time_on_msec_;
  unsigned time_off_msec_;
  unsigned need_stop_ : 1;
  StateFlowTimer timer_{this};
} g_periodic_action;


// The producers need to be polled repeatedly for changes and to execute the
// debouncing algorithm. This class instantiates a refreshloop and adds the two
// producers to it.
openlcb::RefreshLoop loop(
    stack.node(), {&pset, &b1, &b2, &b3, &b4, &b5, &b6, &b7});

enum SetState {
  NORMAL_OP = 0,
  SET_DUTY = 1,
  SET_SERVO_MIN = 2,
  SET_SERVO_MAX = 3,
  SET_MAX = 3
};

SetState set_state;
// On the range of 0..100 the time ratio of on/off
uint8_t duty_cycle = 50;
unsigned period_msec = 2000;

class ConfigApply : public DefaultConfigUpdateListener {
 public:
  UpdateAction apply_configuration(
      int fd, bool initial_load, BarrierNotifiable *done) override {
    config_fd = fd;
    return UPDATED;
  }

  void factory_reset(int fd) override {
    config_fd = fd;
    CDI_FACTORY_RESET(cfg.seg().servo_min);
    CDI_FACTORY_RESET(cfg.seg().servo_max);

    cfg.userinfo().name().write(fd, "Bika vezerlo");
    cfg.userinfo().description().write(fd, "");
  }
} g_config_apply;


void handler_cb(const openlcb::EventRegistryEntry &registry_entry,
                openlcb::EventReport *report, BarrierNotifiable *done) {
  resetblink(0);
  unsigned arg = registry_entry.user_arg;
  if (arg == 16) {
    // set button pressed.
    // We turn off output.
    g_periodic_action.set_constant(false);

    // Increment set state.
    int state = set_state;
    ++state;
    if (state > SET_MAX) {
      state = NORMAL_OP;
    }
    set_state = (SetState)state;
    switch(set_state) {
      case NORMAL_OP:
        resetblink(0);
        break;
      case SET_DUTY:
        resetblink(0x800);
        break;
      case SET_SERVO_MIN:
        resetblink(0x8002);
        break;
      case SET_SERVO_MAX:
        resetblink(0x800A);
        break;
    }
    return;
  }

  if (set_state != NORMAL_OP) {
    // User is changing a setting. The set was pressed
    switch(set_state) {
      case SET_DUTY: {
        // 0 = duty cycle of 0
        // 8 = duty cycle of 100
        duty_cycle = arg * 100 / 8;
        break;
      }
      case SET_SERVO_MIN: {
        // 1 = min
        // 7 = max
        unsigned value = (servo_min * (7 - arg) + servo_max * (arg - 1)) / 7;
        cfg.seg().servo_min().write(config_fd, value);
        break;
      }
      case SET_SERVO_MAX: {
        // 1 = min
        // 7 = max
        unsigned value = (servo_min * (7 - arg) + servo_max * (arg - 1)) / 7;
        cfg.seg().servo_max().write(config_fd, value);
        break;
      }
      case NORMAL_OP:
      default:
        break;
    }
    set_state = NORMAL_OP;
    g_periodic_action.set_constant(false);
    resetblink(0);
  }
  
  bool req_periodic = false;
  switch (registry_entry.user_arg) {
    case 1:
      period_msec = 300;
      req_periodic = true;
      break;
    case 2:
      period_msec = 700;
      req_periodic = true;
      break;
    case 3:
      period_msec = 1500;
      req_periodic = true;
      break;
    case 4:
      period_msec = 3000;
      req_periodic = true;
      break;

    case 5:
      g_periodic_action.set_constant(false);
      break;
    case 6:
      req_periodic = true;
      break;
    case 7:
      g_periodic_action.set_constant(true);
      break;
    default:
      resetblink(0x8000CC);
  }
  if (req_periodic) {
    g_periodic_action.set_periodic(period_msec * duty_cycle / 100,
                                   period_msec * (100 - duty_cycle) / 100);
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

    eventhandler.add_entry(EVBASE, 16);
    
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
