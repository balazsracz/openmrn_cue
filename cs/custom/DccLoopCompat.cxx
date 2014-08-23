
/** \copyright
 * Copyright (c) 2014, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file DccLoopCompat.hxx
 *
 * Compatibility layer from the plain-C interface of the dcc command station.
 *
 * @author Balazs Racz
 * @date 14 May 2014
 */


#include "src/dcc-master.h"
#include "utils/macros.h"

extern "C" {
  void dcc_ignore_fn(void) {}
void crash_fn(void) {
  DIE("Unexpected call to unimplemented function.");
}
}

void DccLoop_Init(void) __attribute__((alias("dcc_ignore_fn")));
void DccLoop_HandlePacket(const PacketBase& can_buf) __attribute__((alias("crash_fn")));
void DccLoop_ProcessIO(void) __attribute__((alias("crash_fn")));
void DccLoop_Timer(void) __attribute__((alias("crash_fn")));

void DccLoop_EmergencyStop(void) {
  // TODO
}

// Returns non-zero if we are powering the track.
uint8_t DccLoop_HasPower(void) {
  // TODO
  return 1;
}

uint8_t DccLoop_SetLocoPaused(uint8_t id, uint8_t is_paused)__attribute__((alias("crash_fn")));
void DccLoop_SetLocoRelativeSpeed(uint8_t id, uint8_t rel_speed)__attribute__((alias("crash_fn")));
uint8_t DccLoop_GetLocoRelativeSpeed(uint8_t id)__attribute__((alias("crash_fn")));
// abs_speed is the raw speed (bit 7 is reversed bit, bit 0-6 is speed step).
void DccLoop_SetLocoAbsoluteSpeed(uint8_t id, uint8_t abs_speed)__attribute__((alias("crash_fn")));

uint8_t DccLoop_IsUnknownLoco(uint8_t id)__attribute__((alias("crash_fn")));

// Returns non-zero if loco is reversed.
uint8_t DccLoop_IsLocoReversed(uint8_t id)__attribute__((alias("crash_fn")));

// Sets a given loco's is_reversed state.
void DccLoop_SetLocoReversed(uint8_t id, uint8_t is_reversed)__attribute__((alias("crash_fn")));

// Returns non-zero if loco is available for push-pull operation.
uint8_t DccLoop_IsLocoPushPull(uint8_t id)__attribute__((alias("crash_fn")));

// Sets a function button to a given value.
// fn=0 is light. fn=1,2,3 is F1,F2,F3. (Corresponds to the canbus protocol.)
uint8_t DccLoop_SetLocoFn(uint8_t id, uint8_t fn, uint8_t value)__attribute__((alias("crash_fn")));
