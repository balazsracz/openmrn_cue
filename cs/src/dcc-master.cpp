#include <string.h>

#include "host_packet.h"
#include "pic_can.h"

#include "dcc-master.h"
#include "base.h"
#include "can-queue.h"
#include "dcc-can-proto.h"
#include "cs_config.h"

#ifndef STATEFLOW_CS


#define PRIORITY_SIZE 4

static uint8_t log_pkt_to_host_ = 0;

#define LOG_PKT_TO_HOST log_pkt_to_host_  // or: CDST_HOST
//#define LOG_REFRESH_STATE_TO_HOST

typedef struct {
  // address of loco. Address 0xff means the loco entry is unused.
  uint8_t address;
  union {
    struct {
      unsigned speed : 7;
      // 0 if forward, 1 if reverse
      unsigned dir : 1;
    } p;
    uint8_t raw;
  } speed;
  // function buttons. bit 0: lights. bits 1-7: f1-f7. 1=on, 0=off.
  uint16_t fn;

  // raw speed will be multiplied with this to get the actual speed.
  uint8_t relative_speed;

  const char* name;
  uint8_t namelen;

  // Number of logical functions populated.
  uint8_t fncount;

  // Tells us where we are in giving out the function mappings.
  // Always shows the next F number to send. Valid values: 1..7.
  unsigned loop_position : 4;
  // This is 1 if the next packet to send should be a speed packet, 0 if a
  // function packet.
  unsigned loop_at_speed : 1;

  // 0: dcc, 1: marklin.
  unsigned drive_type : 3;
  // set to 1 if the direction changed from the last state. Cleared when a
  // speed of zero is set without changing the direction. Ignored if speed is
  // not zero.
  unsigned dir_changed : 1;

  // If 1, then the current train is paused.
  unsigned paused : 1;
  // If 1, then the current train forward/backward is flipped.
  unsigned reversed : 1;

  // 1: this loco is available for push-pull operation.
  unsigned is_pushpull : 1;
} dcc_loco_t;

dcc_loco_t dcc_master_loco[DCC_NUM_LOCO];

enum DccMode {
  MARKLIN_OLD = 0,
  MARKLIN_NEW = 1,
  MFX = 2,
  DCC_14 = 4,
  DCC_28 = 5,
  DCC_128 = 6,
  PUSHPULL = 8,
  MARKLIN_TWOADDR = 16
};


#define DCC_MAX_FN 22

struct const_loco_db_t {
  const uint8_t address;
  // Maps the logical function numbers ot F bits. 0xff terminates the array.
  const uint8_t function_mapping[DCC_MAX_FN];
  // MoSta function definition, parallel to function_mapping.
  const uint8_t function_labels[DCC_MAX_FN];
  // Any zero character ends, but preferable to pad it with zeroes.
  const char name[16];
  const uint8_t mode;
};


enum Symbols {
  LIGHT = 1,
  BEAMER = 2,
  TELEX = 6,
  ABV = 8,
  SMOKE = 10,
  FNT11 = 11,
  FNT12 = 12,
  ENGINE = 13,
  LIGHT1 = 14,
  LIGHT2 = 15,

  HONK = 132,
  WHISTLE = 133,
  FNP = 139,
  SPEECH = 140,
  SOUNDP = 141

};

/*
1       light
8       ABV
2       beamer
132     honk
13      sound
140     music (speech)
141     sound
6       telex
133     pfeife
10      rauch
12      F
139     F
14      light 1
15      light 2
*/


/*const struct const_loco_db_t const_lokdb[] = {
  { 27, { 2, 3,  0xff, }, { LIGHT, ABV,  0xff, },
    "WLE ER20", MARKLIN_NEW },
  { 0, }
  };*/



const struct const_loco_db_t const_lokdb[] = {
  // 0
  { 50, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 0xff} , { LIGHT, ENGINE, HONK, SPEECH, SPEECH, SPEECH, SPEECH, LIGHT1, FNP, ABV, HONK, SOUNDP, SOUNDP, SOUNDP, HONK, HONK, HONK, HONK, HONK, HONK, 0xff }, "ICN", DCC_28 | PUSHPULL },
  // 1
  { 51, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, FNT11, ABV,  0xff, },
    "BR 260417", DCC_28 },
  // 2
  { 12, { 0, 2, 3, 4,  0xff, }, { LIGHT, BEAMER, HONK, ABV,  0xff, },
    "RE 474 014-8", MFX }, // todo: check fnbits
  // 3
  { 2 , { 0, 2, 3, 4,  0xff, }, { LIGHT, HONK, FNT12, ABV,  0xff, },
    "ICE 2", MARKLIN_NEW | PUSHPULL }, // todo: check fnbits
  // 4
  { 22, { 0, 3, 4,  0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "RE 460 TSR", MARKLIN_NEW }, // todo: there is no beamer here
  // 5
  { 32, { 0, 4,  0xff, }, { LIGHT, ABV,  0xff, },
    "RTS RAILTR", MARKLIN_NEW },
  // 6
  { 61, { 0, 1, 2, 3, 4,  0xff, }, { LIGHT, ENGINE, LIGHT2, HONK, 7,  0xff, },
    "MAV M61", DCC_28 }, // todo: check fn definition and type mapping
  // 7
  { 46, { 0, 1, 2, 3, 4, 5, 0xff, }, { LIGHT, HONK, ENGINE, FNT11, ABV, BEAMER,  0xff, },
    "RE 460 118-2", MFX | PUSHPULL },  // todo: there is F5 with beamer that can't be switched.
  // 8
  { 15, { 0, 1, 3, 4,  0xff, }, { LIGHT, BEAMER, FNT11, ABV,  0xff, },
    "RE 4/4 II", DCC_14 }, // todo: snail mode
  // 9
  { 47, { 0,  0xff, }, { LIGHT,  0xff, },
    "RE 465", MARKLIN_OLD | PUSHPULL },
  // id 10
  { 72, { 0, 1, 4, 0xff, }, { LIGHT, LIGHT1, ABV, 0xff },
    "DHG 700", MARKLIN_NEW },
  // id 11
  { 29, { 0, 4,  0xff }, { LIGHT, ABV,  0xff, },
    "BR 290 022-3", MFX },
  // id 12
  { 43, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, FNT11, ABV,  0xff, },
    "Am 843 093-6", DCC_28 },
  // id 13
  { 27, { 0, 4,  0xff, }, { LIGHT, ABV,  0xff, },
    "WLE ER20", MARKLIN_NEW },
  // id 14
  { 58, { 0, 3, 4,  0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "185 595-6", DCC_28 }, // NOTE: hardware is programmed for addr 18
  // id 15
  { 26, { 0,  0xff, }, { LIGHT,  0xff, },
    "Re 460 HAG", MARKLIN_OLD | PUSHPULL },
  // id 16
  { 38, { 0,  0xff, }, { LIGHT,  0xff, },
    "BDe 4/4 1460", MARKLIN_OLD | PUSHPULL },
  // id 17
  { 48, { 0,  0xff, }, { LIGHT,  0xff, },
    "Re 4/4 II 11239", MARKLIN_NEW },
  // id 18
  { 66, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "Re 6/6 11665", DCC_128 },
  // id 19
  { 3, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "RBe 4/4 1423", DCC_28 | PUSHPULL },
  // id 20
  { 24, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "Taurus", MARKLIN_NEW },
  // id 21
  { 52, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "RBe 4/4 1451", MARKLIN_NEW },
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
};

