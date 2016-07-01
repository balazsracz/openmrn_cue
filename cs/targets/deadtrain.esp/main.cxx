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
 * Dead-rail train application.
 *
 * @author Balazs Racz
 * @date 3 Aug 2013
 */

#include <stdio.h>
#include <unistd.h>

#include "dcc/Defs.hxx"
#include "executor/StateFlow.hxx"
#include "freertos_drivers/common/BlinkerGPIO.hxx"
#include "freertos_drivers/common/DummyGPIO.hxx"
#include "freertos_drivers/esp8266/Esp8266Gpio.hxx"
#include "freertos_drivers/esp8266/TimerBasedPwm.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/SimpleStack.hxx"
#include "nmranet/TractionTrain.hxx"
#include "nmranet/TrainInterface.hxx"
#include "os/os.h"
#include "utils/ESPWifiClient.hxx"
#include "utils/GpioInitializer.hxx"
#include "utils/blinker.h"

#include "config.hxx"

extern "C" {
#include <gpio.h>
#include <osapi.h>
#include <user_interface.h>
extern void ets_delay_us(uint32_t us);
#define os_delay_us ets_delay_us
}

#include "nmranet/TrainInterface.hxx"
#include "hardware.hxx"

struct SpeedRequest {
  SpeedRequest() { reset(); }
  nmranet::SpeedType speed_;
  uint8_t emergencyStop_ : 1;
  uint8_t doKick_ : 1;
  void reset() {
    speed_ = 0.0;
    emergencyStop_ = 0;
    doKick_ = 0;
  }
};

