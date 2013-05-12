//! Compatibility layer translating from MCP2515 frame format.
#ifndef _BRACZ_TRAIN_CAN_QUEUE_H_
#define _BRACZ_TRAIN_CAN_QUEUE_H_

#include <stdint.h>

typedef enum {
  // Emit as a log item to the USB bus. Has two extra bytes at the beginning of
  // the packet declaring the packet type and length.
  O_LOG_AT = 5,
  // Same as above, but claims that the extra bytes are at pointer - 2. If
  // neither of these are set, assumes that the packet starts at byte 0.
  O_LOG_BEFORE = 1,
  // Declares that there are two useless bytes at the beginning of the packet.
  O_SKIP_TWO_BYTES = 4,
  // ORs eidh_mask to the eidh byte before sending. For limited use (in mosta
  // master/slave protocol).
  O_MARK = 2,
  // Send the packet to the host as well.
  O_HOST = 8
} can_opts_t;

// Use to enqueue a packet ot the end of the queue. if the send queue is full,
// drops packet.
#define CANQueue_SendPacket(p, o) CANQueue_SendPacket_back(p, o)

void CANQueue_SendPacket_back(const uint8_t* packet, can_opts_t opts);

// no separate _front implementation.
#define CANQueue_SendPacket_front(p, o) CANQueue_SendPacket_back(p, o)

extern uint8_t CANQueue_eidh_mask;

#endif // _BRACZ_TRAIN_CAN_QUEUE_H_
