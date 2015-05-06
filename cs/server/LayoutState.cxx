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

#include "server/LayoutState.hxx"

namespace server {
void TimestampedState::Touch(uint64_t new_ts) {
  vector<Closure> listeners_to_call;
  {
    AtomicHolder l(this);
    if (new_ts > ts_usec) {
      ts_usec = new_ts;
      listeners_to_call.swap(listeners);
    }
  }
  for (auto& l : listeners_to_call) {
    l();
  }
}

void TimestampedState::AddListener(uint64_t last_timestamp, Closure done) {
  bool call = false;
  {
    AtomicHolder l(this);
    if (ts_usec <= last_timestamp) {
      listeners.push_back(done);
    } else {
      call = true;
    }
  }
  if (call) {
    done();
  }
}

}  // namespace server
