#include <unistd.h>

#include "cs_config.h"

#include "os/os.h"

#include "can-queue.h"
//#include "can.h"
#include "nmranet_can.h"
#include "host_packet.h"
#include "usb_proto.h"
#include "mosta-master.h"
#include "dcc-master.h"
#include "pic_can.h"

//! File descriptor handling the CAN interface towards the DCC/MoSta CAN.
static int mostacan_fd;

//! Used by certain commands in the Mobile Station client code to override
//! certain bits in the CAN header frame whilw reading a const progmem packet.
uint8_t CANQueue_eidh_mask;

//! This mutex protects the calls into the Dcc and mobile station code.
os_mutex_t dcc_mutex;


void CANQueue_SendPacket_back(const uint8_t* packet, can_opts_t opts) {
    struct can_frame frame = {0,};
    const uint8_t* data = packet;
    if (opts & (O_SKIP_TWO_BYTES)) data += 2;
    bool extended = (data[1] & 0x08);
    
    CLR_CAN_FRAME_RTR(frame);
    CLR_CAN_FRAME_ERR(frame);
    if (extended) {
	uint32_t id = data[0];
	id <<= 3;
	id |= data[1] >> 5;
	id <<= 2;
	id |= data[1] & 3;
	id <<= 8;
	id |= data[2];
	if (opts & O_MARK) {
	    id |= CANQueue_eidh_mask;
	}
	id <<= 8;
	id |= data[3];
	SET_CAN_FRAME_EFF(frame);
	SET_CAN_FRAME_ID_EFF(frame, id);
    } else {
	uint32_t id = data[0];
	id <<= 3;
	id |= data[1] >> 5;
	CLR_CAN_FRAME_EFF(frame);
	SET_CAN_FRAME_ID(frame, id);
    }
    int len = data[4];
    frame.can_dlc = len;
    memcpy(frame.data, data + 5, len);
    int res = write(mostacan_fd, &frame, sizeof(frame));
    ASSERT(res == sizeof(frame));

    if (opts & O_HOST) {
	PacketBase pkt(len + 5 + 1);
	pkt[0] = CMD_CAN_PKT;
	memcpy(pkt.buf() + 1, data, 5+len);
	if (opts & O_MARK) {
	    pkt[3] |= CANQueue_eidh_mask;
	}
	PacketQueue::instance()->TransmitPacket(pkt);
    }
    if (opts & O_LOG_BEFORE) {
	data -= 2;
	PacketQueue::instance()->TransmitConstPacket(data);
    }
}


void can_frame_to_mcp_buffer(const struct can_frame& frame, uint8_t* pkt) {
    pkt[CAN_LEN] = frame.can_dlc;
    memcpy(&pkt[CAN_D0], frame.data, pkt[CAN_LEN]);
    if (IS_CAN_FRAME_EFF(frame)) {
	uint32_t id = GET_CAN_FRAME_ID_EFF(frame);
	pkt[CAN_EIDL] = id & 0xff;
	id >>= 8;
	pkt[CAN_EIDH] = id & 0xff;
	id >>= 8;
	pkt[CAN_SIDL] = 0x08;
	pkt[CAN_SIDL] |= id & 3;
	id >>= 2;
	pkt[CAN_SIDL] |= (id & 7) << 5;
	id >>= 3;
	pkt[CAN_SIDH] = id & 0xff;
    } else {
	uint32_t id = GET_CAN_FRAME_ID(frame);
	pkt[CAN_SIDH] = id >> 3;
	pkt[CAN_SIDL] = id << 5;
    }
    pkt[0] = pkt[CAN_LEN] + 5 + 1;
    pkt[1] = CMD_CAN_PKT;
}


static uint8_t can_destination = 0;
static uint8_t can_pending = 0;


void CANSetDoNotLogToHost(void) {
    can_destination &= ~CDST_HOST;
}
void CANSetPending(uint8_t destination) {
    can_pending |= destination;
}
void CANSetNotPending(uint8_t destination) {
    can_pending &= ~destination;
}

void* dcc_can_thread(void*) {
    PacketBase pkt(15);
    while(1) {
	struct can_frame frame;
	ssize_t ret = read(mostacan_fd, &frame, sizeof(frame));
	ASSERT(ret == sizeof(frame));
	if (IS_CAN_FRAME_ERR(frame)) continue;
	if (IS_CAN_FRAME_RTR(frame)) continue;
	// We reconstruct an MCP2515-compatible packet from what was received.
	can_frame_to_mcp_buffer(frame, pkt.buf());
	os_mutex_lock(&dcc_mutex);
	can_destination = DST_CAN_INCOMING;
	MoStaMaster_HandlePacket(pkt);
	DccLoop_HandlePacket(pkt);
	DccLoop_ProcessIO();
	DccLoop_ProcessIO();
	bool send_to_host = can_destination & CDST_HOST;
	ASSERT(can_pending == 0);
	os_mutex_unlock(&dcc_mutex);
	if (send_to_host) {
	    PacketQueue::instance()->TransmitConstPacket(pkt.buf());
	}
    }
    return NULL;
}


static os_period_t dcc_timer_callback(void*, void*) {
    os_mutex_lock(&dcc_mutex);
    DccLoop_Timer();
    MoStaMaster_Timer();
    DccLoop_ProcessIO();
    DccLoop_ProcessIO();
    os_mutex_unlock(&dcc_mutex);
    return OS_TIMER_RESTART; //SEC_TO_NSEC(1);
}

static os_timer_t dcc_timer;

void dcc_can_init(int devfd) {
    os_mutex_init(&dcc_mutex);
    mostacan_fd = devfd;
    DccLoop_Init();
    dcc_timer = os_timer_create(&dcc_timer_callback, NULL, NULL);
    os_timer_start(dcc_timer, MSEC_TO_PERIOD(100));
    os_thread_create(NULL, "dcc_can_rx", 0, DCC_CAN_THREAD_CAN_STACK_SIZE,
		     dcc_can_thread, NULL);
}
