#ifdef TARGET_LPC2368
#include "cmsis.h"
#endif

#include "src/cs_config.h"

#define LOGLEVEL WARNING
#include "utils/logging.h"

#include <fcntl.h>
#include <unistd.h>


#include "host_packet.h"


#include "os/os.h"
#include "utils/logging.h"
#include "utils/GridConnectHub.hxx"
#include "executor/Service.hxx"
#include "utils/Hub.hxx"
#include "freertos/can_ioctl.h"

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

  - There are some mutex-unprotected calls into the dcc master from the USB
    packet loop thread.

 */

//Executor<1> vcom_executor("vcom_thread", 0, 900);
//Service vcom_service(&vcom_executor);

//! Class to handle data that comes from the virtual COM pipe and is to be sent
//! on to the USB handler.
class VCOMPipeMember : public HubPort {
public:
  VCOMPipeMember(Service* service, int packet_id) : HubPort(service), packet_id_(packet_id) {}

    virtual Action entry() {
      int count = message()->data()->size();
      PacketBase out_pkt(count + 1);
      out_pkt[0] = packet_id_;
      memcpy(out_pkt.buf() + 1, message()->data()->data(), count);
      PacketQueue::instance()->TransmitPacket(out_pkt);
      return release_and_exit();
    }

private:
    int packet_id_;
};


// Reset vector. Note that this does not go back to the bootloader.
extern "C" { void start(void); }

PacketQueue* PacketQueue::instance_ = NULL;

void PacketQueue::initialize(CanHubFlow* openlcb_can, const char* serial_device, bool force_sync) {
  instance_ = new DefaultPacketQueue(openlcb_can, serial_device, force_sync);
}

void* rx_thread(void* p) {
    ((DefaultPacketQueue*)p)->RxThreadBody();
    return NULL;
}

