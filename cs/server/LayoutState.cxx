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

#include "utils/logging.h"
#include "openlcb/Defs.hxx"
#include "os/os.h"

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

void LayoutState::PopulateLokState(int id, TrainControlResponse* resp) {
  LayoutState* db = this;
  LayoutState::loks_t::const_iterator it = db->loks.find(id);
  if (it == db->loks.end()) {
    LOG(WARNING, "Requested state of lok %d  which does not exist.", id);
    return;
  }
  LokStateProto* lok = resp->add_lokstate();
  const LokState& lst = *it->second;
  lok->set_id(id);
  lok->set_dir(lst.speed_and_dir.dir);
  lok->set_speed(lst.speed_and_dir.speed);
  lok->set_speed_ts(lst.speed_and_dir.ts_usec);
  lok->set_ts(lst.ts_usec);
  for (LokState::fn_t::const_iterator it = lst.fn.begin();
       it != lst.fn.end();
       ++it) {
    LokStateProto_Function* fn = lok->add_function();
    fn->set_id(it->first);
    fn->set_value(it->second->value);
    fn->set_ts(it->second->ts_usec);
  }
}

void LayoutState::PopulateAllLokState(TrainControlResponse* resp) {
  LayoutState* db = this;
  for (LayoutState::loks_t::const_iterator it = db->loks.begin();
       it != db->loks.end();
       ++it) {
    PopulateLokState(it->first, resp);
  }
}

void LayoutState::ZeroLayoutState(const TrainControlResponse_LokDb& lokdb) {
  for (const auto& lok : lokdb.lok()) {
    LokState*& st = loks[lok.id()];
    if (!st) st = new LokState;
    for (const auto& fn : lok.function()) {
      st->fn[fn.id()] = new FnState;
    }
  }
}


}  // namespace server
