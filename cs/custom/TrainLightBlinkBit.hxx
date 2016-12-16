/** \copyright
 * Copyright (c) 2016, Balazs Racz
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
 * \file TrainLightBlinkBit.hxx
 *
 * Debug flow that blinks a train's light based on incoming events.
 *
 * @author Balazs Racz
 * @date 6 Feb 2016
 */

#ifndef _BRACZ_CUSTOM_TRAINLIGHTBLINKBIT_HXX_
#define _BRACZ_CUSTOM_TRAINLIGHTBLINKBIT_HXX_

#include "nmranet/TrainInterface.hxx"

class TrainLightBlinkBit : public openlcb::BitEventInterface {
 public:
  TrainLightBlinkBit(uint64_t event_on, uint64_t event_off,
                     openlcb::Node* node, openlcb::TrainImpl* train)
      : BitEventInterface(event_on, event_off), train_(train), node_(node) {}

  openlcb::EventState get_current_state() OVERRIDE {
    return openlcb::to_event_state(train_->get_fn(0));
  }

  void set_state(bool new_value) OVERRIDE {
    train_->set_fn(0, new_value ? 1 : 0);
  }

  openlcb::Node* node() OVERRIDE { return node_; }

 private:
  openlcb::TrainImpl* train_;
  openlcb::Node* node_;
};

#endif  // _BRACZ_CUSTOM_TRAINLIGHTBLINKBIT_HXX_
