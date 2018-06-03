/** \copyright
 * Copyright (c) 2018, Balazs Racz
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are strictly prohibited without written consent.
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
 * \file ProgrammingTrackFrontend.hxx
 *
 * Flow to access the programming track.
 *
 * @author Balazs Racz
 * @date 1 June 2018
 */

#ifndef _COMMANDSTATON_PROGRAMMINGTRACKFRONTEND_HXX_
#define _COMMANDSTATON_PROGRAMMINGTRACKFRONTEND_HXX_

#include "executor/CallableFlow.hxx"
#include "dcc/ProgrammingTrackBackend.hxx"

#ifdef LOGLEVEL
#undef LOGLEVEL
#endif
#define LOGLEVEL INFO

namespace commandstation {

struct ProgrammingTrackFrontendRequest : public CallableFlowRequestBase {
  enum DirectWriteByte { DIRECT_WRITE_BYTE };
  enum DirectWriteBit { DIRECT_WRITE_BIT };
  enum DirectReadByte { DIRECT_READ_BYTE };
  enum DirectReadBit { DIRECT_READ_BIT };

  /// Request to write a byte sized CV in direct mode.
  /// @param cv_number is the 1-based CV number (as the user sees it).
  /// @param value is the value to set the CV to.
  void reset(DirectWriteByte, unsigned cv_number, uint8_t value) {
    reset_base();
    cmd_ = Type::DIRECT_WRITE_BYTE;
    cvOffset_ = cv_number - 1;
    value_ = value;
  }

  /// Request to read a byte sized CV in direct mode.
  /// @param cv_number is the 1-based CV number (as the user sees it).
  void reset(DirectReadByte, unsigned cv_number) {
    reset_base();
    cmd_ = Type::DIRECT_READ_BYTE;
    cvOffset_ = cv_number - 1;
    value_ = 0;
  }

  /// Request to write a single bit in direct mode.
  /// @param cv_number is the 1-based CV number (as the user sees it).
  /// @param bit is 0..7 for the bit to set
  /// @param value what to set the bit to
  void reset(DirectWriteBit, unsigned cv_number, uint8_t bit, bool value) {
    reset_base();
    cmd_ = Type::DIRECT_WRITE_BIT;
    cvOffset_ = cv_number - 1;
    bitOffset_ = bit;
    value_ = value ? 1 : 0;
  }

  /// Calues for the cmd_ argument.
  enum class Type {
    DIRECT_WRITE_BYTE,
    DIRECT_WRITE_BIT,
    DIRECT_READ_BYTE,
    DIRECT_READ_BIT
  };

  /// What is the instruction to do.
  Type cmd_;
  /// 0-based CV number to read or write
  unsigned cvOffset_;
  /// input or output argument containing the byte or bit value read or
  /// written.
  uint8_t value_;
  /// Which bit number to program (0..7).
  uint8_t bitOffset_;
};

class ProgrammingTrackFrontend : public CallableFlow<ProgrammingTrackFrontendRequest> {
 public:
  ProgrammingTrackFrontend(ProgrammingTrackBackend* backend)
      : CallableFlow<ProgrammingTrackFrontendRequest>(backend->service())
      , backend_(backend) {}

  typedef ProgrammingTrackFrontendRequest::Type RequestType;

  enum ResultCodes {
    /// Success
    ERROR_CODE_OK = openlcb::Defs::ERROR_CODE_OK,
    /// Error code when the locomotive is not responding to programming track
    /// requests. Usually dirty track or so.
    ERROR_NO_LOCO = openlcb::Defs::ERROR_OPENLCB_TIMEOUT | 1,
    /// Re-triable error generated when the loco respons with conflicting
    /// information.
    ERROR_FAILED_VERIFY = openlcb::Defs::ERROR_OPENLCB_TIMEOUT | 2,
    /// Error code when something was invoked that is not implemented.
    ERROR_UNIMPLEMENTED_CMD = openlcb::Defs::ERROR_UNIMPLEMENTED_CMD,
    /// cleared when done is called.
    OPERATION_PENDING = openlcb::DatagramClient::OPERATION_PENDING,
  };

  /// @param cnt sets how many times we should repeat a verify packet when
  /// reading bytes in direct mode using ACK method.
  void set_repeat_verify(unsigned cnt) {
    verifyRepeats_ = cnt;
  }

  /// @param cnt sets how many reset packets we should append a bit or byte
  /// verify packet when reading in direct mode.
  void set_repeat_verify_cooldown(unsigned cnt) {
    verifyCooldownReset_ = cnt;
  }

