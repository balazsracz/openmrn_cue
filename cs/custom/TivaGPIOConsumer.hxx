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
 * \file TivaGPIOConsumer.hxx
 *
 * Simple Bit event implementation that Flips a TIVA gpio bit.
 *
 * @author Balazs Racz
 * @date 13 Dec 2015
 */

#ifndef _BRACZ_CUSTOM_TIVAGPIOCONSUMER_HXX_
#define _BRACZ_CUSTOM_TIVAGPIOCONSUMER_HXX_

#include "openlcb/SimpleStack.hxx"

extern openlcb::SimpleCanStack stack;

class TivaGPIOConsumer : public openlcb::BitEventInterface,
                         public openlcb::BitEventConsumer {
 public:
  TivaGPIOConsumer(uint64_t event_on, uint64_t event_off, uint32_t port,
                   uint8_t pin)
      : BitEventInterface(event_on, event_off),
        BitEventConsumer(this),
        memory_(reinterpret_cast<uint8_t*>(port + (pin << 2))) {}

  openlcb::EventState get_current_state() OVERRIDE {
    return (*memory_) ? openlcb::EventState::VALID
                      : openlcb::EventState::INVALID;
  }

  void set_state(bool new_value) OVERRIDE {
    if (new_value) {
      *memory_ = 0xff;
    } else {
      *memory_ = 0;
    }
  }

  openlcb::Node* node() OVERRIDE { 
    return stack.node(); 
  }

 private:
  volatile uint8_t* memory_;
};

#endif // _BRACZ_CUSTOM_TIVAGPIOCONSUMER_HXX_
