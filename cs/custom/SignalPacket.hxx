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
#include "openlcb/Defs.hxx"
#include "utils/Crc.hxx"
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

  /// Tracks whether CRC has already been added to payload_.
  bool hasCrc_{false};

  /// Tracks whether CRC should be skipped for this packet.
  bool skipCrc_{false};

  /// Returns true if CRC has been added to payload_.
  bool has_crc() const {
    return hasCrc_;
  }

  /// Instructs the packet flow to skip adding CRC to payload_.
  void skip_crc() {
    skipCrc_ = true;
  }

  /// Returns true if CRC addition should be skipped for this packet.
  bool is_crc_skipped() const {
    return skipCrc_;
  }

  /// Adds CRC to payload_ according to specification:
  /// 1. Increments length byte (payload_[1]) by 2.
  /// 2. Computes CRC-16-CCITT over payload_ (Address byte, updated Length byte,
  ///    Command byte, and Payload bytes).
  /// 3. Appends the 16-bit CRC (High Byte, Low Byte) to payload_.
  /// Ignores the call if CRC has already been added, if CRC is set to be skipped,
  /// or if payload is too short.
  void add_crc() {
    if (hasCrc_ || skipCrc_ || payload_.size() < 2) return;
    payload_[1] = static_cast<char>(static_cast<uint8_t>(payload_[1]) + 2);
    Crc16CCITT crc;
    for (size_t i = 0; i < payload_.size(); ++i) {
      crc.update(static_cast<uint8_t>(payload_[i]));
    }
    uint16_t crc_val = crc.get();
    payload_.push_back(static_cast<char>((crc_val >> 8) & 0xFF));
    payload_.push_back(static_cast<char>(crc_val & 0xFF));
    hasCrc_ = true;
  }
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

  /** Waits up to a certain amount for an acknowledgement to come from the
   * addressed device. The minimum timeout is 2 msec to ensure that a full byte
   * is received. */
  virtual Action wait_for_ack(unsigned timeout_msec, Callback c) = 0;

  /** Call this function exactly once at the beginning of the state that is the
   * callback to wait_for_ack. This function will do some mandatory
   * cleanup including switching to TX mode.
   * @return true if there was an ACK on the bus since the start of
   * wait_for_ack(). Specifically, returns true if the bus is not idle level
   * right now. */
  virtual bool postprocess_ack() = 0;
  
  /** Sends a byte to the UART. Returns true if send is successful, false if
   * buffer full. */
  virtual bool try_send_byte(uint8_t data) = 0;

  /** Waits for space in the tx buffer to be available, then transitions to
   * state c. */
  virtual Action wait_for_send(Callback c) = 0;

  
 private:
  Action entry() OVERRIDE {
    if (message()->data()->payload_.empty()) return release_and_exit();
    message()->data()->add_crc();
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
      return call_immediately(STATE(packet_end));
    }
    if (try_send_byte(payload()[offset_])) {
      offset_++;
      return again();
    } else {
      return wait_for_send(STATE(send_data_byte));
    }
  }

  Action packet_end() {
    //return wait_for_send(STATE(release_and_exit));
    return wait_for_send(STATE(start_ack));
  }
  
  Action start_ack() {
    unsigned timeout_msec =
        std::max((unsigned)message()->data()->responseTimeoutMsec_,
                 (unsigned)SignalPacket::DEFAULT_TIMEOUT_MSEC);
    return wait_for_ack(timeout_msec, STATE(eval_ack));
  }
  
 protected:
  Action eval_ack() {
    if (!postprocess_ack()) {
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
