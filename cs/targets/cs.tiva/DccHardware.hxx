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

struct DccHwDefs {
  /// base address of a capture compare pwm timer pair
  static const unsigned long CCP_BASE = TIMER1_BASE;
  /// an otherwise unused interrupt number (could be that of the capture compare pwm timer)
  static const unsigned long OS_INTERRUPT = INT_TIMER1A;
  /// base address of an interval timer
  static const unsigned long INTERVAL_BASE = TIMER0_BASE;
  /// interrupt number of the interval timer
  static const unsigned long INTERVAL_INTERRUPT = INT_TIMER0A;

  /** These timer blocks will be synchronized once per packet, when the
   *  deadband delay is set up. */
  static const auto TIMER_SYNC = TIMER_0A_SYNC | TIMER_0B_SYNC | TIMER_1A_SYNC | TIMER_1B_SYNC;
  
  // Peripherals to enable at boot.
  static const auto CCP_PERIPH = SYSCTL_PERIPH_TIMER1;
  static const auto INTERVAL_PERIPH = SYSCTL_PERIPH_TIMER0;
  static const auto PIN_H_GPIO_PERIPH = GPIO_PORTA_BASE;
  static const auto PIN_H_GPIO_CONFIG = GPIO_PA2_T1CCP0;
  static const auto PIN_H_GPIO_BASE = GPIO_PORTA_BASE;
  static const auto PIN_H_GPIO_PIN = GPIO_PIN_2;
#ifdef BPACK_1_LOW
  // set the output pin to be on boosterpack 1
  static const auto PIN_L_GPIO_PERIPH = GPIO_PORTD_BASE;
  static const auto PIN_L_GPIO_CONFIG = GPIO_PD3_T1CCP1;
  static const auto PIN_L_GPIO_BASE = GPIO_PORTD_BASE;
  static const auto PIN_L_GPIO_PIN = GPIO_PIN_3;
  /** Defines whether the drive-low pin is inverted or not. A non-inverted pin
   *  (value==false) will be driven high during the second half of the DCC bit
   *  (minus L_DEADBAND_DELAY_NSEC), and low during the first half.  A
   *  non-inverted pin will be driven low as safe setting at startup. */
  static const bool PIN_L_INVERT = true;
#else
  // original output pin
  static const auto PIN_L_GPIO_PERIPH = GPIO_PORTA_BASE;
  static const auto PIN_L_GPIO_CONFIG = GPIO_PA3_T1CCP1;
  static const auto PIN_L_GPIO_BASE = GPIO_PORTA_BASE;
  static const auto PIN_L_GPIO_PIN = GPIO_PIN_3;
  /** Defines whether the drive-low pin is inverted or not. A non-inverted pin
   *  (value==false) will be driven high during the second half of the DCC bit
   *  (minus L_DEADBAND_DELAY_NSEC), and low during the first half.  A
   *  non-inverted pin will be driven low as safe setting at startup. */
  static const bool PIN_L_INVERT = true;
#endif

  /** Defines whether the high driver pin is inverted or not. A non-inverted
   *  (value==false) pin will be driven high during the first half of the DCC
   *  bit (minus H_DEADBAND_DELAY_NSEC at the end), and low during the second
   *  half.  A non-inverted pin will be driven low as safe setting at
   *  startup. */
  static const bool PIN_H_INVERT = true;

  /** @returns the number of preamble bits to send (in addition to the end of
   *  packet '1' bit) */
  static int dcc_preamble_count() { return 16; }

  static constexpr uint8_t* LED_PTR =
      (uint8_t*)(GPIO_PORTQ_BASE + (GPIO_PIN_0 << 2));
  static void flip_led() {
    static uint8_t flip = 0xff;
    flip = ~flip;
    *LED_PTR = flip;
  }

  /** the time (in nanoseconds) to wait between turning off the low driver and
   * turning on the high driver. */
  static const int H_DEADBAND_DELAY_NSEC = 6000;
  /** the time (in nanoseconds) to wait between turning off the high driver and
   * turning on the low driver. */
  static const int L_DEADBAND_DELAY_NSEC = 6000;

  /** @returns true to produce the RailCom cutout, else false */
  static bool railcom_cutout() { return true; }

  /** number of outgoing messages we can queue */
  static const size_t Q_SIZE = 4;

  // Pins defined for the short detection module.
  static const auto ADC_BASE = ADC0_BASE;
  static const auto ADC_PERIPH = SYSCTL_PERIPH_ADC0;
  static const int ADC_SEQUENCER = 3;
  static const auto ADC_INTERRUPT = INT_ADC0SS3;

  DECL_PIN(ADCPIN, K, 3);
  static const auto ADCPIN_CH = ADC_CTL_CH19;

  // Pins defined for railcom
  //DECL_PIN(RAILCOM_TRIGGER, B, 4);
  DECL_PIN(RAILCOM_TRIGGER, H, 1);
  static const auto RAILCOM_TRIGGER_INVERT = true;

  static const auto RAILCOM_UART_BASE = UART6_BASE;
  static const auto RAILCOM_UART_PERIPH = SYSCTL_PERIPH_UART6;
  DECL_PIN(RAILCOM_UARTPIN, P, 0);
  static const auto RAILCOM_UARTPIN_CONFIG = GPIO_PP0_U6RX;
};


#endif // _DCC_HARDWARE_HXX_