const uint8_t dcc_erstop[] = {  //
  14, CMD_CAN_SENT,
  /*sid*/ SLAVE_SIDH, SLAVE_SIDL, /*eid*/ 0x00, 0x00, /*len*/ 0x01
  /*data*/, CANCMD_POWEROFF,    0,    0,    0,    0,    0,    0,    0
};

const uint8_t dcc_go[] = {  //
  14, CMD_CAN_SENT,
  /*sid*/ SLAVE_SIDH, SLAVE_SIDL, /*eid*/ 0x00, 0x00, /*len*/ 0x01
  /*data*/, CANCMD_POWERON,    0,    0,    0,    0,    0,    0,    0
};

const uint8_t dcc_keepalive[] = {  //
  14, CMD_CAN_SENT,
  /*sid*/ SLAVE_SIDH, SLAVE_SIDL, /*eid*/ 0x00, 0x00, /*len*/ 0x01
  /*data*/, CANCMD_KEEPALIVE,    0,    0,    0,    0,    0,    0,    0
};


const uint8_t dcc_packet_preamble[] = {  //
  14, CMD_CAN_SENT,
  /*sid*/ SLAVE_SIDH, SLAVE_SIDL, /*eid*/ 0x00, 0x00, /*len*/ 0x01,
  0b00000000
};

const uint8_t marklin_packet_preamble[] = {  //
  14, CMD_CAN_SENT,
  /*sid*/ SLAVE_SIDH, SLAVE_SIDL, /*eid*/ 0x00, 0x00, /*len*/ 0x01,
  0b00100110
};

const uint8_t dcc_service_mode_response[] = {  //
  14, CMD_CAN_PKT,
  /*sid*/ 0xc0, 0x48, /*eid*/ 0x01, 0x00, /*len*/ 7,
  1, 2, 0xFF, 0, 0, 0, 0,
};

uint8_t can_resp_buf[14];

const uint8_t mosta_setfn_response[] = {  //
  /*sid*/ 0x40, 0x48, /*eid*/ 0x01, 0x00, /*len*/ 7,
  1, 2, 0xFF, 0, 0, 0, 0,
};


enum WriteState {
  SST_REQUEST = 1,
  SST_ENTER_IDLE,
  SST_ENTER_RESET_PRE,
  SST_ENTER_RESET,
  SST_SEND_WRITE = 5,
  SST_WAIT_WRITE,
  SST_WAIT_WRITE_2,
  SST_VERIFY = 8,
  SST_WAIT_VERIFY,
  SST_WAIT_VERIFY_2 = 10,
  SST_SEND_READ_BITS,
  SST_WAIT_READ,
  SST_WAIT_READBIT,
  SST_EXIT_RESET,
};

enum ServiceModeRequest {
  SRQ_READ,
  SRQ_WRITE,
  SRQ_VERIFY
};


#define SendPacket(p, o) CANQueue_SendPacket(p, o)



const uint8_t dlog_ping[] = { 2, CMD_CAN_LOG, '^' };
const uint8_t dlog_alive[] = { 2, CMD_CAN_LOG, '*' };
const uint8_t dlog_sent[] = { 2, CMD_CAN_LOG, '@' };
const uint8_t dlog_newline[] = { 2, CMD_CAN_LOG, '\n' };


void UpdateByteCounter();

struct dcc_master_state_t {
  uint8_t free_packet_count;
  unsigned alive : 1;
  unsigned enabled : 1;
  unsigned service_mode : 1;
  unsigned service_mode_request : 2;
  unsigned service_mode_had_ack : 1;
  unsigned service_mode_had_noack : 1;

  uint8_t service_mode_id;
  int service_mode_cv;
  uint8_t service_mode_value;
  uint8_t service_mode_state;
  uint8_t service_mode_next_bit;
  uint8_t service_mode_retry_counter;
#define DCC_SERVICE_RETRY_COUNT 10

  // Holds the id of the last loco that we have addressed.
  uint8_t last_loco_id;

  // Length of the priority array.
  uint8_t priority_len;
  // NUmber of valid locos.
  uint8_t num_locos;

  struct prio {
    // loco ID - 0..DCC_NUM_LOCO - 1.
    unsigned id : 5;
    // which item changed. 15 = speed.
    unsigned what : 4;
#define PRIO_LSPEED 14
#define PRIO_SPEED 15
  } priority[PRIORITY_SIZE];

