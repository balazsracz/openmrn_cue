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
 * \file SignalPacket.hxx
 *
 * Packet transmission flow for 9-bit UART-based packet stream. This
 * transmission flow accepts packets of Payload and formats them to the
 * hardware UART. It needs specialization for the specific UART driver.
 *
 * @author Balazs Racz
 * @date 19 Jul 2014
 */

#ifndef _BRACZ_CUSTOM_SIGNALPACKET_HXX_
#define _BRACZ_CUSTOM_SIGNALPACKET_HXX_

#include "executor/StateFlow.hxx"
#include "utils/Hub.hxx"

namespace bracz_custom {

typedef StateFlow<Buffer<string>, QList<1> > SignalPacketBaseInterface;

class SignalPacketBase : public SignalPacketBaseInterface {
 public:
  SignalPacketBase(Service* s)
      : SignalPacketBaseInterface(s) {}

 protected:
  /** This function initiates flushing the TX buffer. The action returned
   * should delay execution until the buffer flush is complete. A typical
   * implementation will run an ioctl to add *this as a notifiable to the
   * fd's interrupt handler and just return wait_and_call(). */
  virtual Action flush_buffer(Callback c) = 0;

  /** Returns true if the transmit buffer is empty, and it is safe to switch
   * the parity mode. */
  virtual bool is_buffer_empty() = 0;

  /** Sets the 9th bit to 1 for the upcoming bytes to be transmitted. */
  virtual void set_parity_on() = 0;

  /** Sets the 9th bit to 0 for the upcoming bytes to be transmitted. */
  virtual void set_parity_off() = 0;

  /** Sends a byte to the UART. Returns true if send is successful, false if
   * buffer full. */
  virtual bool try_send_byte(uint8_t data) = 0;

  /** Waits for space in the tx buffer to be available, then transitions to
   * state c. */
  virtual Action wait_for_send(Callback c) = 0;

 private:
  Action entry() OVERRIDE {
    if (message()->data()->empty()) return release_and_exit();
    return call_immediately(STATE(wait_for_tx_empty));
  }

  Action wait_for_tx_empty() {
    if (is_buffer_empty()) {
      return call_immediately(STATE(send_address_byte));
    } else {
      return flush_buffer(STATE(wait_for_tx_empty));
    }
  }

  const uint8_t* payload() {
    return reinterpret_cast<const uint8_t*>(message()->data()->data());
  }

  Action send_address_byte() {
    set_parity_on();
    HASSERT(try_send_byte(payload()[0]));
    return flush_buffer(STATE(wait_for_address_empty));
  }

  Action wait_for_address_empty() {
    if (is_buffer_empty()) {
      offset_ = 1;
      set_parity_off();
      return call_immediately(STATE(send_data_byte));
    } else {
      return flush_buffer(STATE(wait_for_address_empty));
    }
  }

  Action send_data_byte() {
    if (offset_ >= message()->data()->size()) {
      release();
      return wait_for_send(STATE(exit));
    }
    if (try_send_byte(payload()[offset_])) {
      offset_++;
      return again();
    } else {
      return wait_for_send(STATE(send_data_byte));
    }
  }

  /** Next byte in the packet to transmit. */
  size_t offset_;
};

}  // namespace bracz_custom

#endif // _BRACZ_CUSTOM_SIGNALPACKET_HXX_
