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

#include "custom/MspM0SignalReceiver.hxx"
#include "custom/Crc32.h"
#include "os/os.h"
#include "os/sleep.h"
#include "utils/blinker.h"

#include "src/base.h"
#include "address.h"
#include "utils/macros.h"

#include <ti/driverlib/m0p/dl_sysctl.h>



extern void set_pix(uint32_t color);

enum Color {
  COLOR_GREEN = 0x00ff00,
  COLOR_RED = 0xff0000,
};

MspM0SignalReceiver g_receiver;

/// Retrieves a halfword from the packet contents. Does not check boundary.
/// @param offset is which byte in the packet the halfword starts at. offset 0
/// is the length byte, offset 1 is the cmd, offset 2 is the first byte of the
/// payload.
/// @return the little-endian half-word at that offset.
uint16_t get_hword(unsigned offset) {
  return (g_receiver.data()[offset] | (g_receiver.data()[offset+1] << 8));
}

/// Converts a linker symbol into an uint32 address.
/// @param sym a linker symbol like __app_start
/// @return the 32-bit address in the memory space for this symbol.
inline uint16_t symbol_to_address(char& sym) {
  return (uint32_t)(&sym);
}

extern char __app_start;
extern char __app_end;

static constexpr unsigned SECTOR_SIZE = 1024;

void __attribute__((noinline)) ack() {
  microsleep(32);
  DL_UART_transmitData(SIG_UART, 0x55);
}

void __attribute__((noinline)) execute_flash_program() {
  uint16_t address = get_hword(2) << 1;
  uint16_t length = g_receiver.size() - 5;
  HASSERT(address >= symbol_to_address(__app_start));
  HASSERT(address + length <= symbol_to_address(__app_end));
  HASSERT(length == 8);
  HASSERT(address % 8 == 0);
  uint64_t data;
  memcpy(&data, g_receiver.data() + 4, 8);
  // Auto-erase
  if ((address & (SECTOR_SIZE - 1)) == 0) {
    DL_FlashCTL_unprotectSector(FLASHCTL, address,
                                DL_FLASHCTL_REGION_SELECT_MAIN);
    DL_FlashCTL_eraseMemory(FLASHCTL, address, DL_FLASHCTL_COMMAND_SIZE_SECTOR);
    auto ret = DL_FlashCTL_waitForCmdDone(FLASHCTL);
    HASSERT(ret);
  }
  // Actually program the data.
  auto ret = DL_FlashCTL_programMemoryBlocking64WithECCGenerated(
      FLASHCTL, address, (uint32_t *)&data, 2, DL_FLASHCTL_REGION_SELECT_MAIN);
  HASSERT(ret);
  ret = DL_FlashCTL_waitForCmdDone(FLASHCTL);
  HASSERT(ret);
  return;
}

void process_packet() {
  switch(g_receiver.cmd()) {
    case SCMD_RESET: {
      DL_SYSCTL_resetDevice(DL_SYSCTL_RESET_BOOT);
      return;
    }
    case SCMD_ASPECT: {
      // Starts main application.
      extern char __app_start;
      // Loads the application's initial stack pointer value to SP, loads the
      // reset vector, and branches to it.
      asm volatile(" mov   r3, %[flash_addr] \n"
                   :
                   : [flash_addr] "r"(&__app_start));
      asm volatile(" ldr r0, [r3]\n"
                   " mov sp, r0\n"
                   " ldr r0, [r3, #4]\n"
                   " bx  r0\n");
      return;
    }
    case SCMD_FLASH_SUM: {
      uint8_t sum = 0;
      for (unsigned i = 1; i < g_receiver.size(); ++i) {
        sum += g_receiver.data()[i];
      }
      if (sum == 0) {
        execute_flash_program();
        return ack();
      } else {
        return;
      }
    }
    case SCMD_CRC: {
      uint16_t address = get_hword(2) << 1;
      uint16_t len = get_hword(4);
      Crc32 crc;
      extern uint8_t __flash_start[];
      for (unsigned i = 0; i < len; ++i) {
        crc.Add(__flash_start[address + i]);
      }
      uint32_t expected = get_hword(6) | (get_hword(8) << 16);
      if (expected == crc.Get()) {
        set_pix(COLOR_GREEN);
        return ack();
      } else {
        set_pix(COLOR_RED);
      }
      return;
    }
    case SCMD_PING: {
      return ack();
    }
    case SCMD_LED: {
      set_pix((g_receiver.data()[2] << 16) | (g_receiver.data()[3] << 8) |
              (g_receiver.data()[4] << 0));
      return ack();
    }
  }

}


/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[]) {
  g_receiver.hw_init();
  setblink(0);
  set_pix(COLOR_RED);
  while (1) {
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
