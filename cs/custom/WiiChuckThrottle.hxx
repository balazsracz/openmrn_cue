#ifndef _BRACZ_CUSTOM_WIICHUCKTHROTTLE_HXX_
#define _BRACZ_CUSTOM_WIICHUCKTHROTTLE_HXX_

#include "custom/WiiChuckData.hxx"
#include "nmranet/If.hxx"

namespace bracz_custom {

typedef StateFlow<Buffer<WiiChuckData>, QList<1> > WiiChuckFlow;

class WiiChuckThrottle : public WiiChuckFlow {
 public:
  WiiChuckThrottle(nmranet::Node* node, nmranet::NodeID dst_id)
    : WiiChuckFlow(node->interface()), node_(node), dst_{dst_id, 0} {}

  Action entry() OVERRIDE {
    ParsedGesture g = axis_to_gesture(message()->data()->joy_x());
    if (gesture_differs(g, lastGesture_)) {
      return call_update(STATE(update_velocity));
    }
    return release_and_exit();
  }

 private:
  static constexpr float MAX_SPEED_MPH = 30.0;

  nmranet::If* interface() { return static_cast<nmranet::If*>(service()); }

  Action call_update(Callback c) {
    return allocate_and_call(interface()->addressed_message_write_flow(), c);
  }

  Action send_message(const nmranet::Payload& p) {
    auto* b =
        get_allocation_result(interface()->addressed_message_write_flow());
    b->data()->reset(nmranet::Defs::MTI_TRACTION_CONTROL_COMMAND, node_->node_id(), dst_,
                     p);
    interface()->addressed_message_write_flow()->send(b);
    return call_immediately(STATE(entry));
  }

  Action update_velocity() {
    nmranet::Velocity v;
    ParsedGesture p = axis_to_gesture(message()->data()->joy_x());
    switch (p.gesture) {
      case LEFT_EXTREME: {
        v.reverse();
        v.set_mph(MAX_SPEED_MPH);
        break;
      }
      case LEFT: {
        v.reverse();
        v.set_mph(MAX_SPEED_MPH * p.argument() / 256);
        break;
      }
      case MIDDLE: {
        if (!lastIsForward_) v.reverse();
        v.set_mph(0);
        break;
      }
      case RIGHT: {
        v.set_mph(MAX_SPEED_MPH * p.argument() / 256);
        break;
      }
      case RIGHT_EXTREME: {
        v.set_mph(MAX_SPEED_MPH);
        break;
      }
    }
    if (v.direction() == nmranet::Velocity::FORWARD) {
      lastIsForward_ = 1;
    } else {
      lastIsForward_ = 0;
    }
    lastGesture_ = p;
    return send_message(nmranet::TractionDefs::speed_set_payload(v));
  }

  enum Gesture : uint8_t {
    LEFT_EXTREME,
    LEFT,
    MIDDLE,
    RIGHT,
    RIGHT_EXTREME
  };

  struct ParsedGesture {
    // value of enum Gesture.
    Gesture gesture;
    // Gesture value scaled to 0..255
    uint8_t arg;
    unsigned argument() { return arg; }
    ParsedGesture() : gesture(MIDDLE), arg(0) {}
    explicit ParsedGesture(Gesture e) : gesture(e), arg(0) {}
    ParsedGesture(Gesture e, uint8_t v) : gesture(e), arg(v) {}
  };

  /** Turns a raw measurement into a scaled gesture. */
  ParsedGesture axis_to_gesture(uint8_t d) {
    // Absolute value of steady state.
    static const unsigned HCENTER = 0x7b;
    static const unsigned LEFTEXTREME = 0x1a;
    static const unsigned RIGHTEXTREME = 0xdc;
    // Distance from the center to each direction which is the dead zone.
    static const unsigned CENTERPAD = 0x10;
    // Distance from an extreme to be recognized as such.
    static const unsigned EXTREMEPAD = 0x08;

    if (d < LEFTEXTREME + EXTREMEPAD) {
      return ParsedGesture(LEFT_EXTREME);
    } else if (d < HCENTER - CENTERPAD) {
      unsigned ofs = HCENTER - CENTERPAD - d;
      unsigned max = HCENTER - CENTERPAD - (LEFTEXTREME + EXTREMEPAD);
      ofs <<= 8;
      ofs /= max;
      if (ofs > 255) ofs = 255;
      return ParsedGesture(LEFT, ofs);
    } else if (d < HCENTER + CENTERPAD) {
      return ParsedGesture(MIDDLE);
    } else if (d < RIGHTEXTREME - EXTREMEPAD) {
      unsigned ofs = d - (HCENTER + CENTERPAD);
      unsigned max = RIGHTEXTREME - EXTREMEPAD - (HCENTER + CENTERPAD);
      ofs <<= 8;
      ofs /= max;
      if (ofs > 255) ofs = 255;
      return ParsedGesture(RIGHT, ofs);
    } else {
      return ParsedGesture(RIGHT_EXTREME);
    }
  }

  /** @returns true if the two gestures are materially different. */
  bool gesture_differs(ParsedGesture a, ParsedGesture b) {
    static const unsigned ARG_THRESHOLD = 0x20;
    if (a.gesture != b.gesture) return true;
    if (a.argument() > b.argument() + ARG_THRESHOLD) return true;
    if (b.argument() > a.argument() + ARG_THRESHOLD) return true;
    return false;
  }

  ParsedGesture lastGesture_;
  // X axis offset from previously.
  unsigned lastOffset_ : 8;
  // last value of button C
  unsigned lastC_ : 1;
  // last value of button Z
  unsigned lastZ_ : 1;
  unsigned lastIsForward_ : 1;

  nmranet::Node* node_;
  nmranet::NodeHandle dst_;
  DISALLOW_COPY_AND_ASSIGN(WiiChuckThrottle);
};

}  // namespace bracz_custom

#endif  // _BRACZ_CUSTOM_WIICHUCKTHROTTLE_HXX_
