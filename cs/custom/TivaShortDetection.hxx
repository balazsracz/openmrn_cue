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
 * \file ShortDetection.hxx
 *
 * Control flow that use the ADC of the TivaWare library to detect short
 * circuit conditions.
 *
 * @author Balazs Racz
 * @date 23 Aug 2014
 */

#ifndef _TIVA_SHORTDETECTION_HXX_
#define _TIVA_SHORTDETECTION_HXX_

#include "driverlib/adc.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom_map.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"

#include "executor/StateFlow.hxx"
#include "utils/logging.h"
#include "dcc_control.hxx"
#include "DccHardware.hxx"

static const auto SHUTDOWN_CURRENT_AMPS = 1.5;
// ADC value at which we turn off the output.
static const unsigned SHUTDOWN_LIMIT = (SHUTDOWN_CURRENT_AMPS * 0.2 / 3.3) * 0xfff;

static const auto KILL_CURRENT_AMPS = 4.0;
// ADC value at which we turn off the output.
static const unsigned KILL_LIMIT = (KILL_CURRENT_AMPS * 0.2 / 3.3) * 0xfff;

// We try this many times to reenable after a short.
static const unsigned OVERCURRENT_RETRY = 3;
static const long long OVERCURRENT_RETRY_DELAY = MSEC_TO_NSEC(300);

template<class HW>
class TivaShortDetectionModule;

template<class HW>
class TivaShortDetectionModule : public StateFlowBase {
 public:
  TivaShortDetectionModule(Service* s, long long period)
      : StateFlowBase(s),
        timer_(this),
        period_(period),
        num_disable_tries_(0),
        num_overcurrent_tests_(0) {
    start_flow(STATE(start_timer));

    SysCtlPeripheralEnable(HW::ADC_PERIPH);
    SysCtlPeripheralEnable(HW::ADCPIN_PERIPH);

    GPIODirModeSet(HW::ADCPIN_BASE, HW::ADCPIN_PIN, GPIO_DIR_MODE_HW);
    GPIOPadConfigSet(HW::ADCPIN_BASE, HW::ADCPIN_PIN, GPIO_STRENGTH_2MA,
                     GPIO_PIN_TYPE_ANALOG);

    // Sets ADC to 16 MHz clock.
    //ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, 30);
    ADCHardwareOversampleConfigure(HW::ADC_BASE, 64);

    ADCSequenceConfigure(HW::ADC_BASE, HW::ADC_SEQUENCER, ADC_TRIGGER_PROCESSOR,
                         0);
    ADCSequenceStepConfigure(
        HW::ADC_BASE, HW::ADC_SEQUENCER, 0,
        HW::ADCPIN_CH | ADC_CTL_IE | ADC_CTL_SHOLD_256 | ADC_CTL_END);
    ADCSequenceEnable(HW::ADC_BASE, HW::ADC_SEQUENCER);

    ADCIntEnable(HW::ADC_BASE, HW::ADC_SEQUENCER);
    MAP_IntPrioritySet(HW::ADC_INTERRUPT, configKERNEL_INTERRUPT_PRIORITY);
    MAP_IntEnable(HW::ADC_INTERRUPT);
    next_report_ = 0;
  }

  ~TivaShortDetectionModule() {
  }

  void interrupt_handler() {
    ADCIntClear(HW::ADC_BASE, HW::ADC_SEQUENCER);
    notify_from_isr();
  }

 private:
  Action start_timer() {
    return sleep_and_call(&timer_, period_, STATE(start_conversion));
  }

  Action start_conversion() {
    ADCIntClear(HW::ADC_BASE, HW::ADC_SEQUENCER);
    ADCProcessorTrigger(HW::ADC_BASE, HW::ADC_SEQUENCER);
    return wait_and_call(STATE(conversion_done));
  }

  Action conversion_done() {
    uint32_t adc_value[1];
    adc_value[0] = 0;
    ADCSequenceDataGet(HW::ADC_BASE, HW::ADC_SEQUENCER, adc_value);
    if (adc_value[0] > KILL_LIMIT) {
      disable_dcc();
      LOG(INFO, "kill value: %04" PRIx32, adc_value[0]);
      return call_immediately(STATE(shorted));
    }
    if (adc_value[0] > SHUTDOWN_LIMIT) {
      if (++num_overcurrent_tests_ >= 5 || adc_value[0] > KILL_LIMIT) {
        disable_dcc();
        LOG(INFO, "%s value: %04" PRIx32,
            adc_value[0] <= KILL_LIMIT ? "disable" : "kill", adc_value[0]);
        ++num_disable_tries_;
        if (num_disable_tries_ < OVERCURRENT_RETRY) {
          return call_immediately(STATE(retry_wait));
        } else {
          return call_immediately(STATE(shorted));
        }
      } else {
        LOG(INFO, "overcurrent value: %04" PRIx32, adc_value[0]);
        // If we measured an overcurrent situation, we start another conversion
        // really soon.
        return sleep_and_call(&timer_, MSEC_TO_NSEC(1),
                              STATE(start_conversion));
        //return call_immediately(STATE(start_conversion));
      }
    } else {
      num_overcurrent_tests_ = 0;
    }
    if (os_get_time_monotonic() > next_report_) {
      LOG(INFO, "  adc value: %04" PRIx32, adc_value[0]);
      next_report_ = os_get_time_monotonic() + MSEC_TO_NSEC(250);
    }
    return call_immediately(STATE(start_timer));
  }

  Action shorted() {
    num_disable_tries_ = 0;
    LOG(INFO, "short detected");
    // TODO(balazs.racz): send event telling that we have a short.
    return call_immediately(STATE(start_timer));
  }

  Action retry_wait() {
    return sleep_and_call(&timer_, OVERCURRENT_RETRY_DELAY, STATE(retry_enable));
  }

  Action retry_enable() {
    enable_dcc();
    return call_immediately(STATE(start_timer));
  }

  StateFlowTimer timer_;
  long long period_;
  uint8_t num_disable_tries_;
  uint8_t num_overcurrent_tests_;
  long long next_report_;
};

class TivaTrackPowerOnOffBit : public nmranet::BitEventInterface {
 public:
  TivaTrackPowerOnOffBit(uint64_t event_on, uint64_t event_off)
      : BitEventInterface(event_on, event_off) {}

  virtual bool GetCurrentState() { return query_dcc(); }
  virtual void SetState(bool new_value) {
    if (new_value) {
      LOG(WARNING, "enable dcc");
      enable_dcc();
    } else {
      LOG(WARNING, "disable dcc");
      disable_dcc();
    }
  }

  virtual nmranet::Node* node()
  {
    extern nmranet::DefaultNode g_node;
    return &g_node;
  }
};

#endif // _TIVA_SHORTDETECTION_HXX_