  Action entry() override {
    request()->resultCode = OPERATION_PENDING;
    switch (request()->cmd_) {
      case RequestType::DIRECT_WRITE_BYTE:
      case RequestType::DIRECT_WRITE_BIT:
      case RequestType::DIRECT_READ_BYTE:
      case RequestType::DIRECT_READ_BIT:
        return call_immediately(STATE(enter_service_mode));
      default:
        return return_with_error(ERROR_UNIMPLEMENTED_CMD);
    }
  }

  Action enter_service_mode() {
    return invoke_subflow_and_wait(backend_, STATE(send_initial_resets),
                                   ProgrammingTrackRequest::ENTER_SERVICE_MODE);
  }

  Action send_initial_resets() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    return invoke_subflow_and_wait(backend_, STATE(enter_service_mode_done),
                                   ProgrammingTrackRequest::SEND_RESET, 15);
  }

  Action enter_service_mode_done() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    switch (request()->cmd_) {
      case RequestType::DIRECT_WRITE_BYTE:
        serviceModePacket_.set_dcc_svc_write_byte(request()->cvOffset_,
                                                  request()->value_);
        break;
      case RequestType::DIRECT_WRITE_BIT:
        serviceModePacket_.set_dcc_svc_write_bit(request()->cvOffset_,
                                                 request()->bitOffset_,
                                                 request()->value_ != 0);
        break;
      case RequestType::DIRECT_READ_BYTE:
        return call_immediately(STATE(direct_read_byte));
      default:
        return exit_unimplemented();
    }
    foundAck_ = 0;
    hasWriteAck_ = 0;
    return invoke_subflow_and_wait(
        backend_, STATE(write_pkt_sent),
        ProgrammingTrackRequest::SEND_PROGRAMMING_PACKET, serviceModePacket_,
        DEFAULT_WRITE_REPEATS);
  }

  /// Turns off service mode and returns an unimplemented error.
  Action exit_unimplemented() {
    // Bail out as we have not written this code yet.
    request()->resultCode |= ERROR_UNIMPLEMENTED_CMD;
    return invoke_subflow_and_wait(
        backend_, STATE(return_response),
        ProgrammingTrackRequest::EXIT_SERVICE_MODE);
  }
  
  Action write_pkt_sent() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    if (b->data()->hasAck_) {
      foundAck_ = 1;
    }
    // Cooldown for the decoder.
    //
    // @todo: we need to instead create some form of timer for when to exit
    // service mode. Consecutive operations should not exit and enter service
    // mode separately.
    return invoke_subflow_and_wait(backend_, STATE(write_cooldown_done),
                                   ProgrammingTrackRequest::SEND_RESET, 15);
  }

  Action write_cooldown_done() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    if (b->data()->hasAck_) {
      foundAck_ = 1;
    }
    hasWriteAck_ = foundAck_;
    // Now we do write verify
    switch (request()->cmd_) {
      case RequestType::DIRECT_WRITE_BYTE:
        serviceModePacket_.set_dcc_svc_verify_byte(request()->cvOffset_,
                                                  request()->value_);
        break;
      case RequestType::DIRECT_WRITE_BIT:
        serviceModePacket_.set_dcc_svc_verify_bit(request()->cvOffset_,
                                                  request()->bitOffset_,
                                                  request()->value_ != 0);
        break;
      default:
        return exit_unimplemented();
    }
    foundAck_ = 0;
    return invoke_subflow_and_wait(
        backend_, STATE(write_verify_cooldown),
        ProgrammingTrackRequest::SEND_PROGRAMMING_PACKET, serviceModePacket_,
        verifyRepeats_);
  }

  Action write_verify_cooldown() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    if (b->data()->hasAck_) {
      foundAck_ = 1;
    }
    return invoke_subflow_and_wait(backend_, STATE(write_verify_cooldown_done),
                                   ProgrammingTrackRequest::SEND_RESET,
                                   verifyCooldownReset_);
  }

  Action write_verify_cooldown_done() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    if (b->data()->hasAck_) {
      foundAck_ = 1;
    }
    if (foundAck_) {
      if (!hasWriteAck_) {
        LOG(WARNING, "Direct write: write ack missing, but verify is okay.");
      }
      // we're good
      request()->resultCode |= ERROR_CODE_OK;
    } else if (hasWriteAck_) {
      request()->resultCode |= ERROR_FAILED_VERIFY;
    } else {
      request()->resultCode |= ERROR_NO_LOCO;
    }
    return invoke_subflow_and_wait(
        backend_, STATE(return_response),
        ProgrammingTrackRequest::EXIT_SERVICE_MODE);
  }

  /// Root state of reading one byte using direct mode from the decoder.
  Action direct_read_byte() {
    nextBitToRead_ = 0;
    confirmedOnes_ = 0;
    confirmedZeros_ = 0;
    return call_immediately(STATE(read_next_bit));
  }
  
  Action read_next_bit() {
    LOG(INFO, "Testing bit %d", nextBitToRead_);
    serviceModePacket_.set_dcc_svc_verify_bit(request()->cvOffset_,
                                              nextBitToRead_,
                                              true);
    return invoke_subflow_and_wait(
        backend_, STATE(check_next_bit_one),
        ProgrammingTrackRequest::SEND_PROGRAMMING_PACKET, serviceModePacket_,
        verifyRepeats_);
  }

  Action check_next_bit_one() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    LOG(INFO, "bit %d verify ack %u", nextBitToRead_, b->data()->hasAck_);
    if (b->data()->hasAck_) {
      confirmedOnes_ |= (1<<nextBitToRead_);
    }
    return invoke_subflow_and_wait(backend_, STATE(cooldown_next_bit_one),
                                   ProgrammingTrackRequest::SEND_RESET,
                                   verifyCooldownReset_);
  }

  Action cooldown_next_bit_one() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    LOG(INFO, "bit %d cooldown ack %u", nextBitToRead_, b->data()->hasAck_);
    if (b->data()->hasAck_) {
      confirmedOnes_ |= (1<<nextBitToRead_);
    }
    if (nextBitToRead_ == 7) {
      // we're done reading. verify what we have.
      LOG(INFO, "read verify 0x%02x", confirmedOnes_);
      
      serviceModePacket_.set_dcc_svc_verify_byte(request()->cvOffset_,
                                                 confirmedOnes_);
      request()->value_ = confirmedOnes_;
      return invoke_subflow_and_wait(
          backend_, STATE(check_final_byte),
          ProgrammingTrackRequest::SEND_PROGRAMMING_PACKET, serviceModePacket_,
          verifyRepeats_);
    } else {
      ++nextBitToRead_;
      return call_immediately(STATE(read_next_bit));
    }
  }

  Action check_final_byte() {
    auto b = get_buffer_deleter(full_allocation_result(backend_)); 
    LOG(INFO, "read verify ack %u", b->data()->hasAck_);
    if (b->data()->hasAck_) {
      request()->resultCode |= ERROR_CODE_OK;
    } else {
      // send some cooldown too
      return invoke_subflow_and_wait(backend_, STATE(cooldown_final_verify),
                                     ProgrammingTrackRequest::SEND_RESET,
                                     verifyCooldownReset_);
    }
    return invoke_subflow_and_wait(
        backend_, STATE(return_response),
        ProgrammingTrackRequest::EXIT_SERVICE_MODE);
  }

  Action cooldown_final_verify() {
    auto b = get_buffer_deleter(full_allocation_result(backend_)); 
    LOG(INFO, "read verify cooldown ack %u", b->data()->hasAck_);
    if (b->data()->hasAck_) {
      request()->resultCode |= ERROR_CODE_OK;
    } else {
      request()->resultCode |= ERROR_FAILED_VERIFY;
    }
    return invoke_subflow_and_wait(
        backend_, STATE(return_response),
        ProgrammingTrackRequest::EXIT_SERVICE_MODE);
  }
  
  Action return_response() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    return return_with_error(request()->resultCode & (~OPERATION_PENDING));
  }

 private:
  /// How many times by default we send out a programming track verify packet
  /// to get exactly one ack.
  static constexpr unsigned DEFAULT_VERIFY_REPEATS = 5;
  /// How many times by default we send out a programming track verify packet
  /// to get exactly one ack.
  static constexpr unsigned DEFAULT_VERIFY_COOLDOWN = 15;

  /// How many times by default we send out a programming track write packet.
  static constexpr unsigned DEFAULT_WRITE_REPEATS = 5;

  /// Which bits were confirmed to be ones by the decoder.
  uint8_t confirmedOnes_;
  /// Which bits were confirmed to be zeros by the decoder.
  uint8_t confirmedZeros_;
  /// The number of times we have to send out a verify packet to get one
  /// acknowledgement.
  uint8_t verifyRepeats_{DEFAULT_VERIFY_REPEATS};
  /// The number of reset commands to send between verify bit commands.
  uint8_t verifyCooldownReset_{DEFAULT_VERIFY_COOLDOWN};
  /// When reading a byte, which bt we are testing next.
  uint8_t nextBitToRead_ : 4;
  /// 1 if we have an ack (aggregated over an write/verify and the cooldown
  /// reset packets.
  uint8_t foundAck_ : 1;
  /// 1 if the write request has seen an ack pulse.
  uint8_t hasWriteAck_ : 1;
  /// Backend flow for executing low-level programming track requests.
  ProgrammingTrackBackend* backend_;
  /// Holding buffer for the next programming track packet to send.
  dcc::Packet serviceModePacket_;
};

}  // namespace commandstation

#endif  // _COMMANDSTATON_PROGRAMMINGTRACKFRONTEND_HXX_
