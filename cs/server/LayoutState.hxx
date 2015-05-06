/** \copyright
 * Copyright (c) 2015, Balazs Racz
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
 * \file LayoutState.hxx
 *
 * Maintains the state of the entire layout (all locos) with change-listening
 * functionality.
 *
 * @author Balazs Racz
 * @date 5 May 2015
 */

#ifndef _SERVER_LAYOUTSTATE_HXX_
#define _SERVER_LAYOUTSTATE_HXX_

#include <stdint.h>
#include <functional>

#include "utils/Singleton.hxx"
#include "utils/Atomic.hxx"

namespace server {

using Closure = std::function<void()>;

struct TimestampedState : private Atomic {
  TimestampedState()
      : ts_usec(0) {}

  // Last modified.
  uint64_t ts_usec;
  // Listeners. They are all self-owning callbacks or externally owned
  // callbacks. Only called at the next change in state.
  vector<Closure> listeners;

  // Updates timestamp (if advanced) and calls any listener if needed.
  void Touch(uint64_t new_ts);

  // Calls done closure when ts_usec > last_timestamp. Might call done
  // synchronously.
  void AddListener(uint64_t last_timestamp, Closure done);

 private:
  DISALLOW_COPY_AND_ASSIGN(TimestampedState);
};

struct FnState : TimestampedState {
  FnState()
      : value(0) {}
  // 0: off, 1: on
  int value;
};

struct SpeedAndDirState : TimestampedState {
  SpeedAndDirState()
      : dir(1), speed(0) {}
  // +1: forward, -1: backward.
  int dir;
  // 0..127
  int speed;
};

struct LokState : TimestampedState {
  typedef map<int, FnState*> fn_t;
  // function id -> value mapping.
  fn_t fn;
  SpeedAndDirState speed_and_dir;

  const FnState* GetFn(int fn_id) const {
    fn_t::const_iterator it = fn.find(fn_id);
    if (it == fn.end()) return NULL;
    return it->second;
  }
};

struct LayoutState : TimestampedState, public Singleton<LayoutState> {
  LayoutState()
      : stop(true) {}
  typedef map<int, LokState*> loks_t;
  loks_t loks;
  // true: emergency stopped. false: power on.
  bool stop;

  // Returns the lok entry for that id, or null if it does not exist.
  LokState* GetLok(int id) const {
    loks_t::const_iterator it = loks.find(id);
    if (it == loks.end()) return NULL;
    return it->second;
  }
};

} // namespace server

#endif // _SERVER_LAYOUTSTATE_HXX_