  /*uint8_t preamble[6];
  uint8_t len;
  struct pkt_t packetbits;
  uint8_t buffer[4];*/
} dcc_master;


// Only call with valid 'id'.
static uint8_t GetSpeedForTrain(uint8_t id) {
  if (dcc_master_loco[id].paused) return 0;
  unsigned speed = dcc_master_loco[id].speed.p.speed;
  speed *= dcc_master_loco[id].relative_speed;
  speed >>= 4;
  if (speed > 64) speed = 64;
  return speed;
}

// Only call with valid 'id'. Returns 0 for forward, 1 for reverse.
static uint8_t IsTrainMovingReverse(uint8_t id) {
  return dcc_master_loco[id].speed.p.dir ^ dcc_master_loco[id].reversed;
}

// Adds an entry to the back of the priority queue. Returns 1 if added
// successful, 0 if failed (queue full).
static uint8_t AddToPriorityQueue(uint8_t id, uint8_t what) {
#ifdef __FreeRTOS__
  taskENTER_CRITICAL();
#endif
  uint8_t myofs = dcc_master.priority_len;
  uint8_t ret = 0;
  if (myofs < PRIORITY_SIZE) {
    dcc_master.priority_len++;
    dcc_master.priority[myofs].id = id;
    dcc_master.priority[myofs].what = what;
    ret = 1;
  }
#ifdef __FreeRTOS__
  taskEXIT_CRITICAL();
#endif
  return ret;
}

uint8_t DccLoop_HasPower() {
  return dcc_master.alive;
}

static void DccLoop_HandlePacketToMaster(const PacketBase& can_buf) {
  uint8_t cmd = can_buf[CAN_D0];
  switch(cmd) {
    case CANCMD_KEEPALIVE: {
      SendPacket(dcc_keepalive, O_SKIP_TWO_BYTES);
      dcc_master.free_packet_count = can_buf[CAN_D1];
      PacketQueue::instance()->TransmitConstPacket(dlog_ping);
      dcc_master.alive = can_buf[CAN_D2] ? 1 : 0;
      if (can_buf[CAN_D2]) {
        PacketQueue::instance()->TransmitConstPacket(dlog_alive);
      }
      UpdateByteCounter();
      // Skip printing the log to the host.
      CANSetDoNotLogToHost();
      //if (dcc_master.service_mode) can_destination |= CDST_HOST;
      break;
    }
    case CANCMD_LOG: {
      CANSetDoNotLogToHost();
      break;
    }
    case CANCMD_ACK: {
      dcc_master.service_mode_had_ack = 1;
      break;
    }
    case CANCMD_NOACK: {
      dcc_master.service_mode_had_noack = 1;
      break;
    }
    case CANCMD_DISABLE: {
      dcc_master.enabled = 0;
      break;
    }
    case CANCMD_ENABLE: {
      dcc_master.enabled = 1;
      break;
    }
    default:
      CANSetDoNotLogToHost();
  }
}

static void RespondLocoFnValue(uint8_t id, uint8_t what) {
  uint8_t can_resp_buf[10];
  can_resp_buf[0] = 0x40;
  can_resp_buf[1] = 0x48;
  can_resp_buf[2] = 1 | (id << 2);
  can_resp_buf[3] = 0;
  can_resp_buf[4] = 3;
  can_resp_buf[5] = what;
  can_resp_buf[6] = 0;
  if (what == 32) {
    can_resp_buf[7] = dcc_master_loco[id].paused;
  } else if (what == 33) {
    can_resp_buf[7] = dcc_master_loco[id].reversed;
  } else if (what == 1) {
    can_resp_buf[7] = dcc_master_loco[id].speed.raw;
  } else {
    what -= 2;
    if (what >= dcc_master_loco[id].fncount) return;
    what = const_lokdb[id].function_mapping[what];
    if (dcc_master_loco[id].fn & (1<<what)) {
      can_resp_buf[7] = 1;
    } else {
      can_resp_buf[7] = 0;
    }
  }
  SendPacket(can_resp_buf, O_HOST);
}

uint8_t DccLoop_IsUnknownLoco(uint8_t id) {
  if (id >= DCC_NUM_LOCO || !dcc_master_loco[id].address) {
    return 1;
  }
  return 0;
}

uint8_t DccLoop_IsLocoPushPull(uint8_t id) {
  if (id >= DCC_NUM_LOCO || !dcc_master_loco[id].address) {
    return 0;
  }
  return dcc_master_loco[id].is_pushpull;
}

uint8_t DccLoop_SetLocoPaused(uint8_t id, uint8_t is_paused) {
  if (id >= DCC_NUM_LOCO || !dcc_master_loco[id].address) {
    return 1;
  }
  dcc_master_loco[id].paused = is_paused ? 1 : 0;
  RespondLocoFnValue(id, 32);
  if (AddToPriorityQueue(id, PRIO_SPEED)) {
    return 0;
  } else {
    return 2;
  }
}

uint8_t DccLoop_SetLocoFn(uint8_t id, uint8_t fn, uint8_t value) {
  if (id >= DCC_NUM_LOCO || !dcc_master_loco[id].address || fn > 7) {
    return 1;
  }
  if (value) {
    dcc_master_loco[id].fn |= (1<<fn);
  } else {
    dcc_master_loco[id].fn &= ~(1<<fn);
  }
  if (AddToPriorityQueue(id, fn)) {
    return 0;
  } else {
    return 2;
  }
}

void DccLoop_SetLocoAbsoluteSpeed(uint8_t id, uint8_t abs_speed) {
  if (id >= DCC_NUM_LOCO || !dcc_master_loco[id].address) {
    return;
  }
  dcc_master_loco[id].speed.raw = abs_speed;
  if ((abs_speed ^ dcc_master_loco[id].speed.raw) & 0x80) {
    dcc_master_loco[id].dir_changed = 1;
  }
  RespondLocoFnValue(id, 1);
  AddToPriorityQueue(id, PRIO_LSPEED);
}

