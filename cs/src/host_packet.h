
#ifndef _HOST_PACKET_H_
#define _HOST_PACKET_H_

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "os/os.h"

#include "cs_config.h"

class PacketBase {
public:
    PacketBase()
	: size_(0),
	  data_(NULL) {}

    explicit PacketBase(int size)
	: size_(size),
	  data_(static_cast<uint8_t*>(malloc(size))) {
	ASSERT(data_);
    }

    ~PacketBase() {
	free(data_);
    }

    uint8_t& operator[](size_t offset) {
	ASSERT(offset < size_);
	ASSERT(data_);
	return data_[offset];
    }

    uint8_t operator[] (size_t offset) const {
	ASSERT(offset < size_);
	ASSERT(data_);
	return data_[offset];
    }

    uint8_t* buf() {
	ASSERT(data_);
	return data_;
    }

    const uint8_t* buf() const {
	ASSERT(data_);
	return data_;
    }

    size_t size() const {
	return size_;
    }

    void release() {
	data_ = NULL;
    }

protected:
    size_t size_;
    uint8_t* data_;
};

template<class T> class TypedPacket : public PacketBase {
public:
    TypedPacket() : PacketBase(sizeof(T)) {}

    const T& get() const { return *static_cast<T*>(data_); }

    T* get_mutable() { return static_cast<T*>(data_); }
};


class PacketQueue {
public:
    //! Returns the static instance of PacketQueue. Dies if packet queue is not running.
    static PacketQueue* instance() {
	ASSERT(instance_);
	return instance_;
    }

    /// Initializes the static instance of PacketQueue. Must be called before any call to instance().
    /// @param serial_device is the filepath of the device to communicate packets through.
    static void initialize(const char* serial_device);

    //! Takes ownership of packet. Deletes it when done.
    void TransmitPacket(PacketBase* packet) {
	TransmitPacket(*packet);
	delete packet;
    }
    //! Makes packet empty when done.
    void TransmitPacket(PacketBase& packet);

    //! Transmits a packet from const memory. The first byte of the packet is
    //! the length, and that many following byte will be transmitted.
    void TransmitConstPacket(const uint8_t *packet);

    void TransmitPacketFromISR(PacketBase* packet) {
	diewith(CS_DIE_UNSUPPORTED);
    }


private:
    //! Static instance returned by instance(). Non-NULL if packet queue is
    //! running.
    static PacketQueue* instance_;

    PacketQueue(int fd);
    ~PacketQueue();

    //! Received packet handler thread body.
    void RxThreadBody();
    //! Transmit packet handler thread.
    void TxThreadBody();

    //! Called on every incoming packet. Takes ownership of pkt.
    void ProcessPacket(PacketBase* pkt);
    //! Handles incoming CMD_UMISC packets.
    void HandleMiscPacket(const PacketBase& in_pkt);

    friend void* tx_thread(void* p);
    friend void* rx_thread(void* p);

    //! Device to read/write packets from.
    int fd_;

    //! Packets waiting for transmission to the host.
    os_mq_t tx_queue_;

    os_timer_t sync_packet_timer_;
};

#endif // _HOST_PACKET_H_
