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
 * @date 1 Aug 2015
 */

#ifndef _BRACZ_CUSTOM_TIVADAC_HXX_
#define _BRACZ_CUSTOM_TIVADAC_HXX_

#include "driverlib/rom_map.h"
#include "driverlib/timer.h"

template<class HW> class TivaDAC {
 public:
  TivaDAC() {
    MAP_SysCtlPeripheralEnable(HW::TIMER_PERIPH);
    MAP_TimerClockSourceSet(HW::TIMER_BASE, TIMER_CLOCK_SYSTEM);
    if (HW::config_timer) {
      MAP_TimerConfigure(HW::TIMER_BASE, HW::TIMER_CFG);
    }
    MAP_TimerControlStall(HW::TIMER_BASE, HW::TIMER, false);
    HW::TIMER_Pin::hw_init();
    HW::DIV_Pin::hw_init();
    HW::DIV_Pin::set(true);
  }

  static void set_pwm(uint32_t nominator, uint32_t denominator) {
    HASSERT(nominator < denominator);
    HASSERT(denominator < 0xffffU);
    MAP_TimerControlLevel(HW::TIMER_BASE, HW::TIMER, true);
    MAP_TimerLoadSet(HW::TIMER_BASE, HW::TIMER, denominator);
    MAP_TimerMatchSet(HW::TIMER_BASE, HW::TIMER, nominator);
    MAP_TimerEnable(HW::TIMER_BASE, HW::TIMER);
    HW::TIMER_Pin::set_hw();
  }

  static void set_div(bool do_div) {
    HW::DIV_Pin::set(do_div);
  }
};

#endif // _BRACZ_CUSTOM_TIVADAC_HXX_
