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
 * \file MspM0SignalReceiver.hxx
 *
 * Class for receiving the UART signal packets in an MSPM0 device.
 *
 * @author Balazs Racz
 * @date 12 Jan 2025
 */

#ifndef _CUSTOM_MSPM0SIGNALRECEIVER_HXX_
#define _CUSTOM_MSPM0SIGNALRECEIVER_HXX_

#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#include "os/sleep.h"

#include "hardware.hxx"

class MspM0SignalReceiver {
 public:
  static constexpr unsigned MAX_PACKET_LEN = 32;
  
  void hw_init() {
    static const DL_UART_Main_ClockConfig gUART_0ClockConfig = {
        .clockSel = DL_UART_MAIN_CLOCK_BUSCLK,
        .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1};

    static const DL_UART_Main_Config gUART_0Config = {
        .mode = DL_UART_MAIN_MODE_NORMAL,
        .direction = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
        .parity = DL_UART_MAIN_PARITY_STICK_ZERO,
        .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS,
        .stopBits = DL_UART_MAIN_STOP_BITS_ONE};

    DL_UART_Main_setClockConfig(
        SIG_UART, (DL_UART_Main_ClockConfig *)&gUART_0ClockConfig);

    DL_UART_Main_init(SIG_UART, (DL_UART_Main_Config *)&gUART_0Config);

    DL_UART_Main_setOversampling(SIG_UART, DL_UART_OVERSAMPLING_RATE_16X);
    // 15625 baud. = 32000000 / (16 * 15625) = 128. No fractional part.
    DL_UART_Main_setBaudRateDivisor(SIG_UART, 128, 0);

    /* Configure FIFOs */
    DL_UART_Main_enableFIFOs(SIG_UART);
    DL_UART_Main_setRXFIFOThreshold(SIG_UART,
                                    DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_Main_setTXFIFOThreshold(SIG_UART,
                                    DL_UART_TX_FIFO_LEVEL_ONE_ENTRY);

    DL_UART_Main_enable(SIG_UART);
  }

  void loop() __attribute__((noinline,optimize("O0"))) {
    while (!DL_UART_isRXFIFOEmpty(SIG_UART)) {
      uint16_t d = SIG_UART->RXDATA;
      if (d & DL_UART_ERROR_PARITY) {
        // Address byte.
        size_ = 0;
        address_ = d & 0xff;
      } else if (size_ < MAX_PACKET_LEN) {
        packet_[size_++] = d & 0xff;
      }
    }
  }

  void ack() {
    microsleep(32);
    DL_UART_transmitData(SIG_UART, 0x55);
  }
  
  /// @return true if we have received a complete packet.
  bool is_full() {
    return size_ >= 1 && size_ >= packet_[0];
  }

  uint8_t address() {
    return address_;
  }

  uint8_t cmd() {
    return packet_[1];
  }

  /// @return the number of bytes in this packet, including the size and
  /// command bytes.
  uint8_t size() {
    return packet_[0];
  }
  
  void clear() {
    size_ = 0;
  }

  /// @return the raw data in the packet. Offset [0] is the size, offset [1] is
  /// the command, offset [2] is the first payload byte.
  uint8_t* data() {
    return packet_;
  }
  
  uint8_t address_; 
  uint8_t packet_[MAX_PACKET_LEN];
  uint8_t size_ = 0;
};  // class MspM0SignalReceiver

#endif  // _CUSTOM_MSPM0SIGNALRECEIVER_HXX_