void DccLoop_SetLocoRelativeSpeed(uint8_t id, uint8_t rel_speed) {
  if (id >= DCC_NUM_LOCO || !dcc_master_loco[id].address) {
    return;
  }
  if (dcc_master_loco[id].relative_speed != rel_speed) {
    dcc_master_loco[id].relative_speed = rel_speed;
    AddToPriorityQueue(id, PRIO_LSPEED);
  }
}

uint8_t DccLoop_GetLocoRelativeSpeed(uint8_t id) {
  if (id >= DCC_NUM_LOCO || !dcc_master_loco[id].address) {
    return 0xff;
  }
  return dcc_master_loco[id].relative_speed;
}

// Returns non-zero if loco is reversed.
uint8_t DccLoop_IsLocoReversed(uint8_t id) {
  return dcc_master_loco[id].reversed;
}

// Sets a given loco's is_reversed state.
void DccLoop_SetLocoReversed(uint8_t id, uint8_t is_reversed) {
  if (is_reversed != dcc_master_loco[id].reversed) {
    dcc_master_loco[id].reversed = is_reversed;
    dcc_master_loco[id].dir_changed = 1;
    AddToPriorityQueue(id, PRIO_SPEED);
  }
  RespondLocoFnValue(id, 33);
}

void DccLoop_HandlePacket(const PacketBase& can_buf) {
    // TODO(bracz): This needs some equivalent upstream.
    //if (!CANIsDestination(CDST_DCCMASTER)) {
    //return;
    //}
  CANSetPending(CDST_DCCMASTER);
  if (can_buf[CAN_SIDH] == MASTER_SIDH &&
      can_buf[CAN_SIDL] == MASTER_SIDL &&
      can_buf[CAN_LEN] > 0) {
    DccLoop_HandlePacketToMaster(can_buf);
  }
  if (SIDMATCH(0x4008) &&
      // THis accepts only from the slave (host and slave mosta). If need to
      // accept from master mosta, need to replace with EIDMMMATCH. The problem
      // is that master mosta sends keepalives with this sid/eid.
      EIDMATCH(0x0901) &&
      can_buf[CAN_LEN] == 3 &&
      can_buf[CAN_D0] == 1 &&
      can_buf[CAN_D1] == 0) {
    if (can_buf[CAN_D2]) {
      DccLoop_EmergencyStop();
    } else {
      CANQueue_SendPacket((uint8_t*)dcc_go, O_LOG_AT);
    }
  }
  while (SIDMATCH(0x4048) &&
         (can_buf[CAN_EIDL] & 0xfe) == 0 &&
         (can_buf[CAN_EIDH] & 3) == 1 &&
         can_buf[CAN_LEN] == 3 &&
         can_buf[CAN_D1] == 0) {
    uint8_t id = can_buf[CAN_EIDH] >> 2;
    if (id >= dcc_master.num_locos) break;
    if (!can_buf[CAN_D0]) break;
    uint8_t what = 14;
    if (can_buf[CAN_D0] == 1) {
      // Set speed command.
      if ((can_buf[CAN_D2] ^ dcc_master_loco[id].speed.raw) & 0x80) {
        dcc_master_loco[id].dir_changed = 1;
      } else {
        dcc_master_loco[id].dir_changed = 0;
      }
      dcc_master_loco[id].speed.raw = can_buf[CAN_D2];
      AddToPriorityQueue(id, PRIO_LSPEED);
    } else {
      what = can_buf[CAN_D0];
      if (what == 32) {
        DccLoop_SetLocoPaused(id, can_buf[CAN_D2]);
      } else if (what == 33) {
        DccLoop_SetLocoReversed(id, can_buf[CAN_D2]);
      } else {
        what -= 2;
        if (what >= dcc_master_loco[id].fncount) break;
        what = const_lokdb[id].function_mapping[what];
        if (can_buf[CAN_D2]) {
          dcc_master_loco[id].fn |= (1<<what);
        } else {
          dcc_master_loco[id].fn &= ~(1<<what);
        }
        AddToPriorityQueue(id, what);
      }
    }
    break;
  }
  if ((SIDMATCH(0x4048) || SIDMATCH(0xC048)) &&
      (can_buf[CAN_EIDL] & 0xfe) == 0 &&
      (can_buf[CAN_EIDH] & 3) == 1 &&
      can_buf[CAN_D1] == 0 &&
      ((can_buf[CAN_LEN] == 2 &&
        can_buf[CAN_D0] >= 32) ||
       can_buf[CAN_EIDH] >= 40 ||
       can_buf[CAN_D0] >= 0xb)) {
    RespondLocoFnValue(can_buf[CAN_EIDH] >> 2, can_buf[CAN_D0]);
  }
  while (SIDMATCH(0xC048) &&
      can_buf[CAN_LEN] >= 5 &&
      can_buf[CAN_D0] == 1 &&
      can_buf[CAN_D1] == 2 &&
      can_buf[CAN_D2] == 0xff) {
    dcc_master.service_mode_id = can_buf[CAN_EIDH] >> 2;
    dcc_master.service_mode_cv = can_buf[CAN_D3];
    dcc_master.service_mode_cv <<= 8;
    dcc_master.service_mode_cv |= can_buf[CAN_D4];
    dcc_master.service_mode_cv--;
    dcc_master.service_mode = 0;
    if (can_buf[CAN_LEN] == 7 && can_buf[CAN_D5] == 0) {
      dcc_master.service_mode = 1;
      dcc_master.service_mode_request = SRQ_WRITE;
      dcc_master.service_mode_value = can_buf[CAN_D6];
      dcc_master.service_mode_state = SST_REQUEST;
    } else if (can_buf[CAN_LEN] == 7 && can_buf[CAN_D5] == 1) {
      dcc_master.service_mode = 1;
      dcc_master.service_mode_request = SRQ_VERIFY;
      dcc_master.service_mode_value = can_buf[CAN_D6];
      dcc_master.service_mode_state = SST_REQUEST;
    } else {
      dcc_master.service_mode = 1;
      dcc_master.service_mode_request = SRQ_READ;
      dcc_master.service_mode_state = SST_REQUEST;
    }
    break;
  }
  CANSetNotPending(CDST_DCCMASTER);
}

