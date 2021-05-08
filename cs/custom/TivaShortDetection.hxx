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
#include "openlcb/EventHandlerTemplates.hxx"
#include "openlcb/TractionDefs.hxx"
#include "utils/logging.h"
#include "commandstation/dcc_control.hxx"
#include "DccHardware.hxx"

static const auto SHUTDOWN_CURRENT_AMPS = 1.5;
// ADC value at which we turn off the output.
static const unsigned SHUTDOWN_LIMIT =
    (SHUTDOWN_CURRENT_AMPS * 0.2 / 3.3) * 0xfff;

static const auto KILL_CURRENT_AMPS = 4.0;
// ADC value at which we turn off the output.
static const unsigned KILL_LIMIT = (KILL_CURRENT_AMPS * 0.2 / 3.3) * 0xfff;

// We try this many times to reenable after a short.
static const unsigned OVERCURRENT_RETRY = 3;
static const long long OVERCURRENT_RETRY_DELAY = MSEC_TO_NSEC(300);

static const uint64_t EVENT_CS_DETECTED_SHORT = UINT64_C(0x0501010114FF0000) | 0x18;

template <class HW>
class TivaShortDetectionModule;

template <class HW>
class ADCFlowBase : public StateFlowBase {
 public:
  void interrupt_handler() {
    ADCIntClear(HW::ADC_BASE, HW::ADC_SEQUENCER);
    MAP_IntDisable(HW::ADC_INTERRUPT);
    notify_from_isr();
  }

 protected:
  ADCFlowBase(Service* s, long long period)
      : StateFlowBase(s), timer_(this), period_(period) {
    start_flow(STATE(start_timer));

    SysCtlPeripheralEnable(HW::ADC_PERIPH);
    SysCtlPeripheralEnable(HW::ADCPIN_PERIPH);

    GPIODirModeSet(HW::ADCPIN_BASE, HW::ADCPIN_PIN, GPIO_DIR_MODE_HW);
    GPIOPadConfigSet(HW::ADCPIN_BASE, HW::ADCPIN_PIN, GPIO_STRENGTH_2MA,
                     GPIO_PIN_TYPE_ANALOG);

    // Sets ADC to 16 MHz clock.
    // ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL,
    // 30);
    ADCHardwareOversampleConfigure(HW::ADC_BASE, 64);

    ADCSequenceConfigure(HW::ADC_BASE, HW::ADC_SEQUENCER, ADC_TRIGGER_PROCESSOR,
                         0);
    ADCSequenceStepConfigure(
        HW::ADC_BASE, HW::ADC_SEQUENCER, 0,
        HW::ADCPIN_CH | ADC_CTL_IE | ADC_CTL_SHOLD_256 | ADC_CTL_END);
    ADCSequenceEnable(HW::ADC_BASE, HW::ADC_SEQUENCER);

    ADCIntEnable(HW::ADC_BASE, HW::ADC_SEQUENCER);
    MAP_IntPrioritySet(HW::ADC_INTERRUPT, configKERNEL_INTERRUPT_PRIORITY);
  }

  Action start_timer() {
    return sleep_and_call(&timer_, period_, STATE(start_conversion));
  }

  Action start_after(long long period) {
    return sleep_and_call(&timer_, period, STATE(start_conversion));
  }

  StateFlowTimer timer_;

 private:
  Action start_conversion() {
    ADCIntClear(HW::ADC_BASE, HW::ADC_SEQUENCER);
    ADCProcessorTrigger(HW::ADC_BASE, HW::ADC_SEQUENCER);
    MAP_IntEnable(HW::ADC_INTERRUPT);
    return wait_and_call(STATE(conversion_done));
  }

  Action conversion_done() {
    uint32_t adc_value[1];
    adc_value[0] = 0;
    ADCSequenceDataGet(HW::ADC_BASE, HW::ADC_SEQUENCER, adc_value);
    return sample(adc_value[0]);
  }

  virtual Action sample(uint32_t adc_value) = 0;

  long long period_;
};

template <class HW>
class TivaShortDetectionModule : public ADCFlowBase<HW> {
 public:
  TivaShortDetectionModule(openlcb::Node* node)
      : ADCFlowBase<HW>(node->iface(), MSEC_TO_NSEC(1)),
        num_disable_tries_(0),
        num_overcurrent_tests_(0),
        node_(node) {
    next_report_ = 0;
  }

  ~TivaShortDetectionModule() {}

 private:
  typedef StateFlowBase::Action Action;
  using StateFlowBase::call_immediately;
  using StateFlowBase::sleep_and_call;

  /// @return the currently active output channel.
  DccOutput *current_output()
  {
    return get_dcc_output(DccOutput::TRACK);
  }
  
