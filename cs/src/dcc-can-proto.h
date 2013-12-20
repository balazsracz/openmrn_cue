// Copyright 2010 Google Inc. All Rights Reserved.
// Author: bracz@google.com (Balazs Racz)
//
// Definitions for the DCC-over-canbus stuff.


#ifndef DCC_CAN_PROTO_H_
#define DCC_CAN_PROTO_H_

#include "usb_proto.h"

struct pkt_t {
  // Always 0.
  unsigned is_pkt : 1;
  // 0: DCC packet, 1: motorola packet.
  unsigned is_marklin : 1;

  // typically for DCC packets:
  // 1: do NOT append an EC byte to the end of the packet.
  unsigned skip_ec : 1;
  // 1: send long preamble instead of packet. 0: send normal preamble and pkt.
  unsigned send_long_preamble : 1;
  // 1: wait for service mode ack and report it back to the host.
  unsigned sense_ack : 1;
  // Repeat count of the packet will be 1 + 5*rept_count. default: 0.
  unsigned rept_count : 2;
};

struct cmd_t {
  // Always 1.
  unsigned is_pkt : 1;
  // Command identifier.
  unsigned cmd : 7;
};


#define MASTER_SIDH 0b11100000
#define MASTER_SIDL 0

#define SLAVE_SIDH 0b11100000
#define SLAVE_SIDL 0b00100000


#define KEEPALIVE_TICKS 5
#define SLAVE_KEEPALIVE_TICKS 8

// Keepalive packet:
// length 2
// byte 0: cmd byte CMD_KEEPALIVE
// byte 1: number of free transmit buffers.

#define CANCMD_KEEPALIVE 1
#define CANCMD_LOG 3
#define CANCMD_ACK 5
#define CANCMD_NOACK 7
#define CANCMD_POWERON 9
#define CANCMD_POWEROFF 11
#define CANCMD_RESET 13
#define CANCMD_DEADBAND 15
#define CANCMD_BREAK 17
#define CANCMD_DISABLE 19
#define CANCMD_ENABLE 21

// log values are defined in usb_proto.h


#endif  // DCC_CAN_PROTO_H_
