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
    GNDACTRL_NOFF_Pin::set(true);
    GNDACTRL_NON_Pin::set(true);
    GNDBCTRL_NOFF_Pin::set(true);
    GNDBCTRL_NON_Pin::set(true);
    SysCtlDelay(10);
    GNDBCTRL_NOFF_Pin::set(false);
    SysCtlDelay(10);
    GNDACTRL_NON_Pin::set(true);
  }

  static const int kDelayCrossover = 40;

  static void enable_railcom_cutout(bool railcom) {
    if (railcom) {
      // switch to B
      GNDACTRL_NON_Pin::set(true);
      GNDACTRL_NOFF_Pin::set(false);
      GNDBCTRL_NOFF_Pin::set(true);
      SysCtlDelay(kDelayCrossover);
      GNDBCTRL_NON_Pin::set(false);
    } else {
      // switch to A
      GNDBCTRL_NON_Pin::set(true);
      GNDBCTRL_NOFF_Pin::set(false);
      GNDACTRL_NOFF_Pin::set(true);
      SysCtlDelay(kDelayCrossover);
      GNDACTRL_NON_Pin::set(false);
    }
  }
};

class TivaBypassControl {
 public:
  static const int kDelayCrossover1 = 3;
  static const int kDelayCrossover2 = 3;

  TivaBypassControl() {
    RCBYPASS_OFF_Pin::set(false);
    SysCtlDelay(kDelayCrossover1);
    RCBYPASS_NON_Pin::set(false);
  }

  static void set(bool bypass_on) {
    if (bypass_on) {
      RCBYPASS_OFF_Pin::set(false);
      SysCtlDelay(kDelayCrossover1);
      RCBYPASS_NON_Pin::set(false);
    } else {
      RCBYPASS_NON_Pin::set(true);
      //SysCtlDelay(kDelayCrossover2);
      RCBYPASS_OFF_Pin::set(true);
    }
  }
};

#endif  // _BRACZ_CUSTOM_TIVAGNDCONTROL_HXX_