static void SendDccPacket(uint8_t cmd, uint8_t len,
                          uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
  uint8_t can_buf[15];
  memcpy(can_buf + CAN_START, dcc_packet_preamble + 2, 6);
  can_buf[CAN_LEN] = len + 1;
  can_buf[CAN_D0] = cmd;
  can_buf[CAN_D1] = d1;
  can_buf[CAN_D2] = d2;
  can_buf[CAN_D3] = d3;
  can_buf[CAN_D4] = d4;

  can_buf[0] = 1 + 5 + (can_buf[CAN_LEN] & 0xf);
  can_buf[1] = CMD_CAN_PKT;

  can_opts_t opts = O_SKIP_TWO_BYTES;
  if (LOG_PKT_TO_HOST) opts = (can_opts_t) (O_SKIP_TWO_BYTES | O_HOST);
  CANQueue_SendPacket(can_buf, opts);
  --dcc_master.free_packet_count;
  PacketQueue::instance()->TransmitConstPacket(dlog_sent);
}

static void SendMarklinPacket(uint8_t cmd, uint8_t d1, uint8_t d2, uint8_t d3) {
  uint8_t can_buf[15];
  memcpy(can_buf + CAN_SIDH, marklin_packet_preamble + 2, 6);
  can_buf[CAN_LEN] = 3 + 1;
  can_buf[CAN_D0] = cmd;
  can_buf[CAN_D1] = d1;
  can_buf[CAN_D2] = d2;
  can_buf[CAN_D3] = d3;

  can_opts_t opts = O_SKIP_TWO_BYTES;
  if (LOG_PKT_TO_HOST) opts = (can_opts_t) (O_SKIP_TWO_BYTES | O_HOST);
  CANQueue_SendPacket(can_buf, opts);
  --dcc_master.free_packet_count;
  PacketQueue::instance()->TransmitConstPacket(dlog_sent);
}


static const uint8_t marklin_address[3] = {0b00, 0b11, 0b10};
static const uint8_t marklin_fn_bits[5] =
{ 0, 0b01010000, 0b00000100, 0b00010100, 0b01010100 };

// Variables used by marklin packet generators.
static uint8_t cmd, b0, b1, b2;

#define MARKLIN_DEFAULT_CMD 0b00100110
#define DCC_DEFAULT_CMD 0
#define DCC_LONG_PREAMBLE_CMD 0b00001100
#define DCC_SERVICE_MODE_5X_WITH_ACK_CMD 0b00111000
// standard dcc packet with 5x repeat.
#define DCC_SERVICE_MODE_5X_CMD 0b00101000
#define DCC_SERVICE_MODE_1X_CMD 0b00001000

// Fills in b0, b1, and certain bits of b2 with address and speed information
// according to the (v1/v2) Marklin protocol.
// Returns the speed step number (0,2..15).
uint8_t GetMarklinAddressAndSpeed(
    uint8_t address, int sp) {
  b0 = 0; b1 = 0; b2 = 0;
  b0 |= marklin_address[address % 3];
  address /= 3;
  b1 |= marklin_address[address % 3] << 6;
  address /= 3;
  b1 |= marklin_address[address % 3] << 4;
  address /= 3;
  b1 |= marklin_address[address % 3] << 2;

  if (sp > 0) sp += 8;
  sp /= 9;
  if (sp > 0) sp++;
  if (sp > 15) sp = 15;
  if (sp & 1) b2 |= 0x80;
  if (sp & 2) b2 |= 0x20;
  if (sp & 4) b2 |= 0x08;
  if (sp & 8) b2 |= 0x02;
  return sp;
}


// Sends a function packet to the loco containing the given position.
static void SendLocoFnPacket(uint8_t loco, uint8_t position, uint8_t prio) {
  b0 = 0; b1 = 0; b2 = 0;
  if (!(dcc_master_loco[loco].drive_type & 0b100)) {
    // Marklin mode.
    if ((dcc_master_loco[loco].drive_type & 0b011) < MARKLIN_NEW) return;
    if (position > 4 &&
        (dcc_master_loco[loco].drive_type & 0b1000) == 0) return;
    uint8_t sp = GetMarklinAddressAndSpeed(dcc_master_loco[loco].address +
                                         (position > 4 ? 1 : 0),
                                         GetSpeedForTrain(loco));

    // The command bits to send through the canbus.
    cmd = MARKLIN_DEFAULT_CMD;
    if (prio) cmd |= 0b01100000;  // i think this ups repetition count.

    if (dcc_master_loco[loco].fn & 1) {
      // Turns on light (aux function).
      b1 ^= 0b11;
    }


    if (dcc_master_loco[loco].fn & (1 << position)) b2 |= 1;
    if (position > 4) position -= 4;
    b2 |= marklin_fn_bits[position];
    // Check for exceptions, when we generated a packet valid for the old
    // protocol.
    if (((b2 & 0xaa) >> 1) == (b2 & 0x55)) {
      b2 &= 0xaa;
      if (sp >= 8) {
        // High speeds. new bits are 0101.
        b2 |= 0b00010001;
      } else {
        // Low speeds. New bits are 1010.
        b2 |= 0b01000100;
      }
    }
    SendMarklinPacket(cmd, b0, b1, b2);
    return;
  } else {
    cmd = DCC_DEFAULT_CMD;
    if (prio) cmd |= 0b00100000;
    // Dcc mode.
    uint8_t pktlen = 2;
    b2 = 0;
    b0 = dcc_master_loco[loco].address & 0x7f;
    if (position <= 4) {
      b1 = 0b10000000;
      if (dcc_master_loco[loco].fn & 1) {
        // Turns on light (F0).
        b1 |= 0b00010000;
      }
      b1 |= (dcc_master_loco[loco].fn & 0b00011110) >> 1;
    } else if (position <= 8) {
      b1 = 0b10110000;
      b1 |= (dcc_master_loco[loco].fn >> 5) & 0x0f;
    } else if (position <= 12) {
      b1 = 0b10100000;
      b1 |= (dcc_master_loco[loco].fn >> 9) & 0x0f;
    } else {
      b1 = 0b11011110;
      b2 = (dcc_master_loco[loco].fn >> 13) & 0xff;
      pktlen = 3;
    }
    SendDccPacket(cmd, pktlen, b0, b1, b2, 0);
  }
}



