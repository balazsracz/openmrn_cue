#include <fcntl.h>
#include <unistd.h>

#include "cmsis.h"

#include "os/os.h"
#include "host_packet.h"

#include "usb_proto.h"
#include "automata_control.h"
#include "extender_node.h"
#include "dcc-master.h"
#include "pic_can.h"
#include "can-queue.h"

/*

  Features that need to be copied over:

  - Timer. Call mostamaster_timer approximately 15 times per second. Set HZ to
    the frequency of calls.

  - Wii controller.

  - the lock bits are sent back to the host once every second. There is also a
    once-per second "tick" command that sends some parameters that noone looked
    at previously. It is sent back with a fake RPCLOG message.

  - Blink LEDs by usb status.

  - There is a request_emergency_stop RPC command in the assembler language. It
    sets a gllobal bit, and in the next processing loop it causes packets to be
    sent out to stop the mosta-master and DCC booster.

  - in the case of some HandlePacket I have commented out "if
    (!canISDestination(CDST_FOO)) return;" commands. Is it okay to remove these?
    Or are there can packets coming in that would cause confusion?

  - in main() there is a reset_train_map() call.
    
  - Are there cases when packets are routed within differnet components using
    the CAN broker?

 */

// Reset vector. Note that this does not go back to the bootloader.
extern "C" { void start(void); }

PacketQueue* PacketQueue::instance_ = NULL;

void PacketQueue::initialize(const char* serial_device) {
    int fd = open(serial_device, O_RDWR);
    ASSERT(fd >= 0);
    instance_ = new PacketQueue(fd);
}

void* tx_thread(void* p) {
    ((PacketQueue*)p)->TxThreadBody();
    return NULL;
}

void* rx_thread(void* p) {
    ((PacketQueue*)p)->RxThreadBody();
    return NULL;
}

