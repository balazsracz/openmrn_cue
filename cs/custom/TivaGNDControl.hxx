/** \copyright
 * Copyright (c) 2015, Balazs Racz
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
 * \file TivaDAC.hxx
 *
 * Device driver for TivaWare to generate a specifiv output voltage using timer
 * PWM, assuming an external RC circuit is used to smooth the signal.
 *
 * @author Balazs Racz
 * @date 2 Aug 2015
 */

#ifndef _BRACZ_CUSTOM_TIVAGNDCONTROL_HXX_
#define _BRACZ_CUSTOM_TIVAGNDCONTROL_HXX_

#include "hardware.hxx"

class TivaGNDControl {
 public:
  TivaGNDControl() {
#ifdef HARDWARE_REVA
    GNDACTRL_NOFF_Pin::set(true);
    GNDACTRL_NON_Pin::set(true);
    GNDBCTRL_NOFF_Pin::set(true);
    // TODO GNDBCTRL_NON_Pin::set(true);
    SysCtlDelay(10);
    GNDBCTRL_NOFF_Pin::set(false);
    SysCtlDelay(10);
    GNDACTRL_NON_Pin::set(false);
#elif defined(HARDWARE_REVB)
    GNDACTRL_ON_Pin::hw_set_to_safe();
    GNDBCTRL_ON_Pin::hw_set_to_safe();
    GNDACTRL_NOFF_Pin::hw_set_to_safe();
    GNDBCTRL_NOFF_Pin::hw_set_to_safe();
#else
    #error not defined rev
#endif
  }

  static const int kDelayCrossover = 40;

  static void enable_railcom_cutout(bool railcom) {
    if (railcom) {
      // switch to A

      // safe mode
      GNDBCTRL_NON_Pin::set(true);
      GNDBCTRL_NOFF_Pin::set(true);
      GNDACTRL_NON_Pin::set(true);
      GNDACTRL_NOFF_Pin::set(true);
      SysCtlDelay(kDelaySafe);

      // turn off A drive
      GNDACTRL_NOFF_Pin::set(false);
      SysCtlDelay(kDelayCrossover);

      // turn on B drive
      GNDBCTRL_NON_Pin::set(false);
    } else {
      // switch to A

      // safe mode
      GNDBCTRL_NON_Pin::set(true);
      GNDBCTRL_NOFF_Pin::set(true);
      GNDACTRL_NON_Pin::set(true);
      GNDACTRL_NOFF_Pin::set(true);
      SysCtlDelay(kDelaySafe);

      // turn off B drive
      GNDBCTRL_NOFF_Pin::set(false);
      SysCtlDelay(kDelayCrossover);

      // turn on A drive
      GNDACTRL_NON_Pin::set(false);
    }
  }

  static constexpr unsigned kDelaySafe = 100;
  static constexpr unsigned kDelayOn = 100;

  static constexpr unsigned kDelayAOff = 26;
  static constexpr unsigned kDelayAOn = 1*26 + 22;

  static constexpr unsigned kDelayBOff = 34;
  static constexpr unsigned kDelayBOn = 2*26;

  static constexpr unsigned kDeadBand = 26;

  /// Computes the delay to do between the first and second phase. The
  /// constraints are: at least kDeadBand, and second_delay + off > on.
  static constexpr unsigned second_delay(unsigned off, unsigned on) {
    return std::min(kDeadBand, on > off ? on - off : 1);
  }

  static void disengage() {
    GNDBCTRL_NON_Pin::set(true);
    GNDACTRL_NON_Pin::set(true);
    SysCtlDelay(100);
    GNDACTRL_NOFF_Pin::set(false);
    GNDBCTRL_NOFF_Pin::set(false);
  }

  static void rr_test(bool railcom) {
    portENTER_CRITICAL();
    //constexpr unsigned kDelayOne = 4*26;
    //constexpr unsigned kDelayTwo = 0*26 + 1;

    // tuned for A

    Debug::GndControlSwitch1::set(railcom);
    if (railcom) {
          // switch to B
      GNDACTRL_NON_Pin::set(true);
      GNDBCTRL_NOFF_Pin::set(true);
      SysCtlDelay(kDelayAOff);
      Debug::GndControlSwitch2::set(true);
      GNDACTRL_NOFF_Pin::set(false);
      SysCtlDelay(second_delay(kDelayAOff, kDelayBOn));
      Debug::GndControlSwitch2::set(false);
      GNDBCTRL_NON_Pin::set(false);
    } else {
      // switch to A
      GNDBCTRL_NON_Pin::set(true);
      GNDACTRL_NOFF_Pin::set(true);
      SysCtlDelay(kDelayBOff);
      Debug::GndControlSwitch2::set(true);
      GNDBCTRL_NOFF_Pin::set(false);
      SysCtlDelay(second_delay(kDelayBOff, kDelayAOn));
      Debug::GndControlSwitch2::set(false);
      GNDACTRL_NON_Pin::set(false);
    }
    portEXIT_CRITICAL();
  }

};

class TivaBypassControl {
 public:
  static const int kDelayCrossover1 = 5;
  static const int kDelayCrossover2 = 5;

  TivaBypassControl() {
#ifdef HARDWARE_REVA    
    RCBYPASS_OFF_Pin::set(false);
    SysCtlDelay(kDelayCrossover1);
    RCBYPASS_NON_Pin::set(false);
#elif defined(HARDWARE_REVB)
    RCBYPASS_Pin::set(true);
#else
    #error not defined rev
#endif
  
  }

  static void set(bool bypass_on) {
#if defined(HARDWARE_REVA)
    if (bypass_on) {
      RCBYPASS_OFF_Pin::set(false);
      SysCtlDelay(kDelayCrossover1);
      RCBYPASS_NON_Pin::set(false);
    } else {
      RCBYPASS_NON_Pin::set(true);
      SysCtlDelay(kDelayCrossover2);
      RCBYPASS_OFF_Pin::set(true);
    }
#elif defined(HARDWARE_REVB)
    RCBYPASS_Pin::set(bypass_on);
#else
    #error not defined rev
#endif    
  }
};

#endif  // _BRACZ_CUSTOM_TIVAGNDCONTROL_HXX_