const uint8_t syncpacket[] = {
    CMD_SYNC_LEN, CMD_SYNC, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

typedef Buffer<PacketBase> PacketQEntry;

class DefaultPacketQueue::TxFlow : public StateFlowBase {
 public:
  TxFlow(DefaultPacketQueue* s)
      : StateFlowBase(s) {
    start_flow(STATE(send_sync_packet));
  }

  DefaultPacketQueue* service() {
    return static_cast<DefaultPacketQueue*>(StateFlowBase::service());
  }

  Action send_sync_packet() {
    if (service()->synced()) {
      return call_immediately(STATE(get_next_packet));
    }
    return write_repeated(&selectHelper_, service()->fd(), syncpacket,
                          sizeof(syncpacket), STATE(send_sync_packet), 0);
  }

  Action get_next_packet() {
    return allocate_and_call(STATE(send_next_packet),
                             service()->outgoing_packet_queue());
  }

  Action send_next_packet() {
    cast_allocation_result(&currentPacket_);
    sizeBuf_ = currentPacket_->data()->size();
    return write_repeated(&selectHelper_, service()->fd(), &sizeBuf_, 1,
                          STATE(send_packet_data), 0);
  }

  Action send_packet_data() {
    HASSERT(!selectHelper_.remaining_);
    return write_repeated(
        &selectHelper_, service()->fd(), currentPacket_->data()->buf(),
        currentPacket_->data()->size(), STATE(send_finished), 0);
  }

  Action send_finished() {
    HASSERT(!selectHelper_.remaining_);
    currentPacket_->unref();
    return call_immediately(STATE(get_next_packet));
  }

 private:
  PacketQEntry* currentPacket_;
  StateFlowSelectHelper selectHelper_{this};
  //! 1-byte buffer for writing packet length.
  uint8_t sizeBuf_;
};

DefaultPacketQueue::DefaultPacketQueue(CanHubFlow* openlcb_can, const char* dev, bool force_sync) : Service(openlcb_can->service()->executor()), synced_(false), usb_vcom_pipe0_(this) {
    async_fd_ = open(dev, O_RDWR | O_NONBLOCK);
    sync_fd_ = open(dev, O_RDWR);
    if (force_sync) ForceInitialSync();
    os_thread_create(NULL, "host_pkt_rx", 3, PACKET_RX_THREAD_STACK_SIZE,
		     rx_thread, this);
    // Wires up packet receive from vcom0 to the USB host.
    usb_vcom0_recv_ = new VCOMPipeMember(this, CMD_VCOM0);
    usb_vcom_pipe0_.register_port(usb_vcom0_recv_);
    gc_adapter_ = GCAdapterBase::CreateGridConnectAdapter(&usb_vcom_pipe0_, openlcb_can, false);
    tx_flow_ = new TxFlow(this);
}

DefaultPacketQueue::~DefaultPacketQueue() {
    delete tx_flow_;
    delete gc_adapter_;
    usb_vcom_pipe0_.unregister_port(usb_vcom0_recv_);
    delete usb_vcom0_recv_;
    // There is no way to destroy an os_mq_t.
    // There is no way to stop a thread.
    abort();
}

void DefaultPacketQueue::ForceInitialSync() {
  int send_ofs = 0;
  char receive[sizeof(syncpacket)];
  int receive_ofs = 0;
  while (true) {
    // try receive one byte
    if (receive_ofs == 0) {
      int res = ::read(async_fd_, receive, 1);
      if (res > 0 && receive[0] == 15) {
        receive_ofs = 1;
      }
    }
    // try receive the rest of the sync packet.
    if (receive_ofs > 0) {
      int res = ::read(async_fd_, receive + receive_ofs, sizeof(receive) - receive_ofs);
      receive_ofs += res;
      if (receive_ofs == sizeof(syncpacket)) {
        if (0 == memcmp(receive, syncpacket, sizeof(syncpacket))) {
          synced_ = true;
          return;
        }
        receive_ofs = 0;
      }
    }
    // try send sync packet
    int res = ::write(async_fd_, syncpacket, sizeof(syncpacket));
    if (res > 0 && res < (int)sizeof(syncpacket)) {
      send_ofs += res;
      // if we managed to send anything, we'll send the rest of the packet.
      res = ::write(sync_fd_, syncpacket + send_ofs,
                    sizeof(syncpacket) - send_ofs);
      send_ofs += res;
      HASSERT(send_ofs == sizeof(syncpacket));
      send_ofs = 0;
    }
  }
}

void DefaultPacketQueue::RxThreadBody() {
    uint8_t size;
    while(1) {
	ssize_t ret = read(sync_fd_, &size, 1);
	ASSERT(ret == 1);
        if (!synced_ && size != 15) continue;
	PacketBase* pkt = new PacketBase(size);
	uint8_t* buf = pkt->buf();
	while (size) {
	    ret = read(sync_fd_, buf, size);
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

void DefaultPacketQueue::ProcessPacket(PacketBase* pkt) {
    const PacketBase& in_pkt(*pkt);
    if (in_pkt.size() == syncpacket[0] && in_pkt[0] == syncpacket[1]) {
      // We do not do detailed logging for the sync packets.
      if (0 == memcmp(in_pkt.buf(), &syncpacket[1], in_pkt.size())) {
        synced_ = true;
      }
    } else {
      if (!synced_) {
        delete pkt;
        return;
      }
      LOG(INFO, "usb packet len %lu, cmd %02x, data %02x, %02x, %02x ...",
          (unsigned long)in_pkt.size(), in_pkt[0], in_pkt.buf()[1], in_pkt.buf()[2], in_pkt.buf()[3]);
    }
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
#ifdef __FreeRTOS__
	taskENTER_CRITICAL();
#endif
	*ptr |= in_pkt[2];
	*ptr &= ~in_pkt[3];
#ifdef __FreeRTOS__
	taskEXIT_CRITICAL();
#endif
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
#ifdef STATEFLOW_CS
        bracz_custom::handle_can_packet_from_host(in_pkt.buf() + 1, in_pkt.size() - 1);
#else
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
#endif
	// Strange that mosta-master does not need this packet.
	break;
    }
    case CMD_VCOM0: {
      auto* b = usb_vcom_pipe0_.alloc();
      b->data()->assign((const char*)(in_pkt.buf() + 1), in_pkt.size() - 1);
      b->data()->skipMember_ = usb_vcom0_recv_;
      usb_vcom_pipe0_.send(b);
      break;
    }
    } // switch

    delete pkt;
}


void DefaultPacketQueue::HandleMiscPacket(const PacketBase& in_pkt) {
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
#elif defined(TARGET_LPC11Cxx)
        // TODO(bracz): define how to reset a Cortex-M0.
        abort();
#elif defined(GCC_ARMCM3)
        // TODO(bracz): define how to reset a Cortex-M3.
        abort();
#elif defined(TARGET_PIC32MX)
        // TODO(bracz): define how to reset a PIC32MX.  In theory jumping to
        // 0xbd000000 should do the trick, but disabling all peripherial
        // interrupts might be a necessity there.
        abort();
#elif !defined(__FreeRTOS__)
        //We ignore the reset command on a host.
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


void DefaultPacketQueue::TransmitPacket(PacketBase& packet) {
  Buffer<PacketBase>* b;
  mainBufferPool->alloc(&b, nullptr);
  *b->data() = packet;
  packet.release(); // The memory is now owned by the buffer<pkt>.
  outgoing_packet_queue_.insert(b);
}

void PacketQueue::TransmitConstPacket(const uint8_t* packet) {
    PacketBase pkt(packet[0]);
    memcpy(pkt.buf(), &packet[1], pkt.size());
    TransmitPacket(pkt);
}
