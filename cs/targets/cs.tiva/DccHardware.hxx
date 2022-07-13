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
 * \file DccHardware.hxx
 *
 * Pin declarations for the DCC hardware.
 *
 * @author Balazs Racz
 * @date 8 November 2014
 */


#ifndef _DCC_HARDWARE_HXX_
#define _DCC_HARDWARE_HXX_

#include "driverlib/adc.h"
#include "driverlib/timer.h"
#include "TivaGPIO.hxx"
#include "DummyGPIO.hxx"
#include "hardware.hxx"
#include "src/cs_config.h"
#include "dcc/DccOutput.hxx"

struct RailcomDefs
{
    static const uint32_t CHANNEL_COUNT = 1;
    static const uint32_t UART_PERIPH[CHANNEL_COUNT];
    static const uint32_t UART_BASE[CHANNEL_COUNT];
    // Make sure there are enough entries here for all the channels times a few
    // DCC packets.
    static const uint32_t Q_SIZE = 6;

    static const auto OS_INTERRUPT = INT_UART6;

    GPIO_HWPIN(CH1, GpioHwPin, P, 0, U6RX, UART);

    static void hw_init() {
         CH1_Pin::hw_init();
    }

    static void set_input() {
        CH1_Pin::set_input();
    }

    static void set_hw() {
        CH1_Pin::set_hw();
    }

    static void enable_measurement(bool) {}
    static void disable_measurement() {}

    static bool need_ch1_cutout() { return false; }
    static void middle_cutout_hook() {}

    static uint8_t get_feedback_channel() {
        return 0xff;
    }

    /** @returns a bitmask telling which pins are active. Bit 0 will be set if
     * channel 0 is active (drawing current).*/
    static uint8_t sample() {
        uint8_t ret = 0;
        if (!CH1_Pin::get()) ret |= 1;
        return ret;
    }

    static unsigned get_timer_tick()
    {
        return MAP_TimerValueGet(TIMER5_BASE, TIMER_A);
    }
};

const uint32_t RailcomDefs::UART_BASE[] __attribute__((weak)) = {UART6_BASE};
const uint32_t RailcomDefs::UART_PERIPH[] __attribute__((weak)) = {SYSCTL_PERIPH_UART6};

struct DccHwDefs {
  /// base address of a capture compare pwm timer pair
  static const unsigned long CCP_BASE = TIMER1_BASE;
  /// an otherwise unused interrupt number (could be that of the capture compare pwm timer)
  static const unsigned long OS_INTERRUPT = INT_TIMER1A;
  /// base address of an interval timer
  static const unsigned long INTERVAL_BASE = TIMER0_BASE;
  /// interrupt number of the interval timer
  static const unsigned long INTERVAL_INTERRUPT = INT_TIMER0A;

  static void set_booster_enabled() {
    PIN_H::set_hw();
    PIN_L::set_hw();
  }

  static void set_booster_disabled() {
    PIN_H::set(PIN_H_INVERT);
    PIN_H::set_output();
    PIN_H::set(PIN_H_INVERT);

    PIN_L::set(PIN_L_INVERT);
    PIN_L::set_output();
    PIN_L::set(PIN_L_INVERT);

    RAILCOM_TRIGGER_Pin::set(RAILCOM_TRIGGER_INVERT);
  }
  
  /** These timer blocks will be synchronized once per packet, when the
   *  deadband delay is set up. */
  static const auto TIMER_SYNC = TIMER_0A_SYNC | TIMER_0B_SYNC | TIMER_1A_SYNC | TIMER_1B_SYNC;
  
  // Peripherals to enable at boot.
  static const auto CCP_PERIPH = SYSCTL_PERIPH_TIMER1;
  static const auto INTERVAL_PERIPH = SYSCTL_PERIPH_TIMER0;
  using PIN_H = ::BOOSTER_H_Pin;
  using PIN_L = ::BOOSTER_L_Pin;
  /** Defines whether the drive-low pin is inverted or not. A non-inverted pin
   *  (value==false) will be driven high during the second half of the DCC bit
   *  (minus L_DEADBAND_DELAY_NSEC), and low during the first half.  A
   *  non-inverted pin will be driven low as safe setting at startup. */
  static const bool PIN_L_INVERT = true;

  /** Defines whether the high driver pin is inverted or not. A non-inverted
   *  (value==false) pin will be driven high during the first half of the DCC
   *  bit (minus H_DEADBAND_DELAY_NSEC at the end), and low during the second
   *  half.  A non-inverted pin will be driven low as safe setting at
   *  startup. */
  static const bool PIN_H_INVERT = true;

