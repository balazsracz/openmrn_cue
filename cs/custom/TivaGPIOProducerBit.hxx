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
 * \file TivaGPIOProducerBit.hxx
 *
 * Simple Bit event implementation that polls a Tiva GPIO bit.
 *
 * @author Balazs Racz
 * @date 13 Dec 2015
 */

#ifndef _BRACZ_CUSTOM_TIVAGPIOPRODUCERBIT_HXX_
#define _BRACZ_CUSTOM_TIVAGPIOPRODUCERBIT_HXX_

#include "openlcb/SimpleStack.hxx"

extern openlcb::SimpleCanStack stack;

class TivaGPIOProducerBit : public openlcb::BitEventInterface {
 public:
  TivaGPIOProducerBit(uint64_t event_on, uint64_t event_off, uint32_t port_base,
                      uint8_t port_bit, bool display = false)
      : BitEventInterface(event_on, event_off),
        ptr_(reinterpret_cast<const uint8_t*>(port_base +
                                              (((unsigned)port_bit) << 2))),
        display_(display) {}

  openlcb::EventState get_current_state() OVERRIDE {
    bool result = *ptr_;
    if (display_) {
      Debug::DetectRepeat::set(result);
    }
    return result ? openlcb::EventState::VALID : openlcb::EventState::INVALID;
  }

  void set_state(bool new_value) OVERRIDE {
    DIE("cannot set state of input producer");
  }

  openlcb::Node* node() OVERRIDE { return stack.node(); }

 private:
  const uint8_t* ptr_;
  bool display_;
};

#endif // _BRACZ_CUSTOM_TIVAGPIOPRODUCERBIT_HXX_
