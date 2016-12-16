
#ifndef _BRACZ_CUSTOM_AUTOMATACONTROL_HXX_
#define _BRACZ_CUSTOM_AUTOMATACONTROL_HXX_

#include "openlcb/DatagramHandlerDefault.hxx"
#include "openlcb/Defs.hxx"
#include "src/automata_runner.h"

namespace bracz_custom {

struct AutomataDefs {
  enum {
    DATAGRAM_CODE = 0xF0,
    RESPONSE_CODE = 0xF1,

    STOP_AUTOMATA = 0x10,
    RESTART_AUTOMATA = 0x11,

    GET_AUTOMATA_STATE = 0x20,

    ERROR_AUTOMATA_NOT_FOUND = openlcb::Defs::ERROR_INVALID_ARGS | 0xF,
  };
};

class AutomataControl : public openlcb::DefaultDatagramHandler {
 public:
  AutomataControl(openlcb::Node* node, openlcb::DatagramService* if_datagram,
                  const insn_t* code)
      : DefaultDatagramHandler(if_datagram), runner_(node, code) {
    dg_service()->registry()->insert(runner_.node(),
                                     AutomataDefs::DATAGRAM_CODE, this);
  }

  ~AutomataControl() {
    dg_service()->registry()->erase(runner_.node(), AutomataDefs::DATAGRAM_CODE,
                                    this);
  }

  Action entry() OVERRIDE {
    if (size() < 2) {
      return respond_reject(openlcb::DatagramClient::PERMANENT_ERROR);
    }
    uint8_t cmd = payload()[1];
    needResponse_ = 0;
    switch (cmd) {
      case AutomataDefs::STOP_AUTOMATA: {
        runner_.Stop(this, false);
        return wait_and_call(STATE(stop_done));
      }
      case AutomataDefs::RESTART_AUTOMATA: {
        runner_.Start();
        return respond_ok(0);
      }
      case AutomataDefs::GET_AUTOMATA_STATE: {
        if (size() < 4) {
          return respond_reject(
              openlcb::Defs::ERROR_INVALID_ARGS_MESSAGE_TOO_SHORT);
        }
        automataNum_ = payload()[2];
        automataNum_ <<= 8;
        automataNum_ |= payload()[3];
        if (automataNum_ >= runner_.GetAllAutomatas().size()) {
          return respond_reject(AutomataDefs::ERROR_AUTOMATA_NOT_FOUND);
        }
        responsePayload_.clear();
        responsePayload_.push_back(AutomataDefs::RESPONSE_CODE);
        responsePayload_.push_back(cmd);
        responsePayload_.push_back(payload()[2]);
        responsePayload_.push_back(payload()[3]);
        responsePayload_.push_back(
            runner_.GetAllAutomatas()[automataNum_]->GetState());
        needResponse_ = 1;
        return respond_ok(openlcb::DatagramDefs::REPLY_PENDING);
      }
      default:
        return respond_reject(openlcb::DatagramClient::PERMANENT_ERROR);
    }
  }

  Action ok_response_sent() override {
    if (!needResponse_) return release_and_exit();
    return allocate_and_call(STATE(client_allocated),
                             dg_service()->client_allocator());
  }

  Action client_allocated() {
    clientFlow_ = full_allocation_result(dg_service()->client_allocator());
    return allocate_and_call(dg_service()->iface()->dispatcher(),
                             STATE(send_response_datagram));
  }

  Action send_response_datagram() {
    auto* b = get_allocation_result(dg_service()->iface()->dispatcher());
    b->set_done(b_.reset(this));
    b->data()->reset(openlcb::Defs::MTI_DATAGRAM,
                     message()->data()->dst->node_id(), message()->data()->src,
                     openlcb::EMPTY_PAYLOAD);
    b->data()->payload.swap(responsePayload_);
    clientFlow_->write_datagram(b);
    return wait_and_call(STATE(wait_response_datagram));
  }

  Action wait_response_datagram() {
    if (clientFlow_->result() & openlcb::DatagramClient::OPERATION_PENDING) {
      DIE("Unexpected notification from the datagram client.");
    }
    dg_service()->client_allocator()->typed_insert(clientFlow_);
    return release_and_exit();
  }

  Action stop_done() { return respond_ok(0); }

 private:
  AutomataRunner runner_;
  openlcb::DatagramClient* clientFlow_;
  openlcb::DatagramPayload responsePayload_;
  uint16_t automataNum_;
  uint16_t needResponse_ : 1;
  BarrierNotifiable b_;
};

}  // namespace bracz_custom

#endif  // _BRACZ_CUSTOM_AUTOMATACONTROL_HXX_
