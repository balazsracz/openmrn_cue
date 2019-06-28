/** \copyright
 * Copyright (c) 2019, Balazs Racz
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
 * \file OlcbBindingsImpl.hxx
 *
 * Internal implementation classes of external variables for OpenLCB bindings
 * purposes.
 *
 * @author Balazs Racz
 * @date 25 June 2019
 */

#ifndef _LOGIC_OLCBBINDINGSIMPL_HXX_
#define _LOGIC_OLCBBINDINGSIMPL_HXX_

#include "logic/OlcbBindings.hxx"

#include "openlcb/EventHandlerTemplates.hxx"

namespace logic {

struct BindingsHelper {
};

class OlcbBoolVariable : public Variable, private openlcb::BitEventInterface {
 public:
  OlcbBoolVariable(uint64_t event_on, uint64_t event_off,
                   OlcbVariableFactory *parent)
      : BitEventInterface(event_on, event_off),
        state_(false),
        state_known_(false),
        parent_(parent) {}

  int max_state() override {
    // boolean: can be 0 or 1.
    return 1;
  }

  int read(const VariableFactory* parent, uint16_t arg) override {
    return state_ ? 1 : 0;
  }
  
  void write(const VariableFactory *parent, uint16_t arg, int value) override {
    bool need_update = !state_known_;
    state_known_ = true;
    if (state_ && !value) need_update = true;
    if (!state_ && value) need_update = true;
    state_ = value ? true : false;
    if (need_update) {
      pc_.SendEventReport(&parent_->helper_, parent_->bn_.reset(&parent_->sn_));
      parent_->sn_.wait_for_notification();
    }
  }

 private:
  openlcb::Node *node() override
  {
    return parent_->node_;
  }
  openlcb::EventState get_current_state() override
  {
    if (!state_known_) return openlcb::EventState::UNKNOWN;
    return state_ ? openlcb::EventState::VALID : openlcb::EventState::INVALID;
  }
  void set_state(bool new_value) override
  {
    state_known_ = true;
    state_ = new_value;
  }

  /// The current state of the variable as seen by the internal
  /// processing. (0=false, 1=true). This state is exported to the network iff
  /// state_known_ == 1.
  uint8_t state_ : 1;
  /// if 0, network queries will return UNKNOWN. If 1, network queries will
  /// return the state as defined by the state_ variable.
  uint8_t state_known_ : 1;
  /// Pointer to the Olcb variable factory that owns this. externally owned.
  OlcbVariableFactory* parent_;
  /// Implementation of the producer-consumer event handler.
  openlcb::BitEventPC pc_{this};
};

} // namespace logic

#endif // _LOGIC_OLCBBINDINGSIMPL_HXX_