  Action sample(uint32_t adc_value) OVERRIDE {
    current_output()->clear_disable_output_for_reason(
            DccOutput::DisableReason::INITIALIZATION_PENDING);
    if (adc_value > KILL_LIMIT) {
      current_output()->disable_output_for_reason(
          DccOutput::DisableReason::SHORTED);
      LOG(INFO, "kill value: %04" PRIx32, adc_value);
      return call_immediately(STATE(shorted));
    }
    if (adc_value > SHUTDOWN_LIMIT) {
      if (++num_overcurrent_tests_ >= 5 || adc_value > KILL_LIMIT) {
        current_output()->disable_output_for_reason(
            DccOutput::DisableReason::SHORTED);
        LOG(INFO, "%s value: %04" PRIx32,
            adc_value <= KILL_LIMIT ? "disable" : "kill", adc_value);
        ++num_disable_tries_;
        if (num_disable_tries_ < OVERCURRENT_RETRY) {
          return call_immediately(STATE(retry_wait));
        } else {
          return call_immediately(STATE(shorted));
        }
      } else {
        LOG(INFO, "overcurrent value: %04" PRIx32, adc_value);
        // If we measured an overcurrent situation, we start another conversion
        // really soon.
        return this->start_after(MSEC_TO_NSEC(1));
        // return call_immediately(STATE(start_conversion));
      }
    } else {
      num_overcurrent_tests_ = 0;
    }
    if (os_get_time_monotonic() > next_report_) {
      LOG(INFO, "  adc value: %04" PRIx32, adc_value);
      next_report_ = os_get_time_monotonic() + MSEC_TO_NSEC(250);
    }
    return call_immediately(STATE(start_timer));
  }

  Action shorted() {
    num_disable_tries_ = 0;
    LOG(INFO, "short detected");
    return this->allocate_and_call(node_->iface()->global_message_write_flow(), STATE(send_short_message));
  }

  Action send_short_message() {
    auto* b = this->get_allocation_result(
        node_->iface()->global_message_write_flow());
    b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     openlcb::eventid_to_buffer(
                         openlcb::Defs::EMERGENCY_OFF_EVENT));
    node_->iface()->global_message_write_flow()->send(b);

    return this->allocate_and_call(node_->iface()->global_message_write_flow(), STATE(send_short_report2));
  }

  Action send_short_report2() {
    auto* b = this->get_allocation_result(
        node_->iface()->global_message_write_flow());
    b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     openlcb::eventid_to_buffer(EVENT_CS_DETECTED_SHORT));
    node_->iface()->global_message_write_flow()->send(b);
    
    return call_immediately(STATE(start_timer));
  }

  Action retry_wait() {
    return sleep_and_call(&this->timer_, OVERCURRENT_RETRY_DELAY,
                          STATE(retry_enable));
  }

  Action retry_enable() {
    current_output()->clear_disable_output_for_reason(
            DccOutput::DisableReason::SHORTED);
    return call_immediately(STATE(start_timer));
  }

  uint8_t num_disable_tries_;
  uint8_t num_overcurrent_tests_;
  long long next_report_;
  openlcb::Node* node_;
};

template <class HW>
class AccessoryOvercurrentMeasurement : public ADCFlowBase<HW> {
 public:
  AccessoryOvercurrentMeasurement(Service* s, openlcb::Node* node)
      : ADCFlowBase<HW>(s, MSEC_TO_NSEC(10)), num_short_(0), node_(node) {
    last_time_not_overcurrent_ = os_get_time_monotonic();
    HW::VOLTAGE_Pin::hw_init();
    is_voltage_ = 0;
    num_uv_ = 0;
  }

 private:
  typedef StateFlowBase::Action Action;
  using StateFlowBase::call_immediately;
  using StateFlowBase::allocate_and_call;
  using StateFlowBase::get_allocation_result;

  Action sample(uint32_t adc_value) OVERRIDE {
    if (is_voltage_) {
      set_current();
      return voltage_sample(adc_value);
    } else {
      set_voltage();
    }
    long long now = os_get_time_monotonic();
    if (adc_value >= HW::SHORT_LIMIT) {
      if (num_short_++ > HW::SHORT_COUNT) {
        HW::ACC_ENABLE_Pin::set(false);
        LOG(INFO, "acc short value: %04" PRIx32, adc_value);
        return call_immediately(STATE(shorted));
      } else {
        return this->start_after(MSEC_TO_NSEC(1));
      }
    } else {
      num_short_ = 0;
    }
    if (adc_value >= HW::OVERCURRENT_LIMIT) {
      if (now - last_time_not_overcurrent_ > HW::OVERCURRENT_TIME) {
        HW::ACC_ENABLE_Pin::set(false);
        LOG(INFO, "acc overcurrent value: %04" PRIx32, adc_value);
        return call_immediately(STATE(overcurrent));
      }
    } else {
      last_time_not_overcurrent_ = now;
    }
    if (count_report_++ > 50) {
      count_report_ = 0;
      LOG(INFO, "acc readout: %04" PRIx32, adc_value);
    }
    return this->start_timer();
  }

