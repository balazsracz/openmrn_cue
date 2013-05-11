#ifndef _BRACZ_TRAIN_AUTOMATA_CONTROL_H
#define _BRACZ_TRAIN_AUTOMATA_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

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

//! Returns a state byte from a particular client and offset. Offset is
//! interpreted in the old system.
// NOTE(bracz): this sees to be a special case for locks?
//	    if (arg == ((comms_get_address()) << 5) + OFS_GLOBAL_BITS) {
//          statepacket[i+3] = global_bits[comms_get_address()];

uint8_t* get_state_byte(int client, int offset);

#ifdef __cplusplus
}
#endif

#endif //_BRACZ_TRAIN_AUTOMATA_CONTROL_H


