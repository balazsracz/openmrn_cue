
#ifndef _CS_CONFIG_H_
#define _CS_CONFIG_H_

extern "C" {
void diewith(uint32_t pattern);
void setblink(uint32_t pattern);
extern uint32_t blinker_pattern;
}

#define CS_DIE_ASSERT 0x80002C2A
#define CS_DIE_UNSUPPORTED 0x8000AC2A

#define ASSERT( x ) do { if (!(x)) diewith(CS_DIE_ASSERT); } while (0)


// Memory size variables
#define PACKET_TX_THREAD_STACK_SIZE 256
#define PACKET_RX_THREAD_STACK_SIZE 1024
//! How many entries of pending packets to send to the USB host we should
//! maintain. Each entry costs 8 bytes.
#define PACKET_TX_QUEUE_LENGTH 10


#endif // _CS_CONFIG_H_