const uint8_t syncpacket[] = {
    CMD_SYNC_LEN, CMD_SYNC, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

static long long sync_packet_callback(void*, void*) {
    PacketQueue::instance()->TransmitConstPacket(syncpacket);
    return OS_TIMER_RESTART; //SEC_TO_NSEC(1);
}

PacketQueue::PacketQueue(int fd) : fd_(fd) {
    tx_queue_ = os_mq_create(PACKET_TX_QUEUE_LENGTH, sizeof(PacketBase));
    os_thread_create(NULL, "host_pkt_tx", 0, PACKET_TX_THREAD_STACK_SIZE,
		     tx_thread, this);
    os_thread_create(NULL, "host_pkt_rx", 0, PACKET_RX_THREAD_STACK_SIZE,
		     rx_thread, this);
    sync_packet_timer_ = os_timer_create(&sync_packet_callback, NULL, NULL);
    os_timer_start(sync_packet_timer_, MSEC_TO_NSEC(250));
}

void PacketQueue::RxThreadBody() {
    uint8_t size;
    while(1) {
	ssize_t ret = read(fd_, &size, 1);
	ASSERT(ret == 1);
	PacketBase* pkt = new PacketBase(size);
	uint8_t* buf = pkt->buf();
	while (size) {
	    ret = read(fd_, buf, size);
	    ASSERT(ret >= 0);
	    buf += ret;
	    size -= ret;
	}
	ProcessPacket(pkt);
    }
}

const uint8_t state_cleared[] = { 2, CMD_SYSLOG, LOG_STATE_CLEARED };
const uint8_t log_illegal_argument[] = { 2, CMD_SYSLOG, LOG_ILLEGAL_ARGUMENT };
const uint8_t mod_state_success[] = {1, CMD_MOD_STATE};
const uint8_t packet_misc_invalidarg[] = { 4, CMD_UMISC, 0x00, 0xff, 0x01 };

void PacketQueue::ProcessPacket(PacketBase* pkt) {
    const PacketBase& in_pkt(*pkt);
    switch (in_pkt[0]) {
    case CMD_PING: {
	PacketBase pongpacket(2);
	pongpacket[0] = CMD_PONG;
	pongpacket[1] = in_pkt[1] + 1;
	PacketQueue::instance()->TransmitPacket(pongpacket);
	break;
    }
    case CMD_CLEAR_STATE: {
	resetrpchandler();
	PacketQueue::instance()->TransmitConstPacket(state_cleared);
	break;
    }
    case CMD_RPC: {
	PacketBase pongpacket(3);
	pongpacket[0] = CMD_RPC_RESPONSE;
	// TODO(balazs.racz): there was a custom handling for address 0 here:
	//if (in_pkt[1] == 0 && in_pkt[2] == CMD_QUERYBITS) {
        //r = clients[0].savedata[in_pkt[3]]; else genrpc()
	unsigned r = genrpc(in_pkt[1], in_pkt.buf() + 2, in_pkt.size() - 2);
	pongpacket[1] = r & 0xff;
	pongpacket[2] = r >> 8;
	PacketQueue::instance()->TransmitPacket(pongpacket);
	break;
    }
    case CMD_GET_OR_SET_ADDRESS: {
	if (in_pkt.size() > 1) {
	    if (in_pkt[1]) {
		suspend_all_automata();
	    } else {
		resume_all_automata();
	    }
	}
	PacketBase response(2);
	response[0] = CMD_CURRENT_ADDRESS;
	response[1] = automata_running() ? 0 : 255;
	PacketQueue::instance()->TransmitPacket(response);
	break;
    }
    case CMD_MOD_STATE: {
	if (in_pkt.size() != 3) {
	    PacketQueue::instance()->TransmitConstPacket(log_illegal_argument);
	    break;
	}
	*get_state_byte(in_pkt[1] >> 5, in_pkt[1] & 31) = in_pkt[2];
	PacketQueue::instance()->TransmitConstPacket(mod_state_success);
	break;
    }
    case CMD_BIT_MODIFY_STATE: {
	if (in_pkt.size() != 4) {
	    PacketQueue::instance()->TransmitConstPacket(log_illegal_argument);
	    break;
	}
	uint8_t* ptr = get_state_byte(in_pkt[1] >> 5, in_pkt[1] & 31);
	taskENTER_CRITICAL();
	*ptr |= in_pkt[2];
	*ptr &= ~in_pkt[3];
	taskEXIT_CRITICAL();
	PacketBase response(2);
	response[0] = CMD_BIT_MODIFY_STATE;
	response[1] = *ptr;
	PacketQueue::instance()->TransmitPacket(response);
	break;
    }
    case CMD_GET_STATE: {
	if (in_pkt.size() != 3 || in_pkt[2] > 8) {
	    PacketQueue::instance()->TransmitConstPacket(log_illegal_argument);
	    break;
	}
	PacketBase response(2 + in_pkt[2]);
	response[0] = CMD_STATE;
	response[1] = in_pkt[1];
	int i = 2;
	for (int arg = in_pkt[1]; arg < in_pkt[1] + in_pkt[2]; ++arg) {
	    response[i++] = *get_state_byte(arg >> 5, arg & 31);
	}
	PacketQueue::instance()->TransmitPacket(response);
	break;
    }
    case CMD_SYNC: {
	break;
    }
    case CMD_UMISC: {
	if (in_pkt.size() < 2) {
	  PacketQueue::instance()->TransmitConstPacket(packet_misc_invalidarg);
	  break;
	}
	HandleMiscPacket(*pkt);
	break;
    }
    case CMD_CAN_PKT: {
	// TODO(bracz): This memory copy could be avoided by rescaling
	// CAN_START, CAN_D0, CAN_EIDH etc. to be at offset 1 instead.
	PacketBase can_buf(in_pkt.size() + 1);
	memcpy(can_buf.buf() + CAN_START, in_pkt.buf() + 1, in_pkt.size() - 1);
	CANQueue_SendPacket(can_buf.buf(), O_SKIP_TWO_BYTES);
	os_mutex_lock(&dcc_mutex);
	DccLoop_HandlePacket(can_buf);
	DccLoop_ProcessIO();
	DccLoop_ProcessIO();
	os_mutex_unlock(&dcc_mutex);
	// Strange that mosta-master does not need this packet.
	break;
    }
    } // switch

    delete pkt;
}


void PacketQueue::HandleMiscPacket(const PacketBase& in_pkt) {
    PacketBase miscpacket(5);
    miscpacket[0] = CMD_UMISC;
    miscpacket[1] = in_pkt[1];
    miscpacket[2] = 0; // no error.
    miscpacket[3] = 0; // retval
    miscpacket[4] = 0; // retval
    switch(in_pkt[1]) {
    case CMDUM_POWERON:
    case CMDUM_POWEROFF:
    case CMDUM_GETPOWER:
    case CMDUM_MEASUREREF:
    case CMDUM_MEASURECUR:
    case CMDUM_GETCURRENT:
    case CMDUM_CURRENTSENSE_TICK_LIMIT:
    case CMDUM_CURRENTSENSE_THRESHOLD:
    case CMDUM_CURRENTSENSE_THRESHOLD_2: {
	// These commands are related to the ADC-based current sensing for the
	// AUX power district. We blindly forward them to the canbus target.
	PacketBase remote_packet(in_pkt.size() - 1);
	memcpy(remote_packet.buf(), in_pkt.buf() + 1, in_pkt.size());
	//aux_power_node->SendCommand(remote_packet);
	return;
    }
    case CMDUM_RESET: {
#ifdef TARGET_LPC2368	
	// Clears all possible interrupt sources.
	NVIC->IntEnClr = NVIC->IntEnable;
	start();
#else
#error Define how to reset your chip.
#endif
	return;
    }

    case CMDUM_SETTRAINAT: {
	if (in_pkt[2] > MAX_TRAIN_LOCATION) {
            miscpacket[2] = 0xff; // invalid argument.
            break;
	}
	train_ids[in_pkt[2]] = in_pkt[3];
	break;
    }
    case CMDUM_GETTRAINAT: {
	if (in_pkt[2] > MAX_TRAIN_LOCATION) {
            miscpacket[2] = 0xff; // invalid argument.
            break;
	}
	miscpacket[3] = in_pkt[2];
	miscpacket[4] = train_ids[in_pkt[2]];
	break;
    }
    case CMDUM_SETRELSPEED: {
	DccLoop_SetLocoRelativeSpeed(in_pkt[2], in_pkt[3]);
	//fallthrough
    }
    case CMDUM_GETRELSPEED: {
	if (DccLoop_IsUnknownLoco(in_pkt[2])) {
            miscpacket[2] = 0xff; // invalid argument.
            break;
	}
	miscpacket[3] = in_pkt[2];
	miscpacket[4] = DccLoop_GetLocoRelativeSpeed(in_pkt[2]);
	break;
    }
    case CMDUM_GET_LOCK_INFO: {
	PacketBase miscpacket(6);
	miscpacket[0] = CMD_UMISC;
	miscpacket[1] = in_pkt[1];
	miscpacket[2] = 0; // no error.
	miscpacket[3] = 0; // retval
	miscpacket[4] = 0; // retval
	
	miscpacket[3] = *get_state_byte(0, OFS_GLOBAL_BITS);
	miscpacket[4] = locks[in_pkt[2]] >> 8;
	miscpacket[5] = locks[in_pkt[2]] & 0xff;
	PacketQueue::instance()->TransmitPacket(miscpacket);
	return;
    }
    case CMDUM_REL_ALL_LOCK: {
	*get_state_byte(0, OFS_GLOBAL_BITS) = 0;
	break;
    }
    } // switch packet cmd
    PacketQueue::instance()->TransmitPacket(miscpacket);
}

void PacketQueue::TxThreadBody() {
    while(1) {
	PacketBase pkt;
	os_mq_receive(tx_queue_, &pkt);
	uint8_t s = pkt.size();
	ssize_t res = write(fd_, &s, 1);
	ASSERT(res == 1);
	uint8_t* buf = pkt.buf();
	size_t size = pkt.size();
	while (size) {
	    res = write(fd_, buf, size);
	    ASSERT(res >= 0);
	    size -= res;
	    buf += res;
	}
    }
}

void PacketQueue::TransmitPacket(PacketBase& packet) {
    os_mq_send(tx_queue_, &packet);
    packet.release(); // The memory is now owned by the queue.
}

void PacketQueue::TransmitConstPacket(const uint8_t* packet) {
    PacketBase pkt(packet[0]);
    memcpy(pkt.buf(), &packet[1], pkt.size());
    TransmitPacket(pkt);
}