  Action voltage_sample(uint32_t adc_value) {
    adc_value *= 201;
    adc_value >>= 12;
    if (!count_report_) {
      LOG(INFO, "acc voltage: %" PRId32, adc_value);
    }
    if (!HW::ACC_ENABLE_Pin::get()) {
      num_uv_ = 0;
    }
    if (adc_value < 140) {
      if (num_uv_++ > HW::SHORT_COUNT) {
        return call_immediately(STATE(shorted));
      }
    } else {
      num_uv_ = 0;
    }
    return this->start_timer();
  }

  Action shorted() {
    return allocate_and_call(node_->iface()->global_message_write_flow(),
                             STATE(send_shorted));
  }

  Action send_shorted() {
    auto* b =
        get_allocation_result(node_->iface()->global_message_write_flow());
    b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     openlcb::eventid_to_buffer(HW::EVENT_SHORT));
    node_->iface()->global_message_write_flow()->send(b);
    return call_immediately(STATE(off));
  }

  Action overcurrent() {
    return allocate_and_call(node_->iface()->global_message_write_flow(),
                             STATE(send_overcurrent));
  }

  Action send_overcurrent() {
    auto* b =
        get_allocation_result(node_->iface()->global_message_write_flow());
    b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     openlcb::eventid_to_buffer(HW::EVENT_OVERCURRENT));
    node_->iface()->global_message_write_flow()->send(b);
    return call_immediately(STATE(off));
  }

  Action off() {
    return allocate_and_call(node_->iface()->global_message_write_flow(),
                             STATE(send_off));
  }

  Action send_off() {
    auto* b =
        get_allocation_result(node_->iface()->global_message_write_flow());
    b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node_->node_id(),
                     openlcb::eventid_to_buffer(HW::EVENT_OFF));
    node_->iface()->global_message_write_flow()->send(b);
    return this->start_timer();
  }

  // Switches the ADC sequencer to the voltage pin.
  void set_voltage() {
    is_voltage_ = 1;
    ADCSequenceDisable(HW::ADC_BASE, HW::ADC_SEQUENCER);
    ADCSequenceConfigure(HW::ADC_BASE, HW::ADC_SEQUENCER, ADC_TRIGGER_PROCESSOR,
                         0);
    ADCSequenceStepConfigure(
        HW::ADC_BASE, HW::ADC_SEQUENCER, 0,
        HW::VOLTAGEPIN_CH | ADC_CTL_IE | ADC_CTL_SHOLD_256 | ADC_CTL_END);
    ADCSequenceEnable(HW::ADC_BASE, HW::ADC_SEQUENCER);
  }

  // Switches the ADC sequencer to the current pin.
  void set_current() {
    is_voltage_ = 0;
    ADCSequenceDisable(HW::ADC_BASE, HW::ADC_SEQUENCER);
    ADCSequenceConfigure(HW::ADC_BASE, HW::ADC_SEQUENCER, ADC_TRIGGER_PROCESSOR,
                         0);
    ADCSequenceStepConfigure(
        HW::ADC_BASE, HW::ADC_SEQUENCER, 0,
        HW::ADCPIN_CH | ADC_CTL_IE | ADC_CTL_SHOLD_256 | ADC_CTL_END);
    ADCSequenceEnable(HW::ADC_BASE, HW::ADC_SEQUENCER);
  }

  long long last_time_not_overcurrent_;
  unsigned num_short_ : 8;
  unsigned num_uv_ : 8;
  unsigned is_voltage_ : 1;
  unsigned count_report_;
  openlcb::Node* node_;
};

template <class HW>
class TivaAccPowerOnOffBit : public openlcb::BitEventInterface {
 public:
  TivaAccPowerOnOffBit(openlcb::Node* node, uint64_t event_on,
                       uint64_t event_off)
      : BitEventInterface(event_on, event_off), node_(node) {}

  openlcb::EventState get_current_state() OVERRIDE {
    return HW::ACC_ENABLE_Pin::get() ? openlcb::EventState::VALID
                                     : openlcb::EventState::INVALID;
  }
  void set_state(bool new_value) OVERRIDE {
    if (!HW::ACC_ENABLE_Pin::get() && new_value) {
      // We are turning power on.
      // sends some event reports to clear off the shorted and overcurrent bits.
      auto* b = node()->iface()->global_message_write_flow()->alloc();
      b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node()->node_id(),
                       openlcb::eventid_to_buffer(HW::EVENT_OVERCURRENT ^ 1));
      node()->iface()->global_message_write_flow()->send(b);
      b = node()->iface()->global_message_write_flow()->alloc();
      b->data()->reset(openlcb::Defs::MTI_EVENT_REPORT, node()->node_id(),
                       openlcb::eventid_to_buffer(HW::EVENT_SHORT ^ 1));
      node()->iface()->global_message_write_flow()->send(b);
    }
    HW::ACC_ENABLE_Pin::set(new_value);
  }

  openlcb::Node* node() OVERRIDE { return node_; }

 private:
  openlcb::Node* node_;
};

#endif  // _TIVA_SHORTDETECTION_HXX_
