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


struct SignalPacket {
  /// The data to send out to the bus. Starts with the address byte, then the
  /// length byte, then the data bytes.
  string payload_;

  /// If not null, this notifiable will be called when the packet send is
  /// complete.
  AutoNotify done_;

  enum {
    RESULT_PENDING = 0x10000,
    RESULT_ACK = 0,
    RESULT_NOACK = openlcb::Defs::OPENMRN_TIMEOUT,
  };

  static constexpr unsigned DEFAULT_TIMEOUT_MSEC = 2;
  
  /// How long to wait for a response from the target
  uint32_t responseTimeoutMsec_{DEFAULT_TIMEOUT_MSEC};
  
  /// What is the outcome of the packet sending.
  uint32_t resultCode_{RESULT_PENDING};

  /// If we received returned data, this is that data.
  string responsePayload_;
};

typedef StateFlow<Buffer<SignalPacket>, QList<1> > SignalPacketBaseInterface;

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

  /** Sets the port to transmit mode. */
  virtual void set_tx() = 0;

  /** Sets the port to receive mode and flushes any pending data. */
  virtual void set_rx() = 0;

  /** Precondition: port is in receive mode. Waits up to a certain amount for
   * an acknowledgement to come from the addressed device. The minimum timeout
   * is 2 msec to ensure that a full byte is received. */
  virtual Action wait_for_ack(unsigned timeout_msec, Callback c) = 0;

  /** Precondition: port is in receive mode. @return true if there was an ACK
   * on the bus since the last call of this function or since putting the port
   * in receive mode. Specifically, returns true if the bus is not idle level
   * right now. */
  virtual bool has_ack() = 0;
  
  /** Sends a byte to the UART. Returns true if send is successful, false if
   * buffer full. */
  virtual bool try_send_byte(uint8_t data) = 0;

  /** Waits for space in the tx buffer to be available, then transitions to
   * state c. */
  virtual Action wait_for_send(Callback c) = 0;

  
 private:
  Action entry() OVERRIDE {
    if (message()->data()->payload_.empty()) return release_and_exit();
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
    return reinterpret_cast<const uint8_t*>(message()->data()->payload_.data());
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
    if (offset_ >= message()->data()->payload_.size()) {
      return wait_for_send(STATE(start_ack));
    }
    if (try_send_byte(payload()[offset_])) {
      offset_++;
      return again();
    } else {
      return wait_for_send(STATE(send_data_byte));
    }
  }

  Action start_ack() {
    unsigned timeout_msec =
        std::max((unsigned)message()->data()->responseTimeoutMsec_,
                 (unsigned)SignalPacket::DEFAULT_TIMEOUT_MSEC);
    set_rx();
    return wait_for_ack(timeout_msec, STATE(eval_ack));
  }
  
 protected:
  Action eval_ack() {
    if (!has_ack()) {
      if (message()->data()->resultCode_ == SignalPacket::RESULT_PENDING) {
        message()->data()->resultCode_ = SignalPacket::RESULT_NOACK;
      }
      message()->data()->done_.reset();
      return release_and_exit();
    } else {
      message()->data()->resultCode_ = SignalPacket::RESULT_ACK;
      message()->data()->done_.reset();
      // Waits for idle bus.
      return wait_for_ack(SignalPacket::DEFAULT_TIMEOUT_MSEC, STATE(eval_ack));
    }
  }

 private:
  /** Next byte in the packet to transmit. */
  size_t offset_;
};

}  // namespace bracz_custom

#endif // _BRACZ_CUSTOM_SIGNALPACKET_HXX_
