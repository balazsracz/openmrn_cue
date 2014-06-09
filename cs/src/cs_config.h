
#ifndef _CS_CONFIG_H_
#define _CS_CONFIG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void diewith(uint32_t pattern);
void setblink(uint32_t pattern);
extern uint32_t blinker_pattern;
#ifdef __cplusplus
}
#endif

#define CS_DIE_ASSERT 0x80002C2A       // 4-1-1
#define CS_DIE_UNSUPPORTED 0x8000AC2A  // 4-1-2
#define CS_DIE_AUT_IMPORTERROR 0x8002AC2A  // 4-1-3
#define CS_DIE_AUT_HALT 0x800AAC2A  // 4-1-4
#define CS_DIE_AUT_WRITETIMERBIT 0x802AAC2A  // 4-1-5
#define CS_DIE_AUT_TWOBYTEFAIL 0x80AAAC2A  // 4-1-6

#define ASSERT( x ) do { if (!(x)) diewith(CS_DIE_ASSERT); } while (0)


// Memory size variables
#define PACKET_TX_THREAD_STACK_SIZE 512
#define PACKET_RX_THREAD_STACK_SIZE 1624
//! How many entries of pending packets to send to the USB host we should
//! maintain. Each entry costs 8 bytes.
#define PACKET_TX_QUEUE_LENGTH 10

///////////

#define DCC_CAN_THREAD_CAN_STACK_SIZE 1000
#define AUTOMATA_THREAD_STACK_SIZE 1700

#define MAX_SIGNALS 64
#define MAX_LOCK_ID 8


#define DEBOUNCE_COUNT 3
#define I2C_READ_UNCHANGED_COUNT DEBOUNCE_COUNT
#define BIT_PRODUCER_UNCHANGED_COUNT DEBOUNCE_COUNT


////////////////


#define BRACZ_NODESPACE 0x050101011400ULL
#define BRACZ_EVENTSPACE 0x0501010114000000ULL
#define BRACZ_LAYOUT 0x0501010114FF0000ULL

#ifdef TARGET_LPC1768
#define SNIFFER
#endif


#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368) || defined(TARGET_LPC11Cxx)
#define HAVE_MBED
#endif

#ifdef TARGET_LPC2368
//#define SECOND
#endif

#define STATEFLOW_CS


#endif // _CS_CONFIG_H_
