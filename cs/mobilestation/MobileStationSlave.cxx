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
 * \file MobileStationSlave.hxx
 *
 * Service that connects to a Marklin MobileStation (I) via CANbus as a slave.
 *
 * @author Balazs Racz
 * @date 16 May 2014
 */

#include "mobilestation/MobileStationSlave.hxx"

#include "custom/HostLogging.hxx"
#include "custom/MCPCanFrameFormat.hxx"
#include "utils/PipeFlow.hxx"
#include "utils/macros.h"
#include "utils/constants.hxx"

namespace mobilestation {

MobileStationSlave* SlaveKeepaliveFlow::service() {
  return static_cast<MobileStationSlave*>(StateFlowBase::service());
}

MobileStationSlave* SlaveInitFlow::service() {
  return static_cast<MobileStationSlave*>(StateFlowBase::service());
}

enum SlaveState {
  STATE_INIT,
  STATE_HANDSHAKE,
  STATE_KEEPALIVE
};

struct MobileStationSlave::StateImpl {
  StateImpl()
      : state(STATE_INIT),
        has_incoming_keepalive(0),
        num_keepalive_no_answer(0),
        slave_address(0) {}
  SlaveState state;
  unsigned has_incoming_keepalive : 1;
  unsigned num_keepalive_no_answer : 4;
  unsigned slave_address : 8;
  unsigned slave_login_offset : 4;
};

MobileStationSlave::MobileStationSlave(ExecutorBase* e, CanIf* device)
    : Service(e),
      state_(new StateImpl),
      device_(device),
      keepalive_flow_(this),
      init_flow_(this) {}

MobileStationSlave::~MobileStationSlave() { delete state_; }

// Send one keepalive message every this often.
DECLARE_CONST(mosta_slave_keepalive_timeout_ms);

enum {
  // Number of missed keepalive periods before we conclude the other side is
  // dead and force back to init state.
  MOSTA_SLAVE_REINIT_COUNT = 8,
  MOSTA_SLAVE_DISCOVER_ID = 0x1c00007e,
  MOSTA_SLAVE_KEEPALIVE_PING_ID = 0x0c000380,
  MOSTA_MASTER_KEEPALIVE_PONG_ID = 0x08000900,
  MOSTA_MASTER_PING_ID = 0x0c000380,
  MOSTA_MASTER_LOGIN_ID = 0x00000380,
  MOSTA_MASTER_SLAVE_QUERY_ID = 0x1c0000fe,
  MOSTA_MASTER_SLAVE_ALIVE_ID = 0x1c0380fe,
  MOSTA_SLAVE_ADDRESS_MASK = 0b01111100 << 8,
};

SlaveKeepaliveFlow::SlaveKeepaliveFlow(MobileStationSlave* service)
    : StateFlowBase(service), timer_(this) {
  start_flow(STATE(call_delay));
}

SlaveKeepaliveFlow::~SlaveKeepaliveFlow() {
  // This typically executed under unit tests and on application exit. Neither
  // of them happen in a production device.
  set_terminated();
  timer_.cancel();
}

SlaveInitFlow::SlaveInitFlow(MobileStationSlave* service)
    : IncomingFrameFlow(service) {
  service->device()->frame_dispatcher()->register_handler(this, 0, 0);
}

SlaveInitFlow::~SlaveInitFlow() {
  service()->device()->frame_dispatcher()->unregister_handler(this, 0, 0);
}

StateFlowBase::Action SlaveKeepaliveFlow::call_delay() {
  return sleep_and_call(&timer_,
                        MSEC_TO_NSEC(config_mosta_slave_keepalive_timeout_ms()),
                        STATE(maybe_send_keepalive));
}

StateFlowBase::Action SlaveKeepaliveFlow::maybe_send_keepalive() {
  if (service()->state_->has_incoming_keepalive) {
    service()->state_->has_incoming_keepalive = 0;
    return call_immediately(STATE(call_delay));
  }
  if (service()->state_->state == STATE_INIT ||
      service()->state_->state == STATE_HANDSHAKE) {
    bracz_custom::send_host_log_event(
        bracz_custom::HostLogEvent::MOSTA_DISCOVER);
    return allocate_and_call(service()->device()->frame_write_flow(),
                             STATE(send_discover));
  } else if (service()->state_->state == STATE_KEEPALIVE) {
    if (++service()->state_->num_keepalive_no_answer >
        MOSTA_SLAVE_REINIT_COUNT) {
      service()->state_->state = STATE_INIT;
      return again();
    }
    bracz_custom::send_host_log_event(bracz_custom::HostLogEvent::MOSTA_PING);
    return allocate_and_call(service()->device()->frame_write_flow(),
                             STATE(send_ping));
  } else {
    DIE("Unknown state of mosta-slave");
    return again();
  }
}

StateFlowBase::Action SlaveKeepaliveFlow::send_discover() {
  auto* b = get_allocation_result(service()->device()->frame_write_flow());
  auto* f = b->data()->mutable_frame();
  SET_CAN_FRAME_EFF(*f);
  SET_CAN_FRAME_ID_EFF(*f, MOSTA_SLAVE_DISCOVER_ID);
  f->can_dlc = 0;
  service()->device()->frame_write_flow()->send(b);
  return call_immediately(STATE(call_delay));
}

StateFlowBase::Action SlaveKeepaliveFlow::send_ping() {
  auto* b = get_allocation_result(service()->device()->frame_write_flow());
  auto* f = b->data()->mutable_frame();
  SET_CAN_FRAME_EFF(*f);
  SET_CAN_FRAME_ID_EFF(*f, MOSTA_SLAVE_KEEPALIVE_PING_ID);
  f->can_dlc = 4;
  f->data[0] = 0;
  f->data[1] = 0;
  f->data[2] = 0;
  f->data[3] = 0;
  service()->device()->frame_write_flow()->send(b);
  return call_immediately(STATE(call_delay));
}

StateFlowBase::Action SlaveInitFlow::entry() {
  uint32_t id = message()->data()->id();
  switch (id) {
    case MOSTA_MASTER_KEEPALIVE_PONG_ID: {
      service()->state_->state = STATE_KEEPALIVE;
      service()->state_->num_keepalive_no_answer = 0;
      bracz_custom::send_host_log_event(
          bracz_custom::HostLogEvent::MOSTA_ALIVE);
      return release_and_exit();
    }
    case MOSTA_MASTER_PING_ID: {
      service()->state_->state = STATE_KEEPALIVE;
      service()->state_->num_keepalive_no_answer = 0;
      bracz_custom::send_host_log_event(bracz_custom::HostLogEvent::MOSTA_PONG);
      release();
      return allocate_and_call(service()->device()->frame_write_flow(),
                               STATE(send_aliveack));
    }
    case MOSTA_MASTER_LOGIN_ID: {
      service()->state_->state = STATE_HANDSHAKE;
      service()->state_->has_incoming_keepalive = 1;
      release();
      return allocate_and_call(service()->device()->frame_write_flow(),
                               STATE(send_login));  // mma pkt 1
    }
  }
  if ((id & ~MOSTA_SLAVE_ADDRESS_MASK) == MOSTA_MASTER_SLAVE_QUERY_ID) {
    service()->state_->slave_address = (id & MOSTA_SLAVE_ADDRESS_MASK) >> 8;
    service()->state_->slave_login_offset = 0;
    release();
    return allocate_and_call(service()->device()->frame_write_flow(),
                             STATE(send_sequence));
  }
  if ((id & ~MOSTA_SLAVE_ADDRESS_MASK) == MOSTA_MASTER_SLAVE_ALIVE_ID) {
    service()->state_->has_incoming_keepalive = 1;
    service()->state_->state = STATE_KEEPALIVE;
    return release_and_exit();
  }
  return release_and_exit();
}

const uint8_t mma_pkt_aliveack[] = {
    /*sid*/ 0x60, 0x08, /*eid*/ 0x03, 0x81, /*len*/ 0x08
    /*data*/,
    0, 0, 0, 1, 0, 5, 0xe6, 0x5e};

StateFlowBase::Action SlaveInitFlow::send_aliveack() {
  auto* b = get_allocation_result(service()->device()->frame_write_flow());
  auto* f = b->data()->mutable_frame();
  bracz_custom::mcp_to_frame(mma_pkt_aliveack, f);
  service()->device()->frame_write_flow()->send(b);
  return exit();
}

StateFlowBase::Action SlaveInitFlow::send_login() {
  auto* b = get_allocation_result(service()->device()->frame_write_flow());
  auto* f = b->data()->mutable_frame();
  SET_CAN_FRAME_EFF(*f);
  SET_CAN_FRAME_ID_EFF(*f, MOSTA_SLAVE_DISCOVER_ID);
  f->can_dlc = 0;
  service()->device()->frame_write_flow()->send(b);
  return exit();
}

const uint8_t mma_pkt_q2[] = {
    /*sid*/ 0xe0, 0xa8, /*eid*/ 0x80, 0x7e, /*len*/ 0x00};

const uint8_t mma_pkt_q3[] = {
    /*sid*/ 0xfc, 0xc9, /*eid*/ 0x00, 0x7e, /*len*/ 0x00};

const uint8_t mma_pkt_q4[] = {
    /*sid*/ 0xeb, 0xa9, /*eid*/ 0x80, 0x7e, /*len*/ 0x00};

// UID and some status bytes.
const uint8_t mma_pkt_hstx1[] = {  // 3b
    /*sid*/ 0xe0, 0x0a, /*eid*/ 0x00, 0x7e, /*len*/ 0x08
    /*data*/,
    0, 5, 0xe6, 0x5e, 0, 2, 0, 3};

// Hardware 2.0 C011
const uint8_t mma_pkt_hstx2[] = {  // 02
    /*sid*/ 0xe0, 0x0a, /*eid*/ 0x80, 0x7e, /*len*/ 0x08
    /*data*/,
    0, 0, 0, 0x97, 2, 0, 0xc0, 0x11};

// "BL" 1.1 and some part of the hardware version again.
const uint8_t mma_pkt_hstx3[] = {  // 03
    /*sid*/ 0xe0, 0x0b, /*eid*/ 0x00, 0x7e, /*len*/ 0x08
    /*data*/,
    1, 1, 2, 0x14, 0, 0, 0xc0, 0x11};

// "AP" 1.7 4011
const uint8_t mma_pkt_hstx4[] = {  // 0f
    /*sid*/ 0xe0, 0x0b, /*eid*/ 0x80, 0x7e, /*len*/ 0x08
    /*data*/,
    0, 0, 0x40, 0x11, 1, 7, 0, 0x2a};

static const uint8_t* const SLAVE_INIT_SEQUENCE[] = {
    mma_pkt_q2,    mma_pkt_q3,    mma_pkt_q4,    mma_pkt_hstx1,
    mma_pkt_hstx2, mma_pkt_hstx3, mma_pkt_hstx4, nullptr};

StateFlowBase::Action SlaveInitFlow::send_sequence() {
  auto* b = get_allocation_result(service()->device()->frame_write_flow());
  auto* f = b->data()->mutable_frame();
  bracz_custom::mcp_to_frame(
      SLAVE_INIT_SEQUENCE[service()->state_->slave_login_offset++], f);
  // Applies the slave address to the message response.
  uint32_t id = GET_CAN_FRAME_ID_EFF(*f);
  id |= service()->state_->slave_address << 8;
  SET_CAN_FRAME_ID_EFF(*f, id);
  cb_.reset(this);
  b->set_done(&cb_);
  service()->device()->frame_write_flow()->send(b);
  return wait_and_call(STATE(next_sequence));
}

StateFlowBase::Action SlaveInitFlow::next_sequence() {
  if (!SLAVE_INIT_SEQUENCE[service()->state_->slave_login_offset]) {
    return exit();
  } else {
    return allocate_and_call(service()->device()->frame_write_flow(),
                             STATE(send_sequence));
  }
}

}  // namespace mobilestation
