// Copyright 2010 Google Inc. All Rights Reserved.
// Author: bracz@google.com (Balazs Racz)
//
// Routines used to serve as a slave for a Marklin Mobile Station acting as a
// master.

#ifndef PICV2_DCC_MASTER_H_
#define PICV2_DCC_MASTER_H_

#include <stdint.h>

#include "base.h"

#define DCC_NUM_LOCO 20

class PacketBase;

void DccLoop_Init(void);
void DccLoop_HandlePacket(const PacketBase& can_buf);
void DccLoop_ProcessIO(void);
void DccLoop_Timer(void);
void DccLoop_EmergencyStop(void);
// Returns non-zero if we are powering the track.
uint8_t DccLoop_HasPower(void);


// Pauses/resumes a loco: puts the current speed aside and sets the speed to
// zero.  Returns 0 if successful. id goes form 0 to DCC_NUM_LOCO - 1.  Can be
// called from an interrupt. If is_paused is non_zero then pause,s otherwise
// resumes.
uint8_t DccLoop_SetLocoPaused(uint8_t id, uint8_t is_paused);

// Changed the speed of a loco relative to the set speed. Useful for
// programmatic slowdown - speedup of trains. Relative speed is interpreted as
// a fixed-point number with 4 bits of fractional and 4 bits of integer
// part. (i.e. rel_speed / 16). The default relative speed is therefore 16
// (=1.0)
void DccLoop_SetLocoRelativeSpeed(uint8_t id, uint8_t rel_speed);
uint8_t DccLoop_GetLocoRelativeSpeed(uint8_t id);
// abs_speed is the raw speed (bit 7 is reversed bit, bit 0-6 is speed step).
void DccLoop_SetLocoAbsoluteSpeed(uint8_t id, uint8_t abs_speed);

// Checks if a loco exists.
// Returns non-zero if loco is unknown.
// Can be called from an interrupt.
uint8_t DccLoop_IsUnknownLoco(uint8_t id);

// Returns non-zero if loco is reversed.
uint8_t DccLoop_IsLocoReversed(uint8_t id);

// Sets a given loco's is_reversed state.
void DccLoop_SetLocoReversed(uint8_t id, uint8_t is_reversed);

// Returns non-zero if loco is available for push-pull operation.
uint8_t DccLoop_IsLocoPushPull(uint8_t id);

// Sets a function button to a given value.
// fn=0 is light. fn=1,2,3 is F1,F2,F3. (Corresponds to the canbus protocol.)
uint8_t DccLoop_SetLocoFn(uint8_t id, uint8_t fn, uint8_t value);


#endif  // PICV2_MOSTA_MASTER_H_