class SpeedController : public StateFlow<Buffer<SpeedRequest>, QList<2>>,
                        private DefaultConfigUpdateListener {
 public:
  SpeedController(Service *s, MotorControl mpar)
      : StateFlow<Buffer<SpeedRequest>, QList<2>>(s),
        mpar_(mpar),
        isMoving_(0),
        lastDirMotAHi_(0),
        enableKick_(0),
        kickRunning_(0) {
    HW::MOT_A_HI_Pin::set_off();
    HW::MOT_B_HI_Pin::set_off();
    pwm_.enable();
  }

  void call_speed(nmranet::Velocity speed) {
    auto *b = alloc();
    b->data()->speed_ = speed;
    send(b, 1);
  }

  void call_estop() {
    auto *b = alloc();
    b->data()->emergencyStop_ = 1;
    send(b, 0);
  }

  void call_kick() {
    kickRunning_ = 0;
    auto *b = alloc();
    b->data()->doKick_ = 1;
    send(b, 0);
  }

 private:
  static constexpr bool invertLow_ = HW::invertLow;
  /// set(LOW_ON) will turn on the low driver.
  static constexpr bool LOW_ON = !invertLow_;
  /// set(LOW_OFF) will turn on the low driver.
  static constexpr bool LOW_OFF = invertLow_;

  long long speed_to_fill_rate(nmranet::SpeedType speed, long long period) {
    int fill_rate = speed.mph();
    if (fill_rate >= 128) fill_rate = 128;
    // Let's do a 1khz
    long long fill = (period * fill_rate) >> 7;
    if (invertLow_) fill = period - fill;
    return fill;
  }

  Action entry() override {
    if (req()->emergencyStop_) {
      pwm_.set_off();
      isMoving_ = 0;
      HW::MOT_A_HI_Pin::set_off();
      HW::MOT_B_HI_Pin::set_off();
      release();
      return sleep_and_call(&timer_, MSEC_TO_NSEC(1), STATE(eoff_enablelow));
    }
    if (req()->doKick_ && isMoving_) {
      pwm_.pause(kickDurationNsec_);
      if (lastDirMotAHi_) {
        HW::MOT_B_LO_Pin::set(LOW_ON);
      } else {
        HW::MOT_A_LO_Pin::set(LOW_ON);
      }
      schedule_kick();
      return release_and_exit();
    }
    // Check if we need to change the direction.
    bool desired_dir =
        (req()->speed_.direction() == nmranet::SpeedType::FORWARD);
    if (lastDirMotAHi_ != desired_dir) {
      pwm_.set_off();
      HW::MOT_B_HI_Pin::set_off();
      HW::MOT_A_HI_Pin::set_off();
      return sleep_and_call(&timer_, MSEC_TO_NSEC(1), STATE(do_speed));
    }
    return call_immediately(STATE(do_speed));
  }

  Action do_speed() {
    // We set the pins explicitly for safety
    bool desired_dir =
        (req()->speed_.direction() == nmranet::SpeedType::FORWARD);
    int lo_pin;
    if (desired_dir) {
      HW::MOT_B_HI_Pin::set_off();
      HW::MOT_A_LO_Pin::set(LOW_OFF);
      lo_pin = HW::MOT_B_LO_Pin::PIN;
      HW::MOT_A_HI_Pin::set_on();
    } else {
      HW::MOT_A_HI_Pin::set_off();
      HW::MOT_B_LO_Pin::set(LOW_OFF);
      lo_pin = HW::MOT_A_LO_Pin::PIN;
      HW::MOT_B_HI_Pin::set_on();
    }

    long long fill = speed_to_fill_rate(req()->speed_, period_);
    if (fill) {
      isMoving_ = 1;
    } else {
      isMoving_ = 0;
    }
    pwm_.old_set_state(lo_pin, period_, fill);
    lastDirMotAHi_ = desired_dir;
    schedule_kick();
    return release_and_exit();
  }

  Action eoff_enablelow() {
    // By shorting both motor outputs to ground we turn it to actively
    // brake.
    HW::MOT_A_LO_Pin::set(LOW_ON);
    HW::MOT_B_LO_Pin::set(LOW_ON);
    return exit();
  }

  SpeedRequest *req() { return message()->data(); }

  void factory_reset(int fd) override {
    mpar_.pwm_frequency().write(fd,
                                mpar_.pwm_frequency_options().defaultvalue());
    mpar_.enable_kick().write(fd, mpar_.enable_kick_options().defaultvalue());
    mpar_.kick_delay().write(fd, mpar_.kick_delay_options().defaultvalue());
    mpar_.kick_length().write(fd, mpar_.kick_length_options().defaultvalue());
  }

  UpdateAction apply_configuration(int fd, bool initial_load,
                                   BarrierNotifiable *done) override {
    AutoNotify an(done);
    auto config_freq = mpar_.pwm_frequency().read(fd);
    printf("pwm freq = %d\n", config_freq);
    period_ = 1000000000 / config_freq;
    enableKick_ = mpar_.enable_kick().read(fd) ? 1 : 0;
    auto kick_length_msec =
        mpar_.kick_length_options().clip(mpar_.kick_length().read(fd));
    kickDurationNsec_ = MSEC_TO_NSEC(kick_length_msec);
    auto kick_delay_msec =
        mpar_.kick_delay_options().clip(mpar_.kick_delay().read(fd));
    kickDelayNsec_ = MSEC_TO_NSEC(kick_delay_msec);
    return UPDATED;
  }

  void schedule_kick() {
    if (!enableKick_ || kickRunning_) return;
    kickRunning_ = 1;
    kickTimer_.start(kickDelayNsec_);
  }

  /// Helper class to send repeated "kick" messages to this state flow.
  ///
  /// @param parent owner.
  class KickTimer : public ::Timer {
   public:
    KickTimer(SpeedController *parent)
        : Timer(parent->service()->executor()->active_timers()),
          parent_(parent) {}

   private:
    /// Sends the kick message.
    long long timeout() override {
      parent_->call_kick();
      return NONE;
    }

    SpeedController *parent_;  ///< owner
  } kickTimer_{this};

  /// Parameter set in the configuration space.
  MotorControl mpar_;
  /// PWM period in nanoseconds.
  long long period_ =
      1000000000 / MotorControl::pwm_frequency_options().defaultvalue();
  /// How long an individual motor kick should be.
  long long kickDurationNsec_ = 0;
  /// How much time to wait between motor kicks.
  long long kickDelayNsec_ = 0;
  /// Helper class to control the hardware timer.
  TimerBasedPwm pwm_;
  /// Timer structure for executing delays in the state flow.
  StateFlowTimer timer_{this};
  /// 1 if the motor is receiving power.
  unsigned isMoving_ : 1;
  /// 1 if the current direction is set so that the MOT_A output is HIGH.
  unsigned lastDirMotAHi_ : 1;
  /// 1 if the configuration enabled the kick timer.
  unsigned enableKick_ : 1;
  /// 1 if the kick timer is running.
  unsigned kickRunning_ : 1;
};

extern SpeedController g_speed_controller;

class ESPHuzzahTrain : public nmranet::TrainImpl {
 public:
  ESPHuzzahTrain() {
    HW::GpioInit::hw_init();
    HW::LIGHT_FRONT_Pin::set(false);
    HW::LIGHT_BACK_Pin::set(false);
  }

  void set_speed(nmranet::SpeedType speed) override {
    lastSpeed_ = speed;
    g_speed_controller.call_speed(speed);
    if (f0) {
      if (speed.direction() == nmranet::SpeedType::FORWARD) {
        HW::LIGHT_FRONT_Pin::set(true);
        HW::LIGHT_BACK_Pin::set(false);
      } else {
        HW::LIGHT_BACK_Pin::set(true);
        HW::LIGHT_FRONT_Pin::set(false);
      }
    }
  }
  /** Returns the last set speed of the locomotive. */
  nmranet::SpeedType get_speed() override { return lastSpeed_; }

  /** Sets the train to emergency stop. */
  void set_emergencystop() override {
    // g_speed_controller.call_estop();
    lastSpeed_.set_mph(0);  // keeps direction
  }

