#include "control-logic.hxx"

namespace automata {

typedef Automata::LocalVariable LocalVariable;

void StraightTrack::Bind(CtrlTrackInterface* me, CtrlTrackInterface* opposite) {
  me->binding = opposite;
}


void SimulateOccupancy(Automata* aut,
                       Automata::LocalVariable* sim_occ,
                       Automata::LocalVariable* tmp_seen_train_in_next,
                       const Automata::LocalVariable& route_set,
                       CtrlTrackInterface* past_if,
                       CtrlTrackInterface* next_if) {
  // tmp_seen_train_in_next is 1 if we have seen a train in the next (close)
  // occupancy block.
  HASSERT(past_if);
  HASSERT(next_if);
  const LocalVariable& previous_occ = aut->ImportVariable(
      past_if->binding->in_next_occ());

  Def()
      .IfReg1(route_set)
      .IfReg1(previous_occ)
      .ActReg1(sim_occ)
      .ActReg0(tmp_seen_train_in_next);

  if (next_if->binding->in_trainlength_occ) {
    const LocalVariable& next_trainlength_occ = aut->ImportVariable(
        *next_if->binding->in_trainlength_occ);
    // If the train has shown up in a far-away block, it has surel passed
    // through us.
    Def().IfReg1(route_set).IfReg1(next_trainlength_occ).
        ActReg0(sim_occ);
  }

  if (next_if->binding->in_close_occ) {
    const LocalVariable& next_close_occ = aut->ImportVariable(
        *next_if->binding->in_close_occ);
    // We save a bit when a trian has shown up in the next block.
    Def()
        .IfReg1(route_set).IfReg1(*sim_occ).IfReg1(next_close_occ)
        .ActReg1(tmp_seen_train_in_next);
    // And when it is gone from there, we must be unoccupied too.
    Def()
        .IfReg1(route_set).IfReg1(*sim_occ)
        .IfReg1(*tmp_seen_train_in_next).IfReg0(next_close_occ)
        .ActReg0(sim_occ);
  }
}


}  // namespace automata
