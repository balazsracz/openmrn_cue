#include <string.h>

#include "cs_config.h"
#include "automata_control.h"

void resetrpchandler() {}

//! Simulates an RPC going to an old version target controller.
uint16_t genrpc(uint8_t address, const uint8_t* data, uint8_t size) {
    return 0xf000;
}

bool automata_on_ = true;

//! Stops processing all automatas.
void suspend_all_automata() { automata_on_ = false; }
//! Restarts processing automatas.
void resume_all_automata() { automata_on_ = true; }
//! Returns true if automata processing is enabled.
bool automata_running() { return automata_on_; }

#ifdef TARGET_LPC11Cxx
uint8_t clients[3][8];
#else
uint8_t clients[8][32];
#endif


//! Returns a state byte from a particular client and offset. Offset is
//! interpreted in the old system.
// NOTE(bracz): this seems to be a special case for locks?
//	    if (arg == ((comms_get_address()) << 5) + OFS_GLOBAL_BITS) {
//          statepacket[i+3] = global_bits[comms_get_address()];
uint8_t* get_state_byte(int client, int offset) {
#ifdef TARGET_LPC11Cxx
    return &(clients[client][offset - 24]);
#else
    return &(clients[client][offset]);
#endif
}

void reset_all_state(void) {
  memset(clients, 0, sizeof(clients));
}

// Array holding the mapping of train location -> train ID.
uint8_t train_ids[MAX_TRAIN_LOCATION];

// Holds owner information for automata mutexes.
volatile uint16_t locks[MAX_LOCK_ID];


uint8_t signal_aspects[MAX_SIGNALS];
