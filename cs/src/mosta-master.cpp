#include <string.h>
#include <stdint.h>

#include "base.h"
#include "can-queue.h"
#include "host_packet.h"
#include "usb_proto.h"
#include "pic_can.h"

#define HZ 10



const uint8_t mma_pkt_1[] = {
  14, CMD_CAN_SENT,
  /*sid*/ 0xe0, 0x08, /*eid*/ 0x00, 0x7e, /*len*/ 0x00
  /*data*/,    0,    0,    0,    0,    0,    0,    0,    0
};

const uint8_t mma_pkt_q2[] = {
  14, CMD_CAN_SENT,
  /*sid*/ 0xe0, 0xa8, /*eid*/ 0x80, 0x7e, /*len*/ 0x00
  /*data*/,    0,    0,    0,    0,    0,    0,    0,    0
};

const uint8_t mma_pkt_q3[] = {
  14, CMD_CAN_SENT,
  /*sid*/ 0xfc, 0xc9, /*eid*/ 0x00, 0x7e, /*len*/ 0x00
  /*data*/,    0,    0,    0,    0,    0,    0,    0,    0
};


const uint8_t mma_pkt_q4[] = {
  14, CMD_CAN_SENT,
  /*sid*/ 0xeb, 0xa9, /*eid*/ 0x80, 0x7e, /*len*/ 0x00
  /*data*/,    0,    0,    0,    0,    0,    0,    0,    0
};


// UID and some status bytes.
const uint8_t mma_pkt_hstx1[] = {  //3b
  14, CMD_CAN_SENT,
  /*sid*/ 0xe0, 0x0a, /*eid*/ 0x00, 0x7e, /*len*/ 0x08
  /*data*/,    0,    5, 0xe6, 0x5e,    0,    2,    0,    3
};

// Hardware 2.0 C011
const uint8_t mma_pkt_hstx2[] = {  //02
  14, CMD_CAN_SENT,
  /*sid*/ 0xe0, 0x0a, /*eid*/ 0x80, 0x7e, /*len*/ 0x08
  /*data*/,    0,    0,    0, 0x97,    2,    0, 0xc0, 0x11
};

// "BL" 1.1 and some part of the hardware version again.
const uint8_t mma_pkt_hstx3[] = {  //03
  14, CMD_CAN_SENT,
  /*sid*/ 0xe0, 0x0b, /*eid*/ 0x00, 0x7e, /*len*/ 0x08
  /*data*/,    1,    1,    2, 0x14,    0,    0, 0xc0, 0x11
};

// "AP" 1.7 4011
const uint8_t mma_pkt_hstx4[] = {  //0f
  14, CMD_CAN_SENT,
  /*sid*/ 0xe0, 0x0b, /*eid*/ 0x80, 0x7e, /*len*/ 0x08
  /*data*/,    0,    0, 0x40, 0x11,    1,    7,    0, 0x2a
};

const uint8_t mma_pkt_slave_keepalive[] = {  //41
  14, CMD_CAN_SENT,
  /*sid*/ 0x60, 0x08, /*eid*/ 0x03, 0x80, /*len*/ 0x04
  /*data*/,    0,    0,    0,    0,    0,    0,    0,    0
};

const uint8_t mma_pkt_slave_discover[] = {  //21
  14, CMD_CAN_SENT,
  /*sid*/ 0xe0, 0x08, /*eid*/ 0x00, 0x7e, /*len*/ 0x00
  /*data*/,    0,    0,    0,    0,    0,    0,    0,    0,
};

const uint8_t mma_pkt_aliveack[] = {  //69
  14, CMD_CAN_SENT,
  /*sid*/ 0x60, 0x08, /*eid*/ 0x03, 0x81, /*len*/ 0x08
  /*data*/,    0,    0,    0,    1,    0,    5, 0xe6, 0x5e
};

const uint8_t mma_pkt_erstop[] = {  //
  14, CMD_CAN_SENT,
  /*sid*/ 0x40, 0x08, /*eid*/ 0x09, 0x01, /*len*/ 0x03
  /*data*/,    1,    0,    1,    0,    0,    0,    0,    0
};



#define STATE_INIT 0
#define STATE_HANDSHAKE 1
#define STATE_KEEPALIVE 2

static uint8_t state_ = STATE_INIT;
static uint8_t timer_ = 0;
static uint8_t keepalive_dead_timeout_ = 0;
uint8_t bytecounter_ = 0;

#define PENDING_PACKET_COUNT 16
#define PENDING_PACKET_MASK 15



