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
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"

#include "executor/StateFlow.hxx"
#include "utils/logging.h"
#include "dcc_control.hxx"

static const int SEQUENCER = 3;
static const auto ADC_BASE = ADC0_BASE;
static const auto ADC_PERIPH = SYSCTL_PERIPH_ADC0;

static const auto ADCPIN_PERIPH = SYSCTL_PERIPH_GPIOK;
static const auto ADCPIN_BASE = GPIO_PORTK_BASE;
static const auto ADCPIN_NUM = GPIO_PIN_0;
static const auto ADCPIN_NUM_ALT = GPIO_PIN_2;
static const auto ADCPIN_CH = ADC_CTL_CH16;
static const auto ADC_INTERRUPT = INT_ADC0SS3;

static const auto SHUTDOWN_CURRENT_AMPS = 1.5;
// ADC value at which we turn off the output.
static const unsigned SHUTDOWN_LIMIT = (SHUTDOWN_CURRENT_AMPS * 0.2 / 3.3) * 0xfff;
// We try this many times to reenable after a short.
static const unsigned OVERCURRENT_RETRY = 3;
static const long long OVERCURRENT_RETRY_DELAY = MSEC_TO_NSEC(300);


class TivaShortDetectionModule;
static TivaShortDetectionModule* g_short_detector = nullptr;

class TivaShortDetectionModule : public StateFlowBase {
 public:
  TivaShortDetectionModule(Service* s, long long period)
      : StateFlowBase(s),
        timer_(this),
        period_(period),
        num_disable_tries_(0) {
    HASSERT(g_short_detector == nullptr);
    g_short_detector = this;

    start_flow(STATE(start_timer));

    SysCtlPeripheralEnable(ADC_PERIPH);
    SysCtlPeripheralEnable(ADCPIN_PERIPH);
    GPIOPinTypeADC(ADCPIN_BASE, ADCPIN_NUM);
    GPIOPinTypeADC(ADCPIN_BASE, ADCPIN_NUM_ALT);

    // Sets ADC to 16 MHz clock.
    //ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, 30);
    ADCHardwareOversampleConfigure(ADC_BASE, 64);

    ADCSequenceConfigure(ADC_BASE, SEQUENCER, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC_BASE, SEQUENCER, 0, ADCPIN_CH | ADC_CTL_IE |
                             ADC_CTL_SHOLD_256 | ADC_CTL_END);
    ADCSequenceEnable(ADC_BASE, SEQUENCER);

    ADCIntEnable(ADC_BASE, SEQUENCER);
    MAP_IntPrioritySet(ADC_INTERRUPT, configKERNEL_INTERRUPT_PRIORITY);
    MAP_IntEnable(ADC_INTERRUPT);
    next_report_ = 0;
  }

  ~TivaShortDetectionModule() {
    g_short_detector = nullptr;
  }

  void interrupt_handler() {
    ADCIntClear(ADC_BASE, SEQUENCER);
    notify_from_isr();
  }

 private:
  Action start_timer() {
    return sleep_and_call(&timer_, period_, STATE(start_conversion));
  }

  Action start_conversion() {
    ADCIntClear(ADC_BASE, SEQUENCER);
    ADCProcessorTrigger(ADC_BASE, SEQUENCER);
    return wait_and_call(STATE(conversion_done));
  }

  Action conversion_done() {
    uint32_t adc_value[1];
    adc_value[0] = 0;
    ADCSequenceDataGet(ADC_BASE, SEQUENCER, adc_value);
    if (os_get_time_monotonic() > next_report_) {
      LOG(INFO, "adc value: %04" PRIx32, adc_value[0]);
      next_report_ = os_get_time_monotonic() + MSEC_TO_NSEC(250);
    }
    if (adc_value[0] > SHUTDOWN_LIMIT) {
      disable_dcc();
      ++num_disable_tries_;
      if (num_disable_tries_ < OVERCURRENT_RETRY) {
        return call_immediately(STATE(retry_wait));
      } else {
        return call_immediately(STATE(shorted));
      }
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
  long long next_report_;
};

extern "C" {

void adc0_seq3_interrupt_handler(void) {
  // COMPILE_ASSERT(ADC_BASE == ADC0_BASE && SEQUENCER == 3)
  g_short_detector->interrupt_handler();
}

}


class TivaTrackPowerOnOffBit : public nmranet::BitEventInterface {
 public:
  TivaTrackPowerOnOffBit(uint64_t event_on, uint64_t event_off)
      : BitEventInterface(event_on, event_off) {}

  virtual bool GetCurrentState() { return query_dcc(); }
  virtual void SetState(bool new_value) {
    if (new_value) {
      enable_dcc();
    } else {
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
