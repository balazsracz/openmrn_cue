#include <unistd.h>

#include "cs_config.h"

#include "can-queue.h"
#include "can.h"
#include "nmranet_can.h"
#include "host_packet.h"
#include "usb_proto.h"

extern int mostacan_fd;

uint8_t CANQueue_eidh_mask;

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