const uint8_t log_into_hs[] = { 3, CMD_SYSLOG, LOG_MMA_INTO_HANDSHAKE, 0 };
const uint8_t log_hs_sent[] = { 3, CMD_SYSLOG, LOG_MMA_HS_SENT, 0 };


const uint8_t log_discover[] = { 2, CMD_CAN_LOG, '%' };
const uint8_t log_ping[] = { 2, CMD_CAN_LOG, ',' };
const uint8_t log_pong[] = { 2, CMD_CAN_LOG, '.' };
const uint8_t log_alive[] = { 2, CMD_CAN_LOG, '_' };
const uint8_t log_newline[] = { 2, CMD_CAN_LOG, '\n' };

void UpdateByteCounter() {
  if (bytecounter_++ > 40) {
    bytecounter_ = 0;
    PacketQueue::instance()->TransmitConstPacket(log_newline);
  }
  CANSetDoNotLogToHost();
}

#define SendPacket(p, o) CANQueue_SendPacket(p, o)

void MoStaMaster_HandlePacket(const PacketBase& can_buf) {
    //if (!CANIsDestination(CDST_MOSTACLIENT)) return;
  CANSetPending(CDST_MOSTACLIENT);
  if (CANMATCH(0x4008, 0x0900)) {
    state_ = STATE_KEEPALIVE;
    keepalive_dead_timeout_ = 0;
    PacketQueue::instance()->TransmitConstPacket(log_alive);
    UpdateByteCounter();
  } else if (CANMATCH(0x6008, 0x0380)) {
    state_ = STATE_KEEPALIVE;
    keepalive_dead_timeout_ = 0;
    PacketQueue::instance()->TransmitConstPacket(log_pong);
    SendPacket(mma_pkt_aliveack, O_SKIP_TWO_BYTES);
    UpdateByteCounter();
  } else if (CANMATCH(0x0008, 0x0380)) {  //R18
    SendPacket(mma_pkt_1, O_LOG_AT);
    PacketQueue::instance()->TransmitConstPacket(log_into_hs);
    state_ = STATE_HANDSHAKE;
    timer_ = 0;
  } else if (CANMMATCH(0xe008,0x00fe)) {
    CANQueue_eidh_mask = can_buf[CAN_EIDH] & 0b01111100;
    PacketQueue::instance()->TransmitConstPacket(log_hs_sent);
    SendPacket(mma_pkt_q2, (can_opts_t)(O_LOG_AT | O_MARK));
    SendPacket(mma_pkt_q3, (can_opts_t)(O_LOG_AT | O_MARK));
    SendPacket(mma_pkt_q4, (can_opts_t)(O_LOG_AT | O_MARK));

    SendPacket(mma_pkt_hstx1, (can_opts_t)(O_LOG_AT | O_MARK));
    SendPacket(mma_pkt_hstx2, (can_opts_t)(O_LOG_AT | O_MARK));
    SendPacket(mma_pkt_hstx3, (can_opts_t)(O_LOG_AT | O_MARK));
    SendPacket(mma_pkt_hstx4, (can_opts_t)(O_LOG_AT | O_MARK));
  } else if (CANMMATCH(0xe00b,0x80fe)) {
    state_ = STATE_KEEPALIVE;
    timer_ = 0;
  }
  CANSetNotPending(CDST_MOSTACLIENT);
}

void MoStaMaster_Timer() {
  timer_++;
  if (state_==STATE_KEEPALIVE && ++keepalive_dead_timeout_ > (HZ << 2)) {
    // Drop back into init state - we lost communications.
    state_ = STATE_INIT;
  }
  if (state_ == STATE_KEEPALIVE && timer_ > (HZ>>2)) {
    SendPacket(mma_pkt_slave_keepalive, O_SKIP_TWO_BYTES);
    timer_ = 0;
    PacketQueue::instance()->TransmitConstPacket(log_ping);
    UpdateByteCounter();
  } else if ((state_ == STATE_INIT || state_ == STATE_HANDSHAKE) &&
             timer_ > (HZ >> 1)) {
    timer_ = 0;
    SendPacket(mma_pkt_slave_discover, O_LOG_AT);
    PacketQueue::instance()->TransmitConstPacket(log_discover);
    UpdateByteCounter();
  }
}

void MoStaMaster_EmergencyStop() {
    // TODO(bracz): will this actually cause the DCC controller to stop as well?
  CANQueue_SendPacket_front(mma_pkt_erstop, O_LOG_AT);
}
