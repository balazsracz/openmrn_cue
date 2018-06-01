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
 * Flow and CV space to access the programming track.
 *
 * @author Balazs Racz
 * @date 1 June 2018
 */

#ifndef _COMMANDSTATON_PROGRAMMINGTRACKFRONTEND_HXX_
#define _COMMANDSTATON_PROGRAMMINGTRACKFRONTEND_HXX_

#include "executor/CallableFlow.hxx"
#include "openlcb/MemoryConfig.hxx"
#include "dcc/ProgrammingTrackBackend.hxx"

namespace commandstation {

struct ProgrammingTrackFrontendRequest : public CallableFlowRequestBase {
  enum DirectWriteByte { DIRECT_WRITE_BYTE };
  enum DirectWriteBit { DIRECT_WRITE_BIT };
  enum DirectReadByte { DIRECT_READ_BYTE };
  enum DirectReadBit { DIRECT_READ_BIT };

  /// Request up to write a byte sized CV in direct mode.
  /// @param cv_number is the 1-based CV number (as the user sees it).
  /// @param value is the value to set the CV to.
  void reset(DirectWriteByte, unsigned cv_number, uint8_t value) {
    reset_base();
    cmd_ = Type::DIRECT_WRITE_BYTE;
    cvOffset_ = cv_number - 1;
    value_ = value;
  }

  /// Request up to read a byte sized CV in direct mode.
  /// @param cv_number is the 1-based CV number (as the user sees it).
  void reset(DirectReadByte, unsigned cv_number) {
    reset_base();
    cmd_ = Type::DIRECT_READ_BYTE;
    cvOffset_ = cv_number - 1;
    value_ = 0;
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
};

class ProgrammingTrackFrontend : public CallableFlow<ProgrammingTrackFrontendRequest> {
 public:
  ProgrammingTrackFrontend(ProgrammingTrackBackend* backend)
      : CallableFlow<ProgrammingTrackFrontendRequest>(backend->service()) {}

  typedef ProgrammingTrackFrontendRequest::Type RequestType;

  enum ResultCodes {
    /// Success
    ERROR_CODE_OK = openlcb::Defs::ERROR_CODE_OK,
    /// Error code when the locomotive is not responding to programming track
    /// requests. Usually dirty track or so.
    ERROR_NO_LOCO = openlcb::Defs::ERROR_OPENLCB_TIMEOUT | 1,
    /// Error code when something was invoked that is not implemented.
    ERROR_UNIMPLEMENTED_CMD = openlcb::Defs::ERROR_UNIMPLEMENTED_CMD,
    /// cleared when done is called.
    OPERATION_PENDING = openlcb::DatagramClient::OPERATION_PENDING,
  };

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
      default:
        // Bail out as we have not written this code yet.
        request()->resultCode |= ERROR_UNIMPLEMENTED_CMD;
        return invoke_subflow_and_wait(
            backend_, STATE(return_response),
            ProgrammingTrackRequest::EXIT_SERVICE_MODE);
    }
    return invoke_subflow_and_wait(
        backend_, STATE(pkt_sent),
        ProgrammingTrackRequest::SEND_PROGRAMMING_PACKET, serviceModePacket_,
        3);
  }

  Action pkt_sent() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    if (b->data()->hasAck_) {
      // we're good
      request()->resultCode |= ERROR_CODE_OK;
    } else {
      request()->resultCode |= ERROR_NO_LOCO;
    }
    // Cooldown for the decoder.
    // 
    // @todo: we need to instead create some form of timer for when to exit
    // service mode. Consecutive operations should not exit and enter service
    // mode separately.
    return invoke_subflow_and_wait(backend_, STATE(cooldown_done),
                                   ProgrammingTrackRequest::SEND_RESET, 15);
    
  }

  Action cooldown_done() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    return invoke_subflow_and_wait(
        backend_, STATE(return_response),
        ProgrammingTrackRequest::EXIT_SERVICE_MODE);
  }
  
  Action return_response() {
    auto b = get_buffer_deleter(full_allocation_result(backend_));
    return return_with_error(request()->resultCode & (~OPERATION_PENDING));
  }

 private:
  ProgrammingTrackBackend* backend_;
  dcc::Packet serviceModePacket_;
};

}  // namespace commandstation

#endif  // _COMMANDSTATON_PROGRAMMINGTRACKFRONTEND_HXX_
