#include <fcntl.h>
#include <unistd.h>

#include "os/os.h"
#include "host_packet.h"

#include "usb_proto.h"

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
  CMD_SYNC, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

static long long sync_packet_callback(void*, void*) {
    PacketBase pkt(CMD_SYNC_LEN);
    memcpy(pkt.buf(), syncpacket, pkt.size());
    PacketQueue::instance()->TransmitPacket(pkt);
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

void PacketQueue::ProcessPacket(PacketBase* pkt) {
    const PacketBase& in_pkt(*pkt);
    switch (in_pkt[0]) {
    case CMD_PING: {
	PacketBase pongpacket(2);
	pongpacket[0] = CMD_PONG;
	pongpacket[1] = in_pkt[1] + 1;
	PacketQueue::instance()->TransmitPacket(pongpacket);
	delete pkt;
	return;
    }
    }
    delete pkt;
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
