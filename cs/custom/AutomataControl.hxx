
#ifndef _BRACZ_CUSTOM_AUTOMATACONTROL_HXX_
#define _BRACZ_CUSTOM_AUTOMATACONTROL_HXX_

#include "src/automata_runner.h"
#include "nmranet/DatagramHandlerDefault.hxx"

namespace bracz_custom {

struct AutomataDefs {
  enum {
    DATAGRAM_CODE = 0xF0,

    STOP_AUTOMATA = 0x10,
    RESTART_AUTOMATA = 0x11,
  };
};

class AutomataControl : public nmranet::DefaultDatagramHandler {
 public:

  AutomataControl(nmranet::Node* node, nmranet::DatagramService* if_datagram, const insn_t* code)
      : DefaultDatagramHandler(if_datagram),
        runner_(node, code) {
    dg_service()->registry()->insert(runner_.node(), AutomataDefs::DATAGRAM_CODE, this);
  }

  ~AutomataControl() {
    dg_service()->registry()->erase(runner_.node(), AutomataDefs::DATAGRAM_CODE, this);
  }

  Action entry() OVERRIDE {
    if (size() < 2) {
      return respond_reject(nmranet::DatagramClient::PERMANENT_ERROR);
    }
    uint8_t cmd = payload()[1];
    switch(cmd) {
      case AutomataDefs::STOP_AUTOMATA: {
        runner_.Stop(this, false);
        return wait_and_call(STATE(stop_done));
      }
      case AutomataDefs::RESTART_AUTOMATA: {
        runner_.Start();
        return respond_ok(0);
      }
      default:
        return respond_reject(nmranet::DatagramClient::PERMANENT_ERROR);
    }
  }

  Action stop_done() {
    return respond_ok(0);
  }

 private:
  AutomataRunner runner_;
};

} // namespace bracz_custom

#endif // _BRACZ_CUSTOM_AUTOMATACONTROL_HXX_
