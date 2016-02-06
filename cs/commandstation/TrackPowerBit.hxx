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
 * \file TrackPowerBit.hxx
 *
 * OpenLCB binding to the dcc track power bit.
 *
 * @author Balazs Racz
 * @date 6 Feb 2016
 */

#include "commandstation/dcc_control.hxx"

namespace commandstation {

class TrackPowerBit : public nmranet::BitEventInterface {
 public:
  TrackPowerOnOffBit(nmranet::Node* node, uint64_t event_on, uint64_t event_off)
      : BitEventInterface(event_on, event_off), node_(node) {}

  nmranet::EventState GetCurrentState() OVERRIDE {
    return query_dcc() ? nmranet::EventState::VALID
                       : nmranet::EventState::INVALID;
  }
  void SetState(bool new_value) OVERRIDE {
    if (new_value) {
      LOG(WARNING, "enable dcc");
      enable_dcc();
    } else {
      LOG(WARNING, "disable dcc");
      disable_dcc();
    }
  }

  nmranet::Node* node() OVERRIDE { return node_; }

 private:
  nmranet::Node* node_;
};

}  // namespace commandstation
