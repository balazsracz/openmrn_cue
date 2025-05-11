/** \copyright
 * Copyright (c) 2025, Balazs Racz
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
 * Application for the turnout button+LED board.
 *
 * @author Balazs Racz
 * @date 12 Jan 2025
 */

#include "address.h"
#include "custom/MspM0SignalReceiver.hxx"
#include "os/os.h"
#include "src/base.h"
#include "utils/blinker.h"
#include "utils/Debouncer.hxx"

MspM0SignalReceiver g_receiver;

/// Current state of the button. True = pressed, false = not pressed.
bool button_state = false;

extern void set_pix(uint32_t color);

enum Color {
  COLOR_BLACK = 0x000000,
  COLOR_RED = 0xff0000,
  COLOR_GREEN = 0x00ff00,
  COLOR_BLUE = 0x0000ff,
  COLOR_CYAN = 0x00ffff,
  COLOR_MAGENTA = 0xff00ff,
  COLOR_YELLOW = 0xffff00,
  COLOR_BROWN = 0xff8000,
};

static const uint32_t aspect_to_color[] = {
    COLOR_BLACK, COLOR_RED,     COLOR_GREEN,  COLOR_BLUE,
    COLOR_CYAN,  COLOR_MAGENTA, COLOR_YELLOW, COLOR_BROWN,
};

void process_packet() {
  switch (g_receiver.cmd()) {
    case SCMD_RESET: {
      DL_SYSCTL_resetDevice(DL_SYSCTL_RESET_BOOT);
      return;
    }
    case SCMD_ASPECT: {
      if (g_receiver.size() < 3) {
        return;
      }
      uint8_t aspect = g_receiver.data()[2];
      if (aspect >= ARRAYSIZE(aspect_to_color)) {
        return;
      }
      set_pix(aspect_to_color[aspect]);
      return g_receiver.ack();
    }
    case SCMD_PING: {
      return g_receiver.ack();
    }
    case SCMD_INZERO: {
      if (g_receiver.size() >= 3 && g_receiver.data()[2] == 0 &&
          !button_state) {
        return g_receiver.ack();
      }
      break;
    }
    case SCMD_INONE: {
      if (g_receiver.size() >= 3 && g_receiver.data()[2] == 0 &&
          button_state) {
        return g_receiver.ack();
      }
      break;
    }
  }
}

extern uint64_t g_time_msec;
QuiesceDebouncer btn_debouncer{10};

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[]) {
  g_receiver.hw_init();
  setblink(0);
  uint64_t last_time = 0;
  btn_debouncer.initialize(true); // not pressed
  while (1) {
    if (last_time != g_time_msec) {
      // Runs once per msec.
      last_time = g_time_msec;
      btn_debouncer.update_state(BUTTON_Pin::get());
      button_state = !btn_debouncer.current_state();
    }
    g_receiver.loop();
    if (g_receiver.is_full()) {
      if (g_receiver.address() == NODEID_LOW_BITS ||
          g_receiver.address() == 0) {
        process_packet();
      }
      g_receiver.clear();
    }
  }
  return 0;
}
