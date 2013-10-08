#ifndef _BRACZ_TRAIN_AUTOMATA_CONTROL_H
#define _BRACZ_TRAIN_AUTOMATA_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define MAX_TRAIN_LOCATION 32

//! Clears all state of all automatas.
void resetrpchandler();

//! Simulates an RPC going to an old version target controller.
uint16_t genrpc(uint8_t address, const uint8_t* data, uint8_t size);

//! Stops processing all automatas.
void suspend_all_automata();
//! Restarts processing automatas.
void resume_all_automata();
//! Returns true if automata processing is enabled.
bool automata_running();
//! Clears all state bytes.
void reset_all_state(void);

//! Returns a state byte from a particular client and offset. Offset is
//! interpreted in the old system.
// NOTE(bracz): this sees to be a special case for locks?
//	    if (arg == ((comms_get_address()) << 5) + OFS_GLOBAL_BITS) {
//          statepacket[i+3] = global_bits[comms_get_address()];

uint8_t* get_state_byte(int client, int offset);

// Array holding the mapping of train location -> train ID.
extern uint8_t train_ids[];

// Holds owner information for automata mutexes.
extern volatile uint16_t locks[];

// Holds the aspects of the signals.
extern uint8_t signal_aspects[];
inline void set_signal_aspect(int offset, uint8_t aspect) {
    // TODO(bracz) send off this value in an event or what.
    signal_aspects[offset] = aspect;
}
inline uint8_t get_signal_aspect(int offset) {
    return signal_aspects[offset];
}

#ifdef __cplusplus
}
#endif

#endif //_BRACZ_TRAIN_AUTOMATA_CONTROL_H


