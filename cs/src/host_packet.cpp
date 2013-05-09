#include <fcntl.h>
#include <unistd.h>

#include "os/os.h"
#include "host_packet.h"



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

PacketQueue::PacketQueue(int fd) : fd_(fd) {
    tx_queue_ = os_mq_create(PACKET_TX_QUEUE_LENGTH, sizeof(PacketBase));
    os_thread_create(NULL, "host_pkt_tx", 0, PACKET_TX_THREAD_STACK_SIZE,
		     tx_thread, this);
    os_thread_create(NULL, "host_pkt_rx", 0, PACKET_RX_THREAD_STACK_SIZE,
		     rx_thread, this);
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