static void SendDccLocoSpeedPacket(uint8_t loco, uint8_t prio) {
  if (!(dcc_master_loco[loco].drive_type & 0b100)) {
    // Marklin mode.
    uint8_t sp = GetMarklinAddressAndSpeed(dcc_master_loco[loco].address,
                                         GetSpeedForTrain(loco));

    // The command bits to send through the canbus.
    cmd = MARKLIN_DEFAULT_CMD;

    if (dcc_master_loco[loco].fn & 1) {
      // Turns on light (aux function).
      b1 ^= 0b11;
    }

    if (dcc_master_loco[loco].dir_changed) {
      // Old direction change command.
      b2 = 0b11000000;
      dcc_master_loco[loco].dir_changed = 0;
      // Up the repeat count so that it will be surely heard.
      cmd |= 0b01100000;
    } else {
      if (prio) cmd |= 0b01100000;
      if (dcc_master_loco[loco].drive_type == MARKLIN_OLD) {
        // The old marklin protocol needs all bits doubled.
        b2 |= (b2 >> 1);
        // Up the repeat count so that it will be surely heard.
        cmd |= 0b01100000;
      } else {
        if (IsTrainMovingReverse(loco)) {
          // reverse
          b2 |= 0x44;
        } else {
          b2 |= 0x10;
        }
        if (sp <= 7) {
          b2 |= 1;
        }
      }
    }
    SendMarklinPacket(cmd, b0, b1, b2);
    return;
  }

  cmd = DCC_DEFAULT_CMD;

  b0 = dcc_master_loco[loco].address & 0x7f;
  b1 = 0b01000000;

  // We assume 28 speed step. TODO: check upon drive mode.

  if (!IsTrainMovingReverse(loco))
    b1 |= 0b00100000;
  if (dcc_master_loco[loco].dir_changed) {
    dcc_master_loco[loco].dir_changed = 0;
    // Set directional E-stop speed.
    b1 |= 1;
    // Up repeat count.
    cmd |= 0b00100000;
  } else {
    if (prio) cmd |= 0b00100000;
    int sp = GetSpeedForTrain(loco);
    sp <<= 1;
    if (sp > 0) sp += 7;
    sp /= 9;
    if (sp > 0) sp += 3; // avoids 00, 01 (stop) and 10 11 (e-stop)
    if (sp > 31) sp = 31;
    b1 |= (sp >> 1) & 0x0f;
    if (sp & 1)
      b1 |= 0b00010000;
  }
  if (dcc_master_loco[loco].drive_type == DCC_14) {
    if (dcc_master_loco[loco].fn & 1) {
      b1 |= 0b00010000;
    } else {
      b1 &= ~0b00010000;
    }
  }
  SendDccPacket(cmd, 2, b0, b1, 0, 0);
}


static void SendServiceModeResponse(uint8_t code, uint8_t value) {
  uint8_t can_buf[15];  
  memcpy(can_buf + CAN_SIDH, dcc_service_mode_response + 2, 8);
  can_buf[CAN_EIDH] |= (dcc_master.service_mode_id << 2);
  ++dcc_master.service_mode_cv;
  can_buf[CAN_D3] = dcc_master.service_mode_cv >> 8;
  can_buf[CAN_D4] = dcc_master.service_mode_cv & 0xff;
  can_buf[CAN_D5] = code;
  can_buf[CAN_D6] = value;
  CANQueue_SendPacket(can_buf, (can_opts_t) (O_SKIP_TWO_BYTES | O_HOST));
  dcc_master.service_mode = 0;
  dcc_master.service_mode_state = 0;
}


/*#define  1
#define  2
#define SST_ENTER_RESET_PRE 3
#define SST_ENTER_RESET 4
#define SST_SEND_WRITE 5
#define SST_WAIT_WRITE 6
*/


