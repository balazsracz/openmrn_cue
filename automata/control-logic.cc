#include <initializer_list>

#include "control-logic.hxx"

namespace automata {

StateRef StInit(0);
StateRef StBase(1);

void HandleInitState(Automata* aut) {
  Def().IfState(StInit).ActState(StBase);
}

bool BindSequence(
    const std::initializer_list<StraightTrackInterface*>& pieces) {
  StraightTrackInterface* before = nullptr;
  for (StraightTrackInterface* entry : pieces) {
    if (!before) {
      before = entry;
    } else {
      before->side_b()->Bind(entry->side_a());
      before = entry;
    }
  }
  return true;
}

typedef Automata::LocalVariable LocalVariable;

void StraightTrack::SimulateAllOccupancy(Automata* aut) {
  auto* sim_occ = aut->ImportVariable(simulated_occupancy_.get());
  auto* tmp = aut->ImportVariable(tmp_seen_train_in_next_.get());
  auto* route_set_ab = aut->ImportVariable(route_set_ab_.get());
  auto* route_set_ba = aut->ImportVariable(route_set_ba_.get());
  auto side_b_release = NewCallback(
      this, &StraightTrack::ReleaseRouteCallback, side_b(), route_set_ab);
  auto side_a_release = NewCallback(
      this, &StraightTrack::ReleaseRouteCallback, side_a(), route_set_ba);
  SimulateOccupancy(aut,
                    sim_occ,
                    tmp,
                    *route_set_ab,
                    side_a(),
                    side_b(),
                    &side_b_release);
  SimulateOccupancy(aut,
                    sim_occ,
                    tmp,
                    *route_set_ba,
                    side_b(),
                    side_a(),
                    &side_a_release);
}

typedef std::initializer_list<GlobalVariable*> MutableVarList;
typedef std::initializer_list<const GlobalVariable*> ConstVarList;

void StraightTrack::ReleaseRouteCallback(CtrlTrackInterface* side_out,
                                         LocalVariable* route_set,
                                         Automata::Op* op) {
  // TODO: strictly speaking here we should also take into account the
  // occupancy status of the neighbors to ensure that we are only releasing a
  // route component if the previous route components are already
  // released. That would need another temporary variable that we can save the
  // pending route release into.

  op->ActReg1(op->parent()->ImportVariable(side_out->out_route_released.get()));
  op->ActReg0(route_set);
}

void SimulateRoute(Automata* aut,
                   CtrlTrackInterface* before,
                   CtrlTrackInterface* after,
                   LocalVariable* any_route_setting_in_progress,
                   LocalVariable* current_route_setting_in_progress,
                   const MutableVarList current_route,
                   const ConstVarList conflicting_routes) {
  LocalVariable* in_try_set_route =
      aut->ImportVariable(before->binding()->out_try_set_route.get());
  LocalVariable* in_route_set_success =
      aut->ImportVariable(before->in_route_set_success.get());
  LocalVariable* in_route_set_failure =
      aut->ImportVariable(before->in_route_set_failure.get());
  LocalVariable* out_route_set_success =
      aut->ImportVariable(after->binding()->in_route_set_success.get());
  LocalVariable* out_route_set_failure =
      aut->ImportVariable(after->binding()->in_route_set_failure.get());
  LocalVariable* out_try_set_route =
      aut->ImportVariable(after->out_try_set_route.get());
  const ConstVarList& const_current_route =
      reinterpret_cast<const ConstVarList&>(current_route);
  // Initialization
  Def().IfState(StInit)
      .ActReg0(out_try_set_route)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(in_route_set_success)
      .ActReg0(in_route_set_failure);

  // We reject the request immediately if there is another route setting in
  // progress.
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg1(*any_route_setting_in_progress)
      .ActReg1(in_route_set_failure)
      .ActReg0(in_route_set_success)
      .ActReg0(in_try_set_route);
  // We also reject if the outgoing route request hasn't returned to zero yet.
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg1(*out_try_set_route)
      .ActReg1(in_route_set_failure)
      .ActReg0(in_route_set_success)
      .ActReg0(in_try_set_route);
  // Check if we can propagate the route setting request.
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg0(*any_route_setting_in_progress)
      .Rept(&Automata::Op::IfReg0, const_current_route)
      .Rept(&Automata::Op::IfReg0, conflicting_routes)
      // then grab the in-progress lock
      .ActReg1(any_route_setting_in_progress)
      // and move the route set request to pending
      .ActReg1(current_route_setting_in_progress)
      .ActReg0(in_try_set_route)
      // reset feedback bits for safety
      .ActReg0(out_route_set_success)
      .ActReg0(out_route_set_failure)
      // and propagate request
      .ActReg1(out_try_set_route);
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg0(*any_route_setting_in_progress)
      // so we failed to propagate; reject the request.
      .ActReg1(in_route_set_failure)
      .ActReg0(in_route_set_success)
      .ActReg0(in_try_set_route);
  // Now look for feedback from propagated request.
  Def()
      .IfReg1(*current_route_setting_in_progress)
      .IfReg1(*out_route_set_failure)
      .ActReg0(current_route_setting_in_progress)
      .ActReg0(out_route_set_failure)
      .ActReg0(out_route_set_success)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(in_route_set_success)
      .ActReg1(in_route_set_failure);
  Def()
      .IfReg1(*current_route_setting_in_progress)
      .IfReg1(*out_route_set_success)
      .ActReg0(current_route_setting_in_progress)
      .ActReg0(out_route_set_failure)
      .ActReg0(out_route_set_success)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(in_route_set_failure)
      .ActReg1(in_route_set_success)
      .Rept(&Automata::Op::ActReg1, current_route);
}

void StraightTrack::SimulateAllRoutes(Automata* aut) {
  LocalVariable* any_route_setting_in_progress =
      aut->ImportVariable(tmp_route_setting_in_progress_.get());
  Def().IfState(StInit)
      .ActReg0(aut->ImportVariable(route_set_ab_.get()))
      .ActReg0(aut->ImportVariable(route_set_ba_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ab_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ba_.get()));
  SimulateRoute(aut,
                side_a(),
                side_b(),
                any_route_setting_in_progress,
                aut->ImportVariable(route_pending_ab_.get()),
                { route_set_ab_.get() },
                { route_set_ba_.get() });
  SimulateRoute(aut,
                side_b(),
                side_a(),
                any_route_setting_in_progress,
                aut->ImportVariable(route_pending_ba_.get()),
                { route_set_ba_.get() },
                { route_set_ab_.get() });
}

void StraightTrackWithDetector::DetectorRoute(Automata* aut) {
  LocalVariable* any_route_setting_in_progress =
      aut->ImportVariable(tmp_route_setting_in_progress_.get());
  Def().IfState(StInit)
      .ActReg0(aut->ImportVariable(route_set_ab_.get()))
      .ActReg0(aut->ImportVariable(route_set_ba_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ab_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ba_.get()));
  // We use the detector as a conflicting route. This will prevent a route
  // being set across a straight track (in either direction) if the detector
  // shows not empty.
  SimulateRoute(aut,
                side_a(),
                side_b(),
                any_route_setting_in_progress,
                aut->ImportVariable(route_pending_ab_.get()),
                { route_set_ab_.get() },
                { route_set_ba_.get(), detector_ });
  SimulateRoute(aut,
                side_b(),
                side_a(),
                any_route_setting_in_progress,
                aut->ImportVariable(route_pending_ba_.get()),
                { route_set_ba_.get() },
                { route_set_ab_.get(), detector_ });
}

void SignalPiece::SignalOccupancy(Automata* aut) {
  const LocalVariable& prev_detector =
      aut->ImportVariable(*side_a()->binding()->LookupNextDetector());
  LocalVariable* occ = aut->ImportVariable(simulated_occupancy_.get());
  LocalVariable* signal = aut->ImportVariable(signal_);
  LocalVariable* route = aut->ImportVariable(route_set_ab_.get());
  Def()
      .IfReg1(prev_detector)
      .ActReg1(occ);
  Def()
      .IfReg0(prev_detector)
      .IfReg1(*occ)
      .ActReg0(signal)
      .ActReg0(route)
      .ActReg0(occ);
}

void StraightTrackWithRawDetector::RawDetectorOccupancy(Automata* aut) {
  static const int kTimeTakenToGoBusy = 2;
  static const int kTimeTakenToGoFree = 3;

  LocalVariable* occ = aut->ImportVariable(simulated_occupancy_.get());
  const LocalVariable& raw = aut->ImportVariable(*raw_detector_);
  LocalVariable* last(aut->ImportVariable(debounce_temp_var_.get()));
  LocalVariable* route1 = aut->ImportVariable(route_set_ab_.get());
  LocalVariable* route2 = aut->ImportVariable(route_set_ba_.get());

  // During boot we purposefully emit an event on the raw detector. This is a
  // poor-man's query: if the raw detector is implemented as a bit PC, then it
  // will respond with another event if the input doesn't match what we
  // emitted.
  Def()
      .IfState(StInit)
      .ActReg1(aut->ImportVariable(const_cast<GlobalVariable *>(raw_detector_)))
      .ActReg1(last);

  // If there is a flip, we start a timer. The timer length depends on the
  // edge.
  Def().IfReg0(raw).IfReg1(*last).ActTimer(kTimeTakenToGoBusy);
  Def().IfReg1(raw).IfReg0(*last).ActTimer(kTimeTakenToGoFree);
  aut->DefCopy(raw, last);

  // If no flip happened for the timer length, we put the value into the
  // occupancy bit.
  Def()
      .IfTimerDone().IfReg0(*last)
      .ActReg1(occ);
  Def()
      .IfTimerDone().IfReg1(*last).IfReg1(*occ)
      .ActReg0(route1)
      .ActReg0(route2)
      .ActReg0(occ);
}

void StraightTrackWithDetector::DetectorOccupancy(Automata* aut) {
  LocalVariable* occ = aut->ImportVariable(simulated_occupancy_.get());
  const LocalVariable& det = aut->ImportVariable(*detector_);
  LocalVariable* route1 = aut->ImportVariable(route_set_ab_.get());
  LocalVariable* route2 = aut->ImportVariable(route_set_ba_.get());
  Def()
      .IfReg1(det)
      .ActReg1(occ);
  Def()
      .IfReg0(det)
      .IfReg1(*occ)
      .ActReg0(route1)
      .ActReg0(route2)
      .ActReg0(occ);
}

void SimulateSignalFwdRoute(Automata* aut,
                            CtrlTrackInterface* before,
                            CtrlTrackInterface* after,
                            LocalVariable* any_route_setting_in_progress,
                            LocalVariable* current_route_setting_in_progress,
                            const MutableVarList current_route,
                            const ConstVarList conflicting_routes,
                            GlobalVariable* go_signal,
                            GlobalVariable* in_request_green) {
  LocalVariable* in_try_set_route =
      aut->ImportVariable(before->binding()->out_try_set_route.get());
  LocalVariable* in_route_set_success =
      aut->ImportVariable(before->in_route_set_success.get());
  LocalVariable* in_route_set_failure =
      aut->ImportVariable(before->in_route_set_failure.get());
  LocalVariable* out_route_set_success =
      aut->ImportVariable(after->binding()->in_route_set_success.get());
  LocalVariable* out_route_set_failure =
      aut->ImportVariable(after->binding()->in_route_set_failure.get());
  LocalVariable* out_try_set_route =
      aut->ImportVariable(after->out_try_set_route.get());
  const ConstVarList& const_current_route =
      reinterpret_cast<const ConstVarList&>(current_route);
  LocalVariable* signal =
      aut->ImportVariable(go_signal);

  // Initialization
  Def().IfState(StInit)
      .ActReg0(out_try_set_route)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(in_route_set_success)
      .ActReg0(in_route_set_failure);

  // We reject the request immediately if there is another route setting in
  // progress.
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg1(*any_route_setting_in_progress)
      .ActReg1(in_route_set_failure)
      .ActReg0(in_route_set_success)
      .ActReg0(in_try_set_route);

  // Accept the incoming route if we can.
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg0(*any_route_setting_in_progress)
      .Rept(&Automata::Op::IfReg0, const_current_route)
      .Rept(&Automata::Op::IfReg0, conflicting_routes)
      // then we accept the incoming route
      // For safety, we set the signal to red if we accepted a route.
      .ActReg0(signal)
      .ActReg0(in_try_set_route)
      .ActReg0(in_route_set_failure)
      .ActReg1(in_route_set_success);

  //  ===== Outgoing route setting ======

  LocalVariable* request_green = aut->ImportVariable(in_request_green);

  Def()
      .IfReg1(*request_green)
      .IfReg0(*out_try_set_route)
      .IfReg0(*out_route_set_success)
      .IfReg0(*out_route_set_failure)
      .IfReg0(*any_route_setting_in_progress)
      .ActReg1(out_try_set_route)
      .ActReg1(any_route_setting_in_progress);

  Def().IfReg1(*out_route_set_success)
      .ActReg0(out_route_set_success)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(request_green)
      .Rept(&Automata::Op::ActReg1, current_route)
      .ActReg1(signal);

  Def().IfReg1(*out_route_set_failure)
      .ActReg0(out_route_set_failure)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(request_green)
      .ActReg0(signal);
}

void SignalPiece::SignalRoute(Automata* aut) {
  LocalVariable* any_route_setting_in_progress =
      aut->ImportVariable(tmp_route_setting_in_progress_.get());
  Def().IfState(StInit)
      .ActReg0(aut->ImportVariable(route_set_ab_.get()))
      .ActReg0(aut->ImportVariable(route_set_ba_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ab_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ba_.get()));

  // In direction b->a the signal track is completely normal.
  SimulateRoute(aut,
                side_b(),
                side_a(),
                any_route_setting_in_progress,
                aut->ImportVariable(route_pending_ba_.get()),
                { route_set_ba_.get() },
                { route_set_ab_.get() });

  // Whereas in direction a->b we have the special route setting logic.
  SimulateSignalFwdRoute(aut,
                         side_a(),
                         side_b(),
                         any_route_setting_in_progress,
                         aut->ImportVariable(route_pending_ab_.get()),
                         { route_set_ab_.get() },
                         { route_set_ba_.get() },
                         signal_,
                         request_green_);

  aut->DefCopy(aut->ImportVariable(*route_set_ab_),
               aut->ImportVariable(signal_));
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
