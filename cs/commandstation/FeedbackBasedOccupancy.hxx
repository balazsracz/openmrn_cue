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
 * \file FeedbackBasedOccupancy.hxx
 *
 * Helper flow for decoding occupancy information that is received from a
 * booster.
 *
 * @author Balazs Racz
 * @date 5 Jun 2015
 */

#ifndef _BRACZ_COMMANDSTATION_FEEDBACKBASEDOCCUPANCY_HXX_
#define _BRACZ_COMMANDSTATION_FEEDBACKBASEDOCCUPANCY_HXX_

#include "nmranet/EventHandlerTemplates.hxx"
#include "dcc/RailcomHub.hxx"

namespace commandstation {

class FeedbackBasedOccupancy : public dcc::RailcomHubPort {
 public:
    FeedbackBasedOccupancy(nmranet::Node* node, uint64_t event_base,
                         unsigned channel_count)
      : dcc::RailcomHubPort(node->iface()),
        currentValues_(0),
        eventHandler_(node, event_base, &currentValues_, channel_count) {}

  Action entry() {
    if (message()->data()->channel != 0xff) return release_and_exit();
    uint32_t new_values = message()->data()->ch1Data[0];
    release();
    if (new_values == currentValues_) return exit();
    unsigned bit = 1;
    unsigned ofs = 0;
    uint32_t diff = new_values ^ currentValues_;
    while (bit && ((diff & bit) == 0)) {
      bit <<= 1;
      ofs++;
    }
    eventHandler_.Set(ofs, (new_values & bit), &h_, n_.reset(this));
    return wait_and_call(STATE(set_done));
  }

  Action set_done() { return exit(); }

 private:
  uint32_t currentValues_;
  nmranet::BitRangeEventPC eventHandler_;
  nmranet::WriteHelper h_;
  BarrierNotifiable n_;
};

} // namespace commandstation

#endif // _BRACZ_COMMANDSTATION_FEEDBACKBASEDOCCUPANCY_HXX_