static void HandleServiceMode() {
  switch (dcc_master.service_mode_state) {
    case SST_WAIT_WRITE_2: {
      // This is where most of the wait will happen.
      if (dcc_master.service_mode_had_ack) {
        dcc_master.service_mode_state = SST_ENTER_RESET;
        dcc_master.service_mode_request = SRQ_VERIFY;
      }
      if (dcc_master.service_mode_had_noack) {
        // Send back an error message.
        SendServiceModeResponse(DCC_ERR_NOACK, 0);
        return;
      }
      break;
    }
    case SST_WAIT_VERIFY_2: {
      // This is where most of the wait will happen.
      if (dcc_master.service_mode_had_ack) {
        SendServiceModeResponse(DCC_SUCCESS, dcc_master.service_mode_value);
        return;
      }
      if (dcc_master.service_mode_had_noack) {
        if (dcc_master.service_mode_retry_counter++ > DCC_SERVICE_RETRY_COUNT) {
          // Send back an error message.
          SendServiceModeResponse(DCC_ERR_NOACK, 0);
          return;
        } else {
          dcc_master.service_mode_state = SST_VERIFY;
        }
      }
      break;
    }
    case SST_WAIT_READBIT: {
      // This is where most of the wait will happen.
      if (dcc_master.service_mode_had_ack) {
        dcc_master.service_mode_retry_counter = 0;
        if (dcc_master.service_mode_next_bit & 0x8) {
          // Bit is set.
          dcc_master.service_mode_value |=
              1 << (dcc_master.service_mode_next_bit & 7);
        } else {
          dcc_master.service_mode_value &=
              ~(1 << (dcc_master.service_mode_next_bit & 7));
        }
        if ((dcc_master.service_mode_next_bit & 7) == 7) {
          // End. Go to verify.
          dcc_master.service_mode_state = SST_ENTER_RESET;
          dcc_master.service_mode_request = SRQ_VERIFY;
        } else {
          dcc_master.service_mode_next_bit =
              (dcc_master.service_mode_next_bit & 7) + 1;
          dcc_master.service_mode_state = SST_SEND_READ_BITS;
        }
      }
      if (dcc_master.service_mode_had_noack) {
        if (dcc_master.service_mode_next_bit & 8) {
          if (dcc_master.service_mode_retry_counter++ > DCC_SERVICE_RETRY_COUNT) {
            // Send back an error message.
            SendServiceModeResponse(DCC_ERR_NOACK, 0);
            return;
          } else {
            dcc_master.service_mode_next_bit &= ~8;
            dcc_master.service_mode_state = SST_SEND_READ_BITS;
          }
        } else {
          dcc_master.service_mode_next_bit |= 8;
          dcc_master.service_mode_state = SST_SEND_READ_BITS;
        }
      }
      break;
    }
  }


  if (dcc_master.free_packet_count == 0) return;

  switch (dcc_master.service_mode_state) {
    case SST_REQUEST: {
      if (dcc_master.service_mode_id != 0x3f) {
        if (dcc_master.service_mode_id >= dcc_master.num_locos) {
          SendServiceModeResponse(DCC_ERR_ID, 0);
          return;
        }
        if (!(dcc_master_loco[dcc_master.service_mode_id].drive_type & 0b100)) {
          SendServiceModeResponse(DCC_ERR_SERVICE_NOT_DCC, 0);
          return;
        }
      }
      SendDccPacket(DCC_LONG_PREAMBLE_CMD, 0, 0, 0, 0, 0);
      dcc_master.service_mode_state = SST_ENTER_IDLE;
      break;
    }
    case SST_ENTER_IDLE: {
      SendDccPacket(DCC_DEFAULT_CMD, 2, 0xff, 0x00, 0, 0);
      dcc_master.service_mode_state = SST_ENTER_RESET_PRE;
      break;
    }
    case SST_ENTER_RESET_PRE: {
      SendDccPacket(DCC_LONG_PREAMBLE_CMD, 0, 0, 0, 0, 0);
      dcc_master.service_mode_state = SST_ENTER_RESET;
      break;
    }
    case SST_ENTER_RESET: {
      SendDccPacket(DCC_SERVICE_MODE_5X_CMD, 2, 0, 0, 0, 0);
      switch (dcc_master.service_mode_request) {
        case SRQ_WRITE: {
          dcc_master.service_mode_state = SST_SEND_WRITE;
          break;
        }
        case SRQ_READ: {
          dcc_master.service_mode_state = SST_SEND_READ_BITS;
          dcc_master.service_mode_next_bit = 0;
          dcc_master.service_mode_retry_counter = 0;
          break;
        }
        case SRQ_VERIFY: {
          dcc_master.service_mode_state = SST_VERIFY; //SST_SEND_WRITE;
          dcc_master.service_mode_retry_counter = 0;
          break;
        }
      }
      break;
    }
    case SST_EXIT_RESET: {
      // Send a reset after the actions.
      SendDccPacket(DCC_SERVICE_MODE_1X_CMD, 2, 0, 0, 0, 0);
      dcc_master.service_mode_state = SST_WAIT_WRITE_2;
      break;
    }
    case SST_SEND_WRITE: {
      dcc_master.service_mode_had_ack = 0;
      dcc_master.service_mode_had_noack = 0;
      SendDccPacket(DCC_SERVICE_MODE_5X_WITH_ACK_CMD,
                    3,
                    0b01111100 | ((dcc_master.service_mode_cv >> 8) & 3),
                    dcc_master.service_mode_cv & 0xff,
                    dcc_master.service_mode_value,
                    0);
      dcc_master.service_mode_state = SST_WAIT_WRITE;
      break;
    }
    case SST_WAIT_WRITE: {
      // Send a reset after the write.
      SendDccPacket(DCC_SERVICE_MODE_1X_CMD, 2, 0, 0, 0, 0);
      dcc_master.service_mode_state = SST_WAIT_WRITE_2;
      break;
    }
    case SST_VERIFY: {
      // At this point: a long preamble was sent.
      dcc_master.service_mode_had_ack = 0;
      dcc_master.service_mode_had_noack = 0;
      SendDccPacket(DCC_SERVICE_MODE_5X_WITH_ACK_CMD,
                    3,
                     0b01110100 | ((dcc_master.service_mode_cv >> 8) & 3),
                    dcc_master.service_mode_cv & 0xff,
                    dcc_master.service_mode_value,
                    0);
      dcc_master.service_mode_state = SST_WAIT_VERIFY;
      break;
    }
    case SST_WAIT_VERIFY: {
      // Send a reset after the verify.
      SendDccPacket(DCC_SERVICE_MODE_1X_CMD, 2, 0, 0, 0, 0);
      dcc_master.service_mode_state = SST_WAIT_VERIFY_2;
      break;
    }
    case SST_SEND_READ_BITS: {
      dcc_master.service_mode_had_ack = 0;
      dcc_master.service_mode_had_noack = 0;
      // Check if bit N == 1.
      SendDccPacket(DCC_SERVICE_MODE_5X_WITH_ACK_CMD,
                    3,
                    0b01111000 | ((dcc_master.service_mode_cv >> 8) & 3),
                    dcc_master.service_mode_cv & 0xff,
                    0b11100000 | (dcc_master.service_mode_next_bit & 0xf),
                    0);
      dcc_master.service_mode_state = SST_WAIT_READ;
      break;
    }
    case SST_WAIT_READ: {
      // Send a reset after the read.
      SendDccPacket(DCC_SERVICE_MODE_1X_CMD, 2, 0, 0, 0, 0);
      dcc_master.service_mode_state = SST_WAIT_READBIT;
      break;
    }
  }
}



