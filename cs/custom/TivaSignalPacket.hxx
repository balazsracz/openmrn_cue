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
 * \file TivaSignalPacket.hxx
 *
 * Implements the hardware-specific part for the UART packet transmit interface
 * for Tiva.
 *
 * @author Balazs Racz
 * @date 19 Jul 2014
 */

#ifndef _BRACZ_CUSTOM_TIVASIGNALPACKET_HXX_
#define _BRACZ_CUSTOM_TIVASIGNALPACKET_HXX_

#ifdef ENABLE_TIVA_SIGNAL_DRIVER

#include "driverlib/interrupt.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "inc/hw_uart.h"

#include "custom/SignalPacket.hxx"
#include "utils/Singleton.hxx"

namespace bracz_custom {

class TivaSignalPacketSender : public SignalPacketBase,
                               public Singleton<TivaSignalPacketSender> {
 public:
  /** Example: Sender(&g_service, 15625, UART1_BASE,  */
  TivaSignalPacketSender(Service* s, unsigned baudrate, unsigned uart_base,
                         unsigned interrupt);

  /** Called from the UART interrupt handler. */
  void interrupt();

 protected:
  /** This function initiates flushing the TX buffer. The action returned
   * should delay execution until the buffer flush is complete. A typical
   * implementation will run an ioctl to add *this as a notifiable to the
   * fd's interrupt handler and just return wait_and_call(). */
  Action flush_buffer(Callback c) OVERRIDE;

  /** Returns true if the transmit buffer is empty, and it is safe to switch
   * the parity mode. */
  bool is_buffer_empty() OVERRIDE;

  /** Sets the 9th bit to 1 for the upcoming bytes to be transmitted. */
  void set_parity_on() OVERRIDE;

  /** Sets the 9th bit to 0 for the upcoming bytes to be transmitted. */
  void set_parity_off() OVERRIDE;

  /** Sends a byte to the UART. Returns true if send is successful, false if
   * buffer full. */
  bool try_send_byte(uint8_t data) OVERRIDE;

  /** Waits for space in the tx buffer to be available, then transitions to
   * state c. */
  Action wait_for_send(Callback c) OVERRIDE;

 private:
  unsigned uartBase_;
  unsigned interrupt_;
};

DEFINE_SINGLETON_INSTANCE(TivaSignalPacketSender);

TivaSignalPacketSender::TivaSignalPacketSender(Service* s, unsigned baudrate,
                                               unsigned uart_base,
                                               unsigned interrupt)
    : SignalPacketBase(s), uartBase_(uart_base), interrupt_(interrupt) {
  MAP_IntDisable(interrupt);
  MAP_UARTConfigSetExpClk(
      uartBase_, MAP_SysCtlClockGet(), baudrate,
      UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_ZERO);
  MAP_IntPrioritySet(interrupt_,
                     std::min(0xff, configKERNEL_INTERRUPT_PRIORITY + 0x20));
  MAP_UARTEnable(uartBase_);
  MAP_UARTFIFOEnable(uartBase_);
}

void TivaSignalPacketSender::interrupt() {
  MAP_UARTIntDisable(uartBase_, UART_INT_TX);
  MAP_IntDisable(interrupt_);
  unsigned long status = MAP_UARTIntStatus(uartBase_, true);
  MAP_UARTIntClear(uartBase_, status);
  service()->executor()->add_from_isr(this);
}

bool TivaSignalPacketSender::is_buffer_empty() {
  return !MAP_UARTBusy(uartBase_);
}

StateFlowBase::Action TivaSignalPacketSender::flush_buffer(Callback c) {
  MAP_UARTTxIntModeSet(uartBase_, UART_TXINT_MODE_EOT);
  MAP_UARTIntEnable(uartBase_, UART_INT_TX);
  MAP_IntEnable(interrupt_);
  return wait_and_call(c);
}

/** Sets the 9th bit to 1 for the upcoming bytes to be transmitted. */
void TivaSignalPacketSender::set_parity_on() {
  MAP_UARTParityModeSet(uartBase_, UART_CONFIG_PAR_ONE);
}

/** Sets the 9th bit to 0 for the upcoming bytes to be transmitted. */
void TivaSignalPacketSender::set_parity_off() {
  MAP_UARTParityModeSet(uartBase_, UART_CONFIG_PAR_ZERO);
}

/** Sends a byte to the UART. Returns true if send is successful, false if
 * buffer full. */
bool TivaSignalPacketSender::try_send_byte(uint8_t data) {
  return MAP_UARTCharPutNonBlocking(uartBase_, data);
}

/** Waits for space in the tx buffer to be available, then transitions to
 * state c. */
StateFlowBase::Action TivaSignalPacketSender::wait_for_send(Callback c) {
  MAP_UARTTxIntModeSet(uartBase_, UART_TXINT_MODE_FIFO);
  MAP_UARTFIFOLevelSet(uartBase_, UART_FIFO_TX2_8, UART_FIFO_RX7_8);
  MAP_UARTIntEnable(uartBase_, UART_INT_TX);
  MAP_IntEnable(interrupt_);
  return wait_and_call(c);
}

extern "C" {
void uart1_interrupt_handler(void) {
  TivaSignalPacketSender::instance()->interrupt();
}
}  // extern "C"

}  // namespace bracz_custom

#endif  // ENABLE_TIVA_SIGNAL_DRIVER
#endif  // _BRACZ_CUSTOM_TIVASIGNALPACKET_HXX_
