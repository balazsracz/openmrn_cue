/** \copyright
 * Copyright (c) 2014-2016, Balazs Racz
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
 * \file FindProtocolDefs.hxx
 *
 * Implementation of the find protocol event handler.
 *
 * @author Balazs Racz
 * @date 18 Feb 2016
 */

#ifndef _COMMANDSTATION_FINDPROTOCOLSERVER_HXX_
#define _COMMANDSTATION_FINDPROTOCOLSERVER_HXX_

#include "commandstation/FindProtocolDefs.hxx"
#include "nmranet/EventHandlerTemplates.hxx"

namespace commandstation {

class FindProtocolServer : public nmranet::SimpleEventHandler {
 public:
  FindProtocolServer(AllTrainNodes *nodes) {
    nmranet::EventRegistry::instance()->register_handler(
        EventRegistryEntry(this, FindProtocolDefs::TRAIN_FIND_BASE),
        FindProtocolDefs::TRAIN_FIND_MASK);
  }

  ~FindProtocolServer() {
    nmranet::EventRegistry::instance()->unregister_handler(this);
  }

  void HandleIdentifyGlobal(const EventRegistryEntry &registry_entry,
                            EventReport *event,
                            BarrierNotifiable *done) override {
    AutoNotify an(done);

    if (event && event->dst_node) {
      // Identify addressed
      auto *impl = parent_->find_node(event->dst_node);
      if (!impl) return;
      static_assert(((FindProtocolDefs::TRAIN_FIND_BASE >>
                      FindProtocolDefs::TRAIN_FIND_MASK) &
                     1) == 1,
                    "The lowermost bit of the TRAIN_FIND_BASE must be 1 or "
                    "else the event produced range encoding must be updated.");
      nmranet::event_write_helper1.WriteAsync(
          event->dst_node, nmranet::Defs::MTI_PRODUCER_IDENTIFIED_RANGE,
          nmranet::WriteHelper::global(),
          nmranet::eventid_to_buffer(FindProtocolDefs::TRAIN_FIND_BASE),
          done->new_child());
    } else {
      // Identify global

      // We do a synchronous alloc here but since we keep hold of the done
      // callback we are actually guaranteed to never have more than one buffer
      // in flight.
      auto *b = flow_.alloc();
      b->set_done(done->new_child());
      pendingGlobalIdentify_ = true;
      b->data()->reset(REQUEST_GLOBAL_IDENTIFY);
      flow_.send(b);
    }
  }

 private:
  enum {
    // Send this in the event_ field if there is a global identify
    // pending. This is not a valid EventID, because the upper byte is 0.
    REQUEST_GLOBAL_IDENTIFY = 0x0001000000000000U,

  };
  struct Request {
    void reset(nmranet::EventId event) { event_ = event; }
    EventId event_;
  };

  class FindProtocolFlow : public StateFlow<Buffer<Request>, QList<1> > {
   public:
    FindProtocolFlow(FindProtocolServer *parent)
        : StateFlow(parent->parent_->tractionService_) {}

    Action entry() override { return release_and_exit(); }
  };

  AllTrainNodes *parent_;

  /// Set to true when a global identify message is received. When a global
  /// identify starts processing, it shall be set to false. If a global
  /// identify request arrives with no pendingGlobalIdentify_, that is a
  /// duplicate request that can be ignored.
  bool pendingGlobalIdentify_{false};

  FindProtocolFlow flow_{this};
};

}  // namespace

#endif  // _COMMANDSTATION_FINDPROTOCOLSERVER_HXX_