void DccLoop_ProcessIO() {
  uint8_t log_pkt[6];
  if (dcc_master.alive &&
      dcc_master.enabled &&
      dcc_master.service_mode) {
    log_pkt_to_host_ = CDST_HOST;
    uint8_t tmp = dcc_master.service_mode_state;
    HandleServiceMode();
    log_pkt_to_host_ = 0;
    if (tmp != dcc_master.service_mode_state) {
      log_pkt[0] = 4;
      log_pkt[1] = CMD_DCCLOG;
      log_pkt[2] = dcc_master.service_mode_value;
      log_pkt[3] = tmp;
      log_pkt[4] = dcc_master.service_mode_state;
      PacketQueue::instance()->TransmitConstPacket(log_pkt);
    }
  }
  if (dcc_master.alive &&
      dcc_master.enabled &&
      !dcc_master.service_mode &&
      dcc_master.free_packet_count > 0) {

    if (dcc_master.priority_len) {
      uint8_t id = dcc_master.priority[0].id;
      if (dcc_master_loco[id].address) {
        if (dcc_master.priority[0].what == 15) {
          SendDccLocoSpeedPacket(id, 1);
        } else {
          SendLocoFnPacket(id, dcc_master.priority[0].what, 1);
        }
      }
#ifdef __FreeRTOS__
      taskENTER_CRITICAL();
#endif
      memmove(dcc_master.priority, dcc_master.priority + 1,
              (dcc_master.priority_len - 1) * sizeof(dcc_master.priority[0]));
      --dcc_master.priority_len;
#ifdef __FreeRTOS__
      taskEXIT_CRITICAL();
#endif
      return;
    }

    for (uint8_t i = 0; i < DCC_NUM_LOCO; ++i) {
      ++dcc_master.last_loco_id;
      if (dcc_master.last_loco_id >= DCC_NUM_LOCO) {
        dcc_master.last_loco_id = 0;
      }
      if (dcc_master_loco[dcc_master.last_loco_id].address) {
#ifdef LOG_REFRESH_STATE_TO_HOST
        log_pkt[0] = 5;
        log_pkt[1] = CMD_DCCLOG;
        log_pkt[2] = dcc_master.last_loco_id;
        log_pkt[3] = dcc_master_loco[dcc_master.last_loco_id].address;
        log_pkt[4] = dcc_master_loco[dcc_master.last_loco_id].loop_at_speed;
        log_pkt[5] = dcc_master_loco[dcc_master.last_loco_id].loop_position;
        PacketQueue::instance()->TransmitConstPacket(log_pkt);
#endif
        // Send next packet for current loco.
        if (dcc_master_loco[dcc_master.last_loco_id].loop_at_speed) {
          SendDccLocoSpeedPacket(dcc_master.last_loco_id, 0);
          dcc_master_loco[dcc_master.last_loco_id].loop_at_speed = 0;
          --dcc_master.last_loco_id;  // send an accessory packet, too.
        } else {
          SendLocoFnPacket(
              dcc_master.last_loco_id,
              dcc_master_loco[dcc_master.last_loco_id].loop_position, 0);
          if (!(dcc_master_loco[dcc_master.last_loco_id].drive_type & 0b100)) {
            // Marklin. Rotate by one among fn 1..4.
            ++dcc_master_loco[dcc_master.last_loco_id].loop_position;
            if (dcc_master_loco[dcc_master.last_loco_id].loop_position > (dcc_master_loco[dcc_master.last_loco_id].fncount > 4 ? 8 : 4)) {
              dcc_master_loco[dcc_master.last_loco_id].loop_position = 1;
            }
          } else {
            // Dcc loco. Flip-flop between 1 and 5.
            dcc_master_loco[dcc_master.last_loco_id].loop_position += 4;
            if (dcc_master_loco[dcc_master.last_loco_id].loop_position > 4) {
              dcc_master_loco[dcc_master.last_loco_id].loop_position = 1;
            }
          }
          dcc_master_loco[dcc_master.last_loco_id].loop_at_speed = 1;
        }
        break;
      }
    }
  }
}


void DccLoop_Timer() {

}

void DccLoop_Init() {
  dcc_master.enabled = 1;
  uint8_t i;
  for (i = 0; i < DCC_NUM_LOCO /* const_lokdb[i].address*/; ++i) {
    dcc_master_loco[i].address = const_lokdb[i].address;
    dcc_master_loco[i].speed.raw = 0;
    uint8_t t = const_lokdb[i].mode;
    dcc_master_loco[i].drive_type = t & (PUSHPULL - 1);
    dcc_master_loco[i].is_pushpull = (t & PUSHPULL) ? 1 : 0;
    dcc_master_loco[i].loop_position = 0;
    dcc_master_loco[i].loop_at_speed = 1;
    dcc_master_loco[i].relative_speed = 16;
    // TODO(bracz): this should be 1 to automatically pause all locos at startup
    dcc_master_loco[i].paused = 0;
    dcc_master_loco[i].reversed = 0;
    dcc_master_loco[i].name = const_lokdb[i].name;
    dcc_master_loco[i].namelen = strlen(dcc_master_loco[i].name);
    for (t = 0; t < DCC_MAX_FN && const_lokdb[i].function_mapping[t] != 0xff; ++t);
    dcc_master_loco[i].fncount = t;
  }
  dcc_master.num_locos = i;
  while (i < DCC_NUM_LOCO) {
    dcc_master_loco[i].address = 0;
    ++i;
  }
  dcc_master.last_loco_id = 0;
  dcc_master.priority_len = 0;
}

void DccLoop_EmergencyStop() {
  CANQueue_SendPacket_front((uint8_t*)dcc_erstop, O_LOG_AT);
  // Makes the packet be sent off right away if possible.
  // This is not needed in FreeRTOS anymore.
  //CANQueue_ProcessIO();
  //CANProcessIO();
}

#endif
