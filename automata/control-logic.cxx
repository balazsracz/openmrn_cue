#include "control-logic.hxx"

namespace automata {


void StraightTrack::Bind(CtrlTrackInterface* me, CtrlTrackInterface* opposite) {
  me->binding = opposite;
}

}  // namespace automata