  /** @returns the number of preamble bits to send (in addition to the end of
   *  packet '1' bit) */
  static int dcc_preamble_count() { return 16; }

  /** @return whether we need the half-zero after the railcom cutout. */
  static bool generate_railcom_halfzero() { return true; }

  static void flip_led() {
    io::TrackPktLed::toggle();
  }

  /** the time (in nanoseconds) to wait between turning off the low driver and
   * turning on the high driver. */
  static const int H_DEADBAND_DELAY_NSEC = 2000;
  /** the time (in nanoseconds) to wait between turning off the high driver and
   * turning on the low driver. */
  static const int L_DEADBAND_DELAY_NSEC = 2000;

  /** @returns true to produce the RailCom cutout, else false */
  static bool railcom_cutout() { return true; }

  /** number of outgoing messages we can queue */
  static const size_t Q_SIZE = 4;

  // Pins defined for the short detection module.
  static const auto ADC_BASE = ADC0_BASE;
  static const auto ADC_PERIPH = SYSCTL_PERIPH_ADC0;
  static const int ADC_SEQUENCER = 3;
  static const auto ADC_INTERRUPT = INT_ADC0SS3;

#ifdef HW_V1
  DECL_PIN(ADCPIN, K, 3);
  static const auto ADCPIN_CH = ADC_CTL_CH19;
#elif defined(HW_V2)
  DECL_PIN(ADCPIN, K, 2);
  static const auto ADCPIN_CH = ADC_CTL_CH18;
#else
#error define version with HWVER=HW_V1 or HWVER=HW_V2
#endif

  // Pins defined for railcom
  using RAILCOM_TRIGGER_Pin = ::RAILCOM_TRIGGER_Pin;
  static const auto RAILCOM_TRIGGER_INVERT = true;
  static const auto RAILCOM_TRIGGER_DELAY_USEC = 6;
  
  static const auto RAILCOM_UART_BASE = UART6_BASE;
  static const auto RAILCOM_UART_PERIPH = SYSCTL_PERIPH_UART6;
  using RAILCOM_UARTPIN = ::RAILCOM_CH1_Pin;

  // Constants tuning railcom cutout window
  static const int RAILCOM_CUTOUT_START_DELTA_USEC = 0;
  static const int RAILCOM_CUTOUT_MID_DELTA_USEC = 0;
  static const int RAILCOM_CUTOUT_END_DELTA_USEC = 0;
  static const int RAILCOM_CUTOUT_POST_DELTA_USEC = 0;

  static_assert(RAILCOM_TRIGGER_INVERT == true,
                "fix railcom enable pin inversion");
  using RAILCOM_ENABLE_Pin = InvertedGpio<RAILCOM_TRIGGER_Pin>;

  struct BOOSTER_ENABLE_Pin {
   public:
    static void hw_init() {
    }
    static void set(bool on) {
      if (on) {
        set_booster_enabled();
      } else {
        set_booster_disabled();
      }
    }
  };
  
  using Output1 = DccOutputHwReal<1, BOOSTER_ENABLE_Pin, RAILCOM_ENABLE_Pin, 1,
                                  RAILCOM_TRIGGER_DELAY_USEC, 0>;
  using Output2 = DccOutputHwDummy<2>;
  using Output3 = DccOutputHwDummy<3>;
  using Output = Output1;
};


struct AccHwDefs {
  GPIO_PIN(ACC_ENABLE, GpioOutputSafeLow, B, 5);
  GPIO_PIN(VOLTAGE, GpioADCPin, K, 3);
  static const auto VOLTAGEPIN_CH = ADC_CTL_CH19;

  DECL_PIN(ADCPIN, B, 4);
  static const auto ADCPIN_CH = ADC_CTL_CH10;

  // Data defined for the short detection module.
  static const auto ADC_BASE = ADC0_BASE;
  static const auto ADC_PERIPH = SYSCTL_PERIPH_ADC0;
  static const int ADC_SEQUENCER = 2;
  static const auto ADC_INTERRUPT = INT_ADC0SS2;

  static const unsigned OVERCURRENT_LIMIT = 0x3E0;  // 0.33 Amps
  static const unsigned SHORT_LIMIT = 0xC00;  // ~2..2.3 amps

  static const long long OVERCURRENT_TIME = SEC_TO_NSEC(10);
  static const unsigned SHORT_COUNT = 10;

  static const uint64_t EVENT_SHORT = BRACZ_LAYOUT | 0x0006;
  static const uint64_t EVENT_OVERCURRENT = BRACZ_LAYOUT | 0x0008;
  static const uint64_t EVENT_OFF = BRACZ_LAYOUT | 0x0005;
};


#endif // _DCC_HARDWARE_HXX_
