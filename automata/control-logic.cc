#include <initializer_list>

#include "control-logic.hxx"

namespace automata {

StateRef StInit(0);
StateRef StBase(1);

void HandleInitState(Automata* aut) {
  Def().IfState(StInit).ActState(StBase);
}

typedef Automata::LocalVariable LocalVariable;

void StraightTrack::SimulateAllOccupancy(Automata* aut) {
  auto* sim_occ = aut->ImportVariable(&simulated_occupancy_);
  auto* tmp = aut->ImportVariable(&tmp_seen_train_in_next_);
  auto side_b_release = NewCallback(
      this, &StraightTrack::ReleaseRouteCallback, side_b());
  auto side_a_release = NewCallback(
      this, &StraightTrack::ReleaseRouteCallback, side_a());
  SimulateOccupancy(aut,
                    sim_occ,
                    tmp,
                    aut->ImportVariable(route_set_ab_),
                    side_a(),
                    side_b(),
                    &side_b_release);
  SimulateOccupancy(aut,
                    sim_occ,
                    tmp,
                    aut->ImportVariable(route_set_ba_),
                    side_b(),
                    side_a(),
                    &side_a_release);
}

typedef std::initializer_list<GlobalVariable*> MutableVarList;
typedef std::initializer_list<const GlobalVariable*> ConstVarList;

void SimulateRoute(Automata* aut,
                   CtrlTrackInterface* before,
                   CtrlTrackInterface* after,
                   LocalVariable* route_setting_in_progress,
                   const MutableVarList current_route,
                   const ConstVarList conflicting_routes) {
  LocalVariable* in_try_set_route =
      aut->ImportVariable(&before->binding()->out_try_set_route);
  LocalVariable* in_route_set_success =
      aut->ImportVariable(&before->in_route_set_success);
  LocalVariable* in_route_set_failure =
      aut->ImportVariable(&before->in_route_set_failure);
  LocalVariable* out_route_set_success =
      aut->ImportVariable(&after->binding()->in_route_set_success);
  LocalVariable* out_route_set_failure =
      aut->ImportVariable(&after->binding()->in_route_set_failure);
  LocalVariable* out_try_set_route =
      aut->ImportVariable(&after->out_try_set_route);
  const ConstVarList& const_current_route =
      reinterpret_cast<const ConstVarList&>(current_route);
  // Initialization
  Def().IfState(StInit)
      .ActReg0(out_try_set_route)
      .ActReg0(route_setting_in_progress)
      .ActReg0(in_route_set_success)
      .ActReg0(in_route_set_failure);

  // We reject the request immediately if there is another route setting in
  // progress.
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg1(*route_setting_in_progress)
      .ActReg1(in_route_set_failure)
      .ActReg0(in_route_set_success)
      .ActReg0(in_try_set_route);
  // We also reject is the outgoing route request hasn't returned to zero yet.
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg1(*route_setting_in_progress)
      .ActReg1(in_route_set_failure)
      .ActReg0(in_route_set_success)
      .ActReg0(in_try_set_route);
  // Check if we can propagate the route setting request.
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg0(*route_setting_in_progress)
      .Rept(&Automata::Op::IfReg0, const_current_route)
      .Rept(&Automata::Op::IfReg0, conflicting_routes)
      // then grab the in-progress lock
      .ActReg1(route_setting_in_progress)
      // reset feedback bits for safety
      .ActReg0(out_route_set_success)
      .ActReg0(out_route_set_failure)
      // and propagate request
      .ActReg1(out_try_set_route);
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg0(*route_setting_in_progress)
      // so we failed to propagate; reject the request.
      .ActReg1(in_route_set_failure)
      .ActReg0(in_route_set_success)
      .ActReg0(in_try_set_route);
  // Now look for feedback from propagated request.
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg1(*out_route_set_failure)
      .ActReg0(in_try_set_route)
      .ActReg0(out_route_set_failure)
      .ActReg0(out_route_set_success)
      .ActReg0(route_setting_in_progress)
      .ActReg0(in_route_set_success)
      .ActReg1(in_route_set_failure);
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg1(*out_route_set_success)
      .ActReg0(in_try_set_route)
      .ActReg0(out_route_set_failure)
      .ActReg0(out_route_set_success)
      .ActReg0(route_setting_in_progress)
      .ActReg0(in_route_set_failure)
      .ActReg1(in_route_set_success)
      .Rept(&Automata::Op::ActReg1, current_route);
}

void StraightTrack::SimulateAllRoutes(Automata* aut) {
  LocalVariable* route_setting_in_progress =
      aut->ImportVariable(&tmp_route_setting_in_progress_);
  SimulateRoute(aut,
                side_a(),
                side_b(),
                route_setting_in_progress,
                { &route_set_ab_ },
                { &route_set_ba_ });
  SimulateRoute(aut,
                side_b(),
                side_a(),
                route_setting_in_progress,
                { &route_set_ba_ },
                { &route_set_ab_ });
}

const CtrlTrackInterface* StraightTrack::FindOtherSide(
    const CtrlTrackInterface* s) {
  if (s == &side_a_) {
    return &side_b_;
  } else if (s == &side_b_) {
    return &side_a_;
  } else {
    extern bool could_not_find_opposide_side();
    HASSERT(false && could_not_find_opposide_side());
  }
  return NULL;
}



void SimulateOccupancy(Automata* aut,
                       Automata::LocalVariable* sim_occ,
                       Automata::LocalVariable* tmp_seen_train_in_next,
                       const Automata::LocalVariable& route_set,
                       CtrlTrackInterface* past_if,
                       CtrlTrackInterface* next_if,
                       OpCallback* release_cb) {
  // tmp_seen_train_in_next is 1 if we have seen a train in the next (close)
  // occupancy block.
  HASSERT(past_if);
  HASSERT(next_if);
  const LocalVariable& previous_occ = aut->ImportVariable(
      *past_if->binding()->LookupNextDetector());

  Def()
      .IfReg1(route_set)
      .IfReg1(previous_occ)
      .IfReg0(*sim_occ)
      .ActReg1(sim_occ)
      .ActReg0(tmp_seen_train_in_next);

  if (next_if->binding()->LookupFarDetector()) {
    //printf("Have far detector\n");
    const LocalVariable& next_trainlength_occ = aut->ImportVariable(
        *next_if->binding()->LookupFarDetector());
    // If the train has shown up in a far-away block, it has surely passed
    // through us.
    Def().IfReg1(route_set).IfReg1(next_trainlength_occ)
        .ActReg0(sim_occ)
        .RunCallback(release_cb);

  }

  if (next_if->binding()->LookupCloseDetector()) {
    //printf("Have close detector\n");
    const LocalVariable& next_close_occ = aut->ImportVariable(
        *next_if->binding()->LookupCloseDetector());
    // We save a bit when a train has shown up in the next block.
    Def()
        .IfReg1(route_set).IfReg1(*sim_occ).IfReg1(next_close_occ)
        .ActReg1(tmp_seen_train_in_next);
    // And when it is gone from there, we must be unoccupied too.
    Def()
        .IfReg1(route_set).IfReg1(*sim_occ)
        .IfReg1(*tmp_seen_train_in_next).IfReg0(next_close_occ)
        .ActReg0(sim_occ)
        .RunCallback(release_cb);
  }
}


}  // namespace automata