  /** Sets the value of a function.
   * @param address is a 24-bit address of the function to set. For legacy DCC
   * locomotives, see @ref TractionDefs for the address definitions (0=light,
   * 1-28= traditional function buttons).
   * @param value is the function value. For binary functions, any non-zero
   * value sets the function to on, zero sets it to off.*/
  void set_fn(uint32_t address, uint16_t value) override {
    switch (address) {
      case 0:
        f0 = value;
        if (!value) {
          HW::LIGHT_FRONT_Pin::set(false);
          HW::LIGHT_BACK_Pin::set(false);
        } else if (lastSpeed_.direction() == nmranet::SpeedType::FORWARD) {
          HW::LIGHT_FRONT_Pin::set(true);
          HW::LIGHT_BACK_Pin::set(false);
        } else {
          HW::LIGHT_BACK_Pin::set(true);
          HW::LIGHT_FRONT_Pin::set(false);
        }
        break;
      case 1:
        /*if (value) {
            analogWrite(2, 700);
        } else {
            analogWrite(2, 100);
            }*/
        f1 = value;
        HW::F1_Pin::set(!value);
        break;
    }
  }

  /** @returns the value of a function. */
  uint16_t get_fn(uint32_t address) override {
    switch (address) {
      case 0:
        return f0 ? 1 : 0;
        break;
      case 1:
        return f1 ? 1 : 0;
        break;
    }
    return 0;
  }

  uint32_t legacy_address() override { return 883; }

  /** @returns the type of legacy protocol in use. */
  dcc::TrainAddressType legacy_address_type() override {
    return dcc::TrainAddressType::DCC_LONG_ADDRESS;
  }

 private:
  nmranet::SpeedType lastSpeed_ = 0.0;
  bool f0 = false;
  bool f1 = false;
};

const char kFdiXml[] =
    R"(<?xml version='1.0' encoding='UTF-8'?>
<?xml-stylesheet type='text/xsl' href='xslt/fdi.xsl'?>
<fdi xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:noNamespaceSchemaLocation='http://openlcb.org/trunk/prototypes/xml/schema/fdi.xsd'>
<segment space='249'><group><name/>
<function size='1' kind='binary'>
<name>Light</name>
<number>0</number>
</function>
<function size='1' kind='momentary'>
<name>Blue</name>
<number>1</number>
</function>
</group></segment></fdi>)";

ESPHuzzahTrain trainImpl;
nmranet::ConfigDef cfg(0);

extern nmranet::NodeID NODE_ID;

namespace nmranet {

extern const char *const CONFIG_FILENAME = "openlcb_config";
// The size of the memory space to export over the above device.
extern const size_t CONFIG_FILE_SIZE = cfg.seg().size() + cfg.seg().offset();
extern const char *const SNIP_DYNAMIC_FILENAME = CONFIG_FILENAME;

}  // namespace nmranet

nmranet::SimpleTrainCanStack stack(&trainImpl, kFdiXml, NODE_ID);
SpeedController g_speed_controller(stack.service(), cfg.seg().motor_control());

extern "C" {
extern char WIFI_SSID[];
extern char WIFI_PASS[];
extern char WIFI_HUB_HOSTNAME[];
extern int WIFI_HUB_PORT;
}

class TestBlinker : public StateFlowBase {
 public:
  TestBlinker() : StateFlowBase(stack.service()) {
    start_flow(STATE(doo));
    pwm_.enable();
  }

 private:
  Action doo() {
    if (isOn_) {
      isOn_ = false;
      pwm_.old_set_state(2, USEC_TO_NSEC(1000), USEC_TO_NSEC(800));
    } else {
      isOn_ = true;
      pwm_.old_set_state(2, USEC_TO_NSEC(1000), USEC_TO_NSEC(200));
    }
    return sleep_and_call(&timer_, MSEC_TO_NSEC(500), STATE(doo));
  }

  TimerBasedPwm pwm_;
  StateFlowTimer timer_{this};
  bool isOn_{true};
};

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[]) {
  gpio16_output_conf();
  gpio16_output_set(1);
  printf("pinout: B hi %d; B lo %d; A hi %d; A lo %d;\n", HW::MOT_B_HI_Pin::PIN,
         HW::MOT_B_LO_Pin::PIN, HW::MOT_A_HI_Pin::PIN, HW::MOT_A_LO_Pin::PIN);
  resetblink(1);
  if (true)
    stack.create_config_file_if_needed(cfg.seg().internal_data(),
                                       nmranet::EXPECTED_VERSION,
                                       nmranet::CONFIG_FILE_SIZE);

  new ESPWifiClient(WIFI_SSID, WIFI_PASS, stack.can_hub(), WIFI_HUB_HOSTNAME,
                    WIFI_HUB_PORT, 1200, []() {
                      resetblink(0);
                      // This will actually return due to the event-driven OS
                      // implementation of the stack.
                      stack.loop_executor();
                    });
  return 0;
}
