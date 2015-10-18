#include <initializer_list>

#include "control-logic.hxx"

namespace automata {

GlobalVariable* reset_routes = nullptr;

void HandleInitState(Automata* aut) { Def().IfState(StInit).ActState(StBase); }

bool BindSequence(
    CtrlTrackInterface* before,
    const std::initializer_list<StraightTrackInterface*>& pieces,
    CtrlTrackInterface* after) {
  for (StraightTrackInterface* entry : pieces) {
    if (before) {
      before->Bind(entry->side_a());
    }
    before = entry->side_b();
  }
  if (after) {
    before->Bind(after);
  }
  return true;
}

bool BindPairs(const std::initializer_list<
    std::initializer_list<CtrlTrackInterface*> >& pieces) {
  for (const auto& p : pieces) {
    assert(p.size() == 2);
    CtrlTrackInterface*const *const entries = p.begin();
    entries[0]->Bind(entries[1]);
  }
  return true;
}


typedef Automata::LocalVariable LocalVariable;

// This funciton will be called from SimulateOccupancy when the simulated
// occupancy goes to zero and thus the route should be released.
void ReleaseRouteCallback(CtrlTrackInterface* side_out,
                          LocalVariable* route_set, Automata::Op* op) {
  // TODO: strictly speaking here we should also take into account the
  // occupancy status of the neighbors to ensure that we are only releasing a
  // route component if the previous route components are already
  // released. That would need another temporary variable that we can save the
  // pending route release into.

  op->ActReg1(op->parent()->ImportVariable(side_out->out_route_released.get()));
  op->ActReg0(route_set);
}

void StraightTrack::SimulateAllOccupancy(Automata* aut) {
  auto* sim_occ = aut->ImportVariable(simulated_occupancy_.get());
  auto* tmp = aut->ImportVariable(tmp_seen_train_in_next_.get());
  auto* route_set_ab = aut->ImportVariable(route_set_ab_.get());
  auto* route_set_ba = aut->ImportVariable(route_set_ba_.get());
  auto side_b_release =
      NewCallback(&ReleaseRouteCallback, side_b(), route_set_ab);
  auto side_a_release =
      NewCallback(&ReleaseRouteCallback, side_a(), route_set_ba);
  SimulateOccupancy(aut, sim_occ, tmp, *route_set_ab, side_a(), side_b(),
                    &side_b_release);
  SimulateOccupancy(aut, sim_occ, tmp, *route_set_ba, side_b(), side_a(),
                    &side_a_release);
}

typedef std::initializer_list<GlobalVariable*> MutableVarList;
typedef std::initializer_list<const GlobalVariable*> ConstVarList;

void SimulateRoute(Automata* aut,
                   OpCallback* condition_cb,
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
  Def()
      .IfState(StInit)
      .ActReg0(out_try_set_route)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(in_route_set_success)
      .ActReg0(in_route_set_failure);

  if (reset_routes) {
    Def().IfReg1(aut->ImportVariable(*reset_routes))
      .ActReg0(out_try_set_route)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(in_route_set_success)
      .ActReg0(in_route_set_failure)
      .Rept(&Automata::Op::ActReg0, current_route);
  }

  // We reject the request immediately if there is another route setting in
  // progress.
  Def()
      .RunCallback(condition_cb)
      .IfReg1(*in_try_set_route)
      .IfReg1(*any_route_setting_in_progress)
      .ActReg1(in_route_set_failure)
      .ActReg0(in_route_set_success)
      .ActReg0(in_try_set_route);
  // We also reject if the outgoing route request hasn't returned to zero yet.
  Def()
      .RunCallback(condition_cb)
      .IfReg1(*in_try_set_route)
      .IfReg1(*out_try_set_route)
      .ActReg1(in_route_set_failure)
      .ActReg0(in_route_set_success)
      .ActReg0(in_try_set_route);
  // Check if we can propagate the route setting request.
  Def()
      .RunCallback(condition_cb)
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
      .RunCallback(condition_cb)
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
  Def()
      .IfState(StInit)
      .ActReg1(aut->ImportVariable(route_set_ab_.get()))
      .ActReg0(aut->ImportVariable(route_set_ab_.get()))
      .ActReg0(aut->ImportVariable(route_set_ba_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ab_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ba_.get()));
  SimulateRoute(aut, nullptr, side_a(), side_b(), any_route_setting_in_progress,
                aut->ImportVariable(route_pending_ab_.get()),
                {route_set_ab_.get()}, {route_set_ba_.get()});
  SimulateRoute(aut, nullptr, side_b(), side_a(), any_route_setting_in_progress,
                aut->ImportVariable(route_pending_ba_.get()),
                {route_set_ba_.get()}, {route_set_ab_.get()});
}

void StraightTrackWithDetector::DetectorRoute(Automata* aut) {
  LocalVariable* any_route_setting_in_progress =
      aut->ImportVariable(tmp_route_setting_in_progress_.get());
  Def()
      .IfState(StInit)
      .ActReg0(aut->ImportVariable(route_set_ab_.get()))
      .ActReg0(aut->ImportVariable(route_set_ba_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ab_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ba_.get()));
  // We use the detector as a conflicting route. This will prevent a route
  // being set across a straight track (in either direction) if the detector
  // shows not empty.
  SimulateRoute(aut, nullptr, side_a(), side_b(), any_route_setting_in_progress,
                aut->ImportVariable(route_pending_ab_.get()),
                {route_set_ab_.get()}, {route_set_ba_.get(), detector_});
  SimulateRoute(aut, nullptr, side_b(), side_a(), any_route_setting_in_progress,
                aut->ImportVariable(route_pending_ba_.get()),
                {route_set_ba_.get()}, {route_set_ab_.get(), detector_});
}

void SignalPiece::SignalOccupancy(Automata* aut) {
  const LocalVariable& prev_detector =
      aut->ImportVariable(*side_a()->binding()->LookupNextDetector());
  LocalVariable* occ = aut->ImportVariable(simulated_occupancy_.get());
  LocalVariable* route = aut->ImportVariable(route_set_ab_.get());
  LocalVariable* route2 = aut->ImportVariable(route_set_ba_.get());
  LocalVariable* signal = signal_ ? aut->ImportVariable(signal_) : nullptr;
  Def().IfReg1(prev_detector).ActReg1(occ);
  Def()
      .IfReg0(prev_detector)
      .IfReg1(*occ)
      .MaybeActReg(signal_, signal, 0)
      .ActReg0(route)
      .ActReg0(route2)
      .ActReg0(occ);
}

void StraightTrackWithRawDetector::RawDetectorOccupancy(int min_occupied_time,
                                                        Automata* aut) {
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
      // Ensures proper initialization of raw detector
      .ActReg1(aut->ImportVariable(const_cast<GlobalVariable*>(raw_detector_)))
      // This will make it fail-safe -- if destination HW is down, it will be
      // marked as busy.
      .ActReg0(aut->ImportVariable(const_cast<GlobalVariable*>(raw_detector_)))
      .ActReg1(last);


  if (min_occupied_time) {
    // If there is a route set, then we do not delay setting the occupancy to
    // busy.
    Def().IfReg0(raw).IfReg1(*last).IfReg1(*route1)
        .ActTimer(min_occupied_time).ActReg0(last).ActReg1(occ);
    Def().IfReg0(raw).IfReg1(*last).IfReg1(*route2)
        .ActTimer(min_occupied_time).ActReg0(last).ActReg1(occ);

    // If there is a flip, we start a timer. The timer length depends on the
    // edge.
    Def().IfReg0(raw).IfReg1(*last).ActTimer(kTimeTakenToGoBusy).ActReg0(last);
    Def().IfReg1(raw).IfReg0(*last)
        .IfTimerDone()
        .ActTimer(kTimeTakenToGoFree).ActReg1(last);

    Def().IfTimerDone().IfReg0(*last).IfReg0(*occ).ActReg1(occ).ActTimer(
        min_occupied_time);
  } else {
    // If there is a route set, then we do not delay setting the occupancy to
    // busy.
    Def().IfReg0(raw).IfReg1(*last).IfReg1(*route1).ActTimer(0).ActReg0(last);
    Def().IfReg0(raw).IfReg1(*last).IfReg1(*route2).ActTimer(0).ActReg0(last);

    // If there is a flip, we start a timer. The timer length depends on the
    // edge.
    Def().IfReg0(raw).IfReg1(*last).ActTimer(kTimeTakenToGoBusy).ActReg0(last);
    Def().IfReg1(raw).IfReg0(*last).ActTimer(kTimeTakenToGoFree).ActReg1(last);
  }

  // If no flip happened for the timer length, we put the value into the
  // occupancy bit.
  Def().IfTimerDone().IfReg0(*last).ActReg1(occ);
  Def()
      .IfTimerDone()
      .IfReg1(*last)
      .IfReg1(*occ)
      .ActReg0(route1)
      .ActReg0(route2)
      .ActReg0(occ);
}

void StraightTrackWithDetector::DetectorOccupancy(Automata* aut) {
  LocalVariable* occ = aut->ImportVariable(simulated_occupancy_.get());
  const LocalVariable& det = aut->ImportVariable(*detector_);
  LocalVariable* route1 = aut->ImportVariable(route_set_ab_.get());
  LocalVariable* route2 = aut->ImportVariable(route_set_ba_.get());
  Def().IfReg1(det).ActReg1(occ);
  Def().IfReg0(det).IfReg1(*occ).ActReg0(route1).ActReg0(route2).ActReg0(occ);
}

void RouteShortcutCb(const LocalVariable& shortcut, Automata::Op* op) {
  op->IfReg1(shortcut);
}

void SimulateSignalFwdRoute(Automata* aut, CtrlTrackInterface* before,
                            CtrlTrackInterface* after,
                            LocalVariable* any_route_setting_in_progress,
                            LocalVariable* current_route_setting_in_progress,
                            const MutableVarList current_route,
                            const ConstVarList conflicting_routes,
                            GlobalVariable* go_signal,
                            GlobalVariable* in_request_green,
                            const GlobalVariable& in_route_no_stop) {
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
  LocalVariable* signal = go_signal ? aut->ImportVariable(go_signal) : nullptr;
  const LocalVariable& route_no_stop = aut->ImportVariable(in_route_no_stop);

  auto cb_nostop = NewCallback(&RouteShortcutCb, route_no_stop);

  // Initialization
  Def()
      .IfState(StInit)
      .ActReg0(out_try_set_route)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(in_route_set_success)
      .ActReg0(in_route_set_failure);

  if (reset_routes) {
    Def().IfReg1(aut->ImportVariable(*reset_routes))
      .ActReg0(out_try_set_route)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(in_route_set_success)
      .ActReg0(in_route_set_failure)
      .Rept(&Automata::Op::ActReg0, current_route);
  }

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
      .IfReg0(route_no_stop)
      .IfReg0(*any_route_setting_in_progress)
      .Rept(&Automata::Op::IfReg0, const_current_route)
      .Rept(&Automata::Op::IfReg0, conflicting_routes)
      // then we accept the incoming route
      // For safety, we set the signal to red if we accepted a route.
      .MaybeActReg(go_signal, signal, 0)
      .ActReg0(in_try_set_route)
      .ActReg0(in_route_set_failure)
      .ActReg1(in_route_set_success);

  // Reject the route if we didn't accept in the previous instruction.
  Def()
      .IfReg1(*in_try_set_route)
      .IfReg0(route_no_stop)
      .ActReg1(in_route_set_failure)
      .ActReg0(in_route_set_success)
      .ActReg0(in_try_set_route);

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

  Def()
      .IfReg1(*request_green)
      .IfReg1(*out_route_set_success)
      .ActReg0(out_route_set_success)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(request_green)
      .Rept(&Automata::Op::ActReg1, current_route)
      .MaybeActReg(go_signal, signal, 1);

  Def()
      .IfReg1(*request_green)
      .IfReg1(*out_route_set_failure)
      .ActReg0(out_route_set_failure)
      .ActReg0(any_route_setting_in_progress)
      .ActReg0(request_green)
      .MaybeActReg(go_signal, signal, 0);

  SimulateRoute(aut, &cb_nostop, before, after, any_route_setting_in_progress,
                current_route_setting_in_progress, current_route,
                conflicting_routes);
}

void SignalPiece::SignalRoute(Automata* aut) {
  LocalVariable* any_route_setting_in_progress =
      aut->ImportVariable(tmp_route_setting_in_progress_.get());
  Def()
      .IfState(StInit)
      .ActReg1(aut->ImportVariable(route_set_ab_.get()))
      .ActReg0(aut->ImportVariable(route_set_ab_.get()))
      .ActReg0(aut->ImportVariable(route_set_ba_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ab_.get()))
      .ActReg0(aut->ImportVariable(route_pending_ba_.get()));
  if (signal_) {
    Def()
        .IfState(StInit)
        .ActReg1(aut->ImportVariable(signal_))  // ensures signals are zero
        .ActReg0(aut->ImportVariable(signal_));
  }

  // In direction b->a the signal track is completely normal.
  SimulateRoute(aut, nullptr, side_b(), side_a(), any_route_setting_in_progress,
                aut->ImportVariable(route_pending_ba_.get()),
                {route_set_ba_.get()}, {route_set_ab_.get()});

  // Whereas in direction a->b we have the special route setting logic.
  SimulateSignalFwdRoute(aut, side_a(), side_b(), any_route_setting_in_progress,
                         aut->ImportVariable(route_pending_ab_.get()),
                         {route_set_ab_.get()}, {route_set_ba_.get()}, signal_,
                         request_green_, *route_no_stop_);

  // We set the signal to green (give power to track) if either ab or ba route
  // is set. TODO(balazs.racz): This is actually a case where the signal and
  // the track-power bit should be treated separately.
  const auto& route_set_ab = aut->ImportVariable(*route_set_ab_);
  const auto& route_set_ba = aut->ImportVariable(*route_set_ba_);
  if (signal_) {
    auto* signal = aut->ImportVariable(signal_);
    Def().IfReg0(route_set_ab).IfReg0(route_set_ba).ActReg0(signal);
    Def().IfReg1(route_set_ab).ActReg1(signal);
    Def().IfReg1(route_set_ba).ActReg1(signal);
  }
}

const CtrlTrackInterface* StraightTrack::FindOtherSide(
    const CtrlTrackInterface* s) const {
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

void SimulateOccupancy(Automata* aut, Automata::LocalVariable* sim_occ,
                       Automata::LocalVariable* tmp_seen_train_in_next,
                       const Automata::LocalVariable& route_set,
                       CtrlTrackInterface* past_if, CtrlTrackInterface* next_if,
                       OpCallback* release_cb) {
  // tmp_seen_train_in_next is 1 if we have seen a train in the next (close)
  // occupancy block.
  HASSERT(past_if);
  HASSERT(next_if);
  const LocalVariable& previous_occ =
      aut->ImportVariable(*past_if->binding()->LookupNextDetector());

  Def()
      .IfReg1(route_set)
      .IfReg1(previous_occ)
      .IfReg0(*sim_occ)
      .ActReg1(sim_occ)
      .ActReg0(tmp_seen_train_in_next);

  auto* far_detector = next_if->binding()->LookupFarDetector();
  auto* next_detector = next_if->binding()->LookupNextDetector();
  if (far_detector) {
    // printf("Have far detector\n");
    const LocalVariable& next_trainlength_occ =
        aut->ImportVariable(*far_detector);
    // If the train has shown up in a far-away block, it has surely passed
    // through us.
    Def()
        .IfReg1(route_set)
        .IfReg1(next_trainlength_occ)
        .ActReg0(sim_occ)
        .RunCallback(release_cb);
  }

  if (next_detector && (next_detector != far_detector)) {
    // printf("Have close detector\n");
    const LocalVariable& next_close_occ =
        aut->ImportVariable(*next_detector);
    // We save a bit when a train has shown up in the next block.
    Def().IfReg1(route_set).IfReg1(*sim_occ).IfReg1(next_close_occ).ActReg1(
        tmp_seen_train_in_next);
    // And when it is gone from there, we must be unoccupied too.
    Def()
        .IfReg1(route_set)
        .IfReg1(*sim_occ)
        .IfReg1(*tmp_seen_train_in_next)
        .IfReg0(next_close_occ)
        .ActReg0(sim_occ)
        .RunCallback(release_cb);
  }
}

void FixedTurnout::FixTurnoutState(Automata* aut) {
  LocalVariable* turnoutstate = aut->ImportVariable(turnout_state_.get());
  // We always fix the turnout state to be the same as at definition-time,
  // except if there is a route set that slices up the turnout. In that case we
  // "fake" the turnout to be sliced up and pointing the other way. This is
  // necessary so that the LookupNextDetector() calls give an accurate value
  // for the blocks to follow. Otherwise they might not see that there is a
  // train coming from behind and the SimulateOccupancy feature would give
  // false values.
  if (state_ == TURNOUT_CLOSED) {
    Def().IfReg1(aut->ImportVariable(*route_set_TP_)).ActReg1(turnoutstate);
    Def().IfReg0(aut->ImportVariable(*route_set_TP_)).ActReg0(turnoutstate);
  } else if (state_ == TURNOUT_THROWN) {
    Def().IfReg1(aut->ImportVariable(*route_set_CP_)).ActReg0(turnoutstate);
    Def().IfReg0(aut->ImportVariable(*route_set_CP_)).ActReg1(turnoutstate);
  } else {
    assert(0);
  }
}

void MovableTurnout::CopyState(Automata* aut) {
  LocalVariable* turnoutstate = aut->ImportVariable(turnout_state_.get());
  const LocalVariable& physical_state = aut->ImportVariable(*magnet_->current_state);
  const LocalVariable& route_thrown = aut->ImportVariable(*route_set_TP_);
  const LocalVariable& route_closed = aut->ImportVariable(*route_set_CP_);
  Def().IfReg1(route_thrown).ActReg1(turnoutstate);
  Def().IfReg1(route_closed).ActReg0(turnoutstate);
  Def()
      .IfReg0(route_thrown).IfReg0(route_closed)
      .IfReg1(physical_state)
      .ActReg1(turnoutstate);
  Def()
      .IfReg0(route_thrown).IfReg0(route_closed)
      .IfReg0(physical_state)
      .ActReg0(turnoutstate);
}



void TurnoutBase::TurnoutOccupancy(Automata* aut) {
  auto* sim_occ = aut->ImportVariable(simulated_occupancy_.get());
  auto* tmp = aut->ImportVariable(tmp_seen_train_in_next_.get());

  for (const auto& d : directions_) {
    auto* route_set = aut->ImportVariable(d.route);
    auto release = NewCallback(&ReleaseRouteCallback, d.to, route_set);
    SimulateOccupancy(aut, sim_occ, tmp, *route_set, d.from, d.to, &release);
  }
}

void DKW::DKWOccupancy(Automata* aut) {
  auto* sim_occ = aut->ImportVariable(simulated_occupancy_.get());
  auto* tmp = aut->ImportVariable(tmp_seen_train_in_next_.get());

  for (const auto& d : routes_) {
    auto* route_set = aut->ImportVariable(d.route_set.get());
    auto release = NewCallback(&ReleaseRouteCallback,
                               points_[d.to].interface.get(), route_set);
    SimulateOccupancy(aut, sim_occ, tmp, *route_set,
                      points_[d.from].interface.get(),
                      points_[d.to].interface.get(), &release);
  }
}

void TurnoutDirectionCheck(const LocalVariable& state, bool set, Automata::Op* op) {
  if (set) {
    op->IfReg1(state);
  } else {
    op->IfReg0(state);
  }
}

void TurnoutBase::PopulateAnyRouteSet(Automata* aut) {
  vector<const GlobalVariable*> all_routes;
  for (const auto& d : directions_) {
    all_routes.push_back(d.route);
  }

  LocalVariable* any_route_set = aut->ImportVariable(any_route_set_.get());

  const vector<const automata::GlobalVariable*>& v = all_routes;

  Def().Rept(&Automata::Op::IfReg0, v).ActReg0(any_route_set);

  for (const auto& d : directions_) {
    Def().IfReg1(aut->ImportVariable(*d.route)).ActReg1(any_route_set);
  }
}

void DKW::PopulateAnyRouteSet(Automata* aut) {
  vector<const GlobalVariable*> all_routes;
  for (const auto& d : routes_) {
    all_routes.push_back(d.route_set.get());
  }

  LocalVariable* any_route_set = aut->ImportVariable(any_route_set_.get());

  const vector<const automata::GlobalVariable*>& v = all_routes;

  Def().Rept(&Automata::Op::IfReg0, v).ActReg0(any_route_set);

  for (auto* v : all_routes) {
    Def().IfReg1(aut->ImportVariable(*v)).ActReg1(any_route_set);
  }
}

void ProxyOneDetector(Automata* aut, OpCallback* condition,
                      const GlobalVariable* next, GlobalVariable* g_proxy) {
  LocalVariable* proxy = aut->ImportVariable(g_proxy);
  if (next) {
    const LocalVariable& det = aut->ImportVariable(*next);
    Def().RunCallback(condition).IfReg0(det).ActReg0(proxy);
    Def().RunCallback(condition).IfReg1(det).ActReg1(proxy);
  } else {
    Def().RunCallback(condition).ActReg0(proxy);
  }
}

void ProxyDetector(Automata* aut, OpCallback* condition,
                   GlobalVariable* detector_next, GlobalVariable* detector_far,
                   CtrlTrackInterface* binding) {
  ProxyOneDetector(aut, condition, binding->LookupNextDetector(),
                   detector_next);
  ProxyOneDetector(aut, condition, binding->LookupFarDetector(), detector_far);
}

void TurnoutBase::ProxyDetectors(Automata* aut) {
  const LocalVariable& state = aut->ImportVariable(*turnout_state_);
  // Passes if state == 0 (closed).
  auto closed_condition = NewCallback(&TurnoutDirectionCheck, state, false);
  // Passes if state == 1 (thrown).
  auto thrown_condition = NewCallback(&TurnoutDirectionCheck, state, true);
  ProxyDetector(aut, &closed_condition, detector_next_.get(),
                detector_far_.get(), side_closed_.binding());
  ProxyDetector(aut, &thrown_condition, detector_next_.get(),
                detector_far_.get(), side_thrown_.binding());
}

void DKW::ProxyDetectors(Automata* aut) {
  const LocalVariable& state = aut->ImportVariable(*turnout_state_);
  // Passes if state == 0 (closed).
  auto closed_condition = NewCallback(&TurnoutDirectionCheck, state, false);
  // Passes if state == 1 (thrown).
  auto thrown_condition = NewCallback(&TurnoutDirectionCheck, state, true);
  for (auto& r : routes_) {
    ProxyDetector(
        aut, r.state == DKW_STRAIGHT ? &thrown_condition : &closed_condition,
        points_[r.from].detector_next.get(),
        points_[r.from].detector_far.get(),
        points_[r.to].interface->binding());
  }
}

void TurnoutBase::TurnoutRoute(Automata* aut) {
  LocalVariable* any_route_setting_in_progress =
      aut->ImportVariable(tmp_route_setting_in_progress_.get());

  for (const auto& d : directions_) {
    Def().IfState(StInit).ActReg0(aut->ImportVariable(d.route)).ActReg0(
        aut->ImportVariable(d.route_pending));
  }
  const LocalVariable& state = aut->ImportVariable(*turnout_state_);
  // Passes if state == 0 (closed).
  auto closed_condition = NewCallback(&TurnoutDirectionCheck, state, false);
  // Passes if state == 1 (thrown).
  auto thrown_condition = NewCallback(&TurnoutDirectionCheck, state, true);

  for (const auto& d : directions_) {
    OpCallback* cb = nullptr;
    switch(d.state_condition) {
      case TURNOUT_CLOSED:
        cb = &closed_condition;
        break;
      case TURNOUT_THROWN:
        cb = &thrown_condition;
        break;
      case TURNOUT_DONTCARE:
        cb = nullptr;
        break;
    }
    SimulateRoute(aut, cb, d.from, d.to, any_route_setting_in_progress,
                  aut->ImportVariable(d.route_pending),
                  {d.route, any_route_set_.get()}, {});
  }
}

void DKW::DKWRoute(Automata* aut) {
  for (const auto& d : routes_) {
    Def()
        .IfState(StInit)
        .ActReg0(aut->ImportVariable(d.route_set.get()))
        .ActReg0(aut->ImportVariable(d.route_pending.get()));
  }
  for (const auto& d : routes_) {
    ClearAutomataVariables(aut);
    LocalVariable* any_route_setting_in_progress =
        aut->ImportVariable(tmp_route_setting_in_progress_.get());
    const LocalVariable& state = aut->ImportVariable(*turnout_state_);
    // Passes if state == 0 (closed).
    auto closed_condition = NewCallback(&TurnoutDirectionCheck, state, false);
    // Passes if state == 1 (thrown).
    auto thrown_condition = NewCallback(&TurnoutDirectionCheck, state, true);

    OpCallback* cb = nullptr;
    switch (d.state) {
      case DKW_STRAIGHT:
        cb = &thrown_condition;
        break;
      case DKW_CURVED:
        cb = &closed_condition;
        break;
    }
    SimulateRoute(aut, cb, points_[d.from].interface.get(),
                  points_[d.to].interface.get(), any_route_setting_in_progress,
                  aut->ImportVariable(d.route_pending.get()),
                  {d.route_set.get(), any_route_set_.get()}, {});
  }
}

void ClearAutomataVariables(Automata* aut) {
  aut->ClearUsedVariables();
}


void MagnetAutomataEntry(MagnetDef* def, Automata* aut) {
  HASSERT(def->aut_state.state != 0);
  aut->ClearUsedVariables();
  LocalVariable* current_state = aut->ImportVariable(def->current_state.get());
  const LocalVariable& command = aut->ImportVariable(*def->command);
  LocalVariable* set_0 = aut->ImportVariable(def->set_0);
  LocalVariable* set_1 = aut->ImportVariable(def->set_1);
  // Make output bits zero and reach a known state to release any magnets.
  Def()
      .IfState(StInit)
      .ActReg1(set_0)
      .ActReg0(set_0)
      .ActReg1(set_1)
      .ActReg0(set_1);
  // This will force an update at boot.
  Def()
      .IfState(StInit)
      .IfReg0(command)
      .ActReg1(current_state);
  Def()
      .IfState(StInit)
      .IfReg1(command)
      .ActReg0(current_state);
  // Pulls the magnet if we can get the lock.
  Def()
      .IfState(StBase)
      .IfTimerDone()
      .IfReg1(command)
      .IfReg0(*current_state)
      .ActState(def->aut_state)
      .ActTimer(1)
      .ActReg1(set_1);
  Def()
      .IfState(StBase)
      .IfTimerDone()
      .IfReg0(command)
      .IfReg1(*current_state)
      .ActState(def->aut_state)
      .ActTimer(1)
      .ActReg1(set_0);
  // Releases the magnets and the lock.
  Def()
      .IfState(def->aut_state)
      .IfTimerDone()
      .IfReg1(*set_1)
      .ActReg0(set_1)
      .ActReg1(current_state)
      .ActState(StBase);
  Def()
      .IfState(def->aut_state)
      .IfTimerDone()
      .IfReg1(*set_0)
      .ActReg0(current_state)
      .ActReg0(set_0)
      .ActState(StBase);
  // Safety: set magnets to zero.
  Def()
      .IfState(StBase)
      .ActReg0(set_0)
      .ActReg0(set_1);
}

void MagnetAutomataCouple(CoupledMagnetDef* def, Automata* aut) {
  aut->ClearUsedVariables();
  LocalVariable* current_state = aut->ImportVariable(def->current_state.get());
  const LocalVariable& remote_state = aut->ImportVariable(*def->original->current_state);
  LocalVariable* command = aut->ImportVariable(def->command.get());
  LocalVariable* remote_command = aut->ImportVariable(def->remote_command.get());
  LocalVariable* real_remote_command = aut->ImportVariable(def->original->command.get());

  bool remote_1 = !def->invert;
  bool remote_0 = def->invert;

  // Propagates remote command changes to the local shadowed state and local
  // command.
  Def().IfReg(*real_remote_command, remote_1).IfReg0(*remote_command)
      .ActReg1(remote_command).ActReg1(command);
  Def().IfReg(*real_remote_command, remote_0).IfReg1(*remote_command)
      .ActReg0(remote_command).ActReg0(command);

  // Propagates local command changes to the remote command.
  Def().IfReg1(*command).IfReg0(*remote_command)
      .ActReg1(remote_command).ActReg(real_remote_command, remote_1);
  Def().IfReg0(*command).IfReg1(*remote_command)
      .ActReg0(remote_command).ActReg(real_remote_command, remote_0);

  // Propagates remote state to local state.
  Def().IfReg(remote_state, remote_1).IfReg0(*current_state)
      .ActReg1(current_state);
  Def().IfReg(remote_state, remote_0).IfReg1(*current_state)
      .ActReg0(current_state);
}

void MagnetAutomataFinal(Automata* aut) {
  // This will make magnets only be pulled at tick times.
  Def().IfState(StBase).IfTimerDone().ActTimer(1);
}

MagnetCommandAutomata::MagnetCommandAutomata(Board* brd, const AllocatorPtr& parent_alloc)
    : alloc_(parent_alloc->Allocate("magnets", 64)), aut_("magnets", brd, this) {
  //AddAutomataPlugin(1, NewCallbackPtr(this, &FixedTurnout::FixTurnoutState));
  AddAutomataPlugin(100, NewCallbackPtr(&MagnetAutomataFinal));
}

void MagnetCommandAutomata::AddMagnet(MagnetDef* def) {
  def->current_state.reset(alloc_->Allocate(def->name_ + ".current_state"));
  def->command.reset(alloc_->Allocate(def->name_ + ".command"));
  // TODO(balazs.racz): Locked is ignored at the moment.
  def->owned_locked.reset(alloc_->Allocate(def->name_ + ".locked"));
  def->locked = def->owned_locked.get();
  AddAutomataPlugin(def->aut_state.state, NewCallbackPtr(&MagnetAutomataEntry, def));
}

void MagnetCommandAutomata::AddCoupledMagnet(CoupledMagnetDef* def) {
  def->current_state.reset(alloc_->Allocate(def->name_ + ".current_state"));
  def->command.reset(alloc_->Allocate(def->name_ + ".command"));
  // TODO(balazs.racz): Locked is ignored at the moment.
  def->remote_command.reset(alloc_->Allocate(def->name_ + ".remote_command"));
  def->locked = def->original->owned_locked.get();
  AddAutomataPlugin(2, NewCallbackPtr(&MagnetAutomataCouple, def));
}

MagnetDef::MagnetDef(MagnetCommandAutomata* aut, const string& name, GlobalVariable* closed, GlobalVariable* thrown)
    : set_0(closed), set_1(thrown), aut_state(aut->NewUserState()), name_(name) {
  aut->AddMagnet(this);
}

CoupledMagnetDef::CoupledMagnetDef(MagnetCommandAutomata* aut,
                                   const string& name, MagnetDef* orig,
                                   bool inv)
    : original(orig), invert(inv), name_(name) {
  aut->AddCoupledMagnet(this);
}

void TrainSchedule::HandleInit(Automata* aut) {
  Def().IfState(StBase)
      .ActReg0(aut->ImportVariable(last_set_reversed_.get()))
      .ActState(StWaiting);
}

void TrainSchedule::HandleBaseStates(Automata* aut) {
  auto* magnets_ready = aut->ImportVariable(magnets_ready_.get());
  Def().IfState(StRequestGreen)
      .IfReg0(current_block_request_green_)
      .ActReg1(&current_block_request_green_)
      .ActState(StGreenRequested);
  Def()
      .IfState(StGreenRequested)
      .IfReg0(current_block_request_green_)
      .IfReg0(current_block_route_out_)
      .ActState(StWaiting);
  Def().IfState(StGreenRequested)
      .IfReg1(current_block_route_out_)
      .ActTimer(2)
      .ActState(StGreenWait);
  Def()
      .IfState(StGreenWait)
      .IfTimerDone()
      .ActState(StRequestTransition);

  Def().IfState(StTransitionDone)
      .IfReg0(aut->ImportVariable(*is_moving_))
      .ActReg1(aut->ImportVariable(req_go_.get()));
  Def().IfState(StTransitionDone)
      .ActState(StWaiting);

  // TODO(balazs.racz): This can crash if the automata is in state is_moving_
  // and the operator removes all location permabits in an effort to stop the
  // train.
  Def().IfReg1(aut->ImportVariable(*is_moving_))
      .IfReg1(current_block_detector_)
      .ActReg1(aut->ImportVariable(req_stop_.get()));

  Def().IfState(StReadyToGo)
      .ActState(StTurnout)
      .ActReg0(magnets_ready);

  Def().IfState(StTurnout)
      .IfReg1(*magnets_ready)
      .ActState(StRequestGreen);

  if (reset_routes) {
    Def().IfState(StTurnout)
        .IfReg1(aut->ImportVariable(*reset_routes))
        .ActState(StWaiting);
  }

  // If we haven't transitioned out of StTurnout state previously, then we
  // reset the "todo" bit and go for another round of magnet setting.
  Def().IfState(StTurnout)
      .ActReg1(magnets_ready);
}

void TrainSchedule::SendTrainCommands(Automata *aut) {
  const auto& is_reversed = aut->ImportVariable(*is_reversed_);
  auto* last_set_reversed = aut->ImportVariable(last_set_reversed_.get());
  auto* req_start_train = aut->ImportVariable(req_go_.get());
  auto* req_stop_train = aut->ImportVariable(req_stop_.get());
  auto* is_moving = aut->ImportVariable(is_moving_.get());
  Def().ActSetId(train_node_id_);
  if (speed_var_) {
    // Variable speed.
    Def().IfReg1(*req_start_train)
        .ActGetValueToSpeed(aut->ImportVariable(*speed_var_), 0);
  } else {
    // Fixed speed.
    Def().IfReg1(*req_start_train)
        .ActLoadSpeed(true, 40);
  }
  Def().IfReg1(*req_start_train)
      .IfReg1(is_reversed)
      .ActSpeedReverse();
  // Copies is_reversed to last_set_reversed.
  Def().IfReg1(*req_start_train)
      .IfReg1(is_reversed)
      .ActReg1(last_set_reversed);
  Def().IfReg1(*req_start_train)
      .IfReg0(is_reversed)
      .ActReg0(last_set_reversed);
  Def().IfReg1(*req_start_train)
      .IfSetSpeed()
      .ActReg0(req_start_train)
      .ActReg1(is_moving);

  // If is reversed != last_set_reversed, then we send a train stop command
  // with the right direction bit. This should take care of startup train
  // reversing.
  Def().IfState(StWaiting)
      .IfReg1(is_reversed)
      .IfReg0(*last_set_reversed)
      .ActReg1(req_stop_train);
  Def().IfState(StWaiting)
      .IfReg0(is_reversed)
      .IfReg1(*last_set_reversed)
      .ActReg1(req_stop_train);

  Def().IfReg1(*req_stop_train)
      .ActLoadSpeed(true, 0);
  Def().IfReg1(*req_stop_train)
      .IfReg1(is_reversed)
      .ActSpeedReverse();
  // Copies is_reversed to last_set_reversed.
  Def().IfReg1(*req_stop_train)
      .IfReg1(is_reversed)
      .ActReg1(last_set_reversed);
  Def().IfReg1(*req_stop_train)
      .IfReg0(is_reversed)
      .ActReg0(last_set_reversed);

  Def().IfReg1(*req_stop_train)
      .IfSetSpeed()
      .ActReg0(req_stop_train)
      .ActReg0(is_moving);
}

void TrainSchedule::StopAndReverseAtStub(StubBlock* dest) {
  // This will set the need_reverse to 1 just before the move.
  auto* need_reverse = aut->ImportVariable(need_reverse_.get());
  MapCurrentBlockPermaloc(&dest->b_);

  Def().IfReg1(current_block_permaloc_)
      .ActImportVariable(dest->detector(),
                         current_block_detector_);

  Def()
      .IfReg1(aut->ImportVariable(*is_moving_))
      .IfReg1(current_block_permaloc_)
      .ActReg1(need_reverse);
  // This will flip the is_reversed permaloc once during the StWaiting state.
  // We make use of the fact that we are before any state transitions outwards
  // of the stub block.
  auto* is_reversed = aut->ImportVariable(is_reversed_.get());
  Def()
      .IfState(StWaiting)
      .IfReg1(current_block_permaloc_)
      .IfReg1(current_block_detector_)
      .IfReg1(*need_reverse)
      .ActReg0(need_reverse)
      .ActState(StBeforeReverseWait)
      .ActTimer(3);
  Def().IfState(StBeforeReverseWait)
      .IfReg1(current_block_permaloc_)
      .IfTimerDone()
      .IfReg0(*is_reversed)
      .ActReg1(is_reversed)
      .ActState(StReverseSendCommand);
  Def().IfState(StBeforeReverseWait)
      .IfReg1(current_block_permaloc_)
      .IfTimerDone()
      .IfReg1(*is_reversed)
      .ActReg0(is_reversed)
      .ActState(StReverseSendCommand);
  Def().IfState(StReverseSendCommand)
      .IfReg1(current_block_permaloc_)
      .ActSetId(train_node_id_)
      .ActLoadSpeed(true, 0);
  Def().IfState(StReverseSendCommand)
      .IfReg1(current_block_permaloc_)
      .IfReg1(*is_reversed)
      .ActSpeedReverse();

  // Copies is_reversed to last_set_reversed.
  Def().IfState(StReverseSendCommand)
      .IfReg1(current_block_permaloc_)
      .IfReg1(*is_reversed)
      .ActReg1(aut->ImportVariable(last_set_reversed_.get()));
  Def().IfState(StReverseSendCommand)
      .IfReg1(current_block_permaloc_)
      .IfReg0(*is_reversed)
      .ActReg0(aut->ImportVariable(last_set_reversed_.get()));

  Def().IfState(StReverseSendCommand)
      .IfReg1(current_block_permaloc_)
      .IfSetSpeed()
      .ActState(StReverseDoneWait)
      .ActTimer(3);
  Def().IfState(StReverseDoneWait)
      .IfReg1(current_block_permaloc_)
      .IfTimerDone()
      .ActState(StWaiting);
}

void TrainSchedule::MapCurrentBlockPermaloc(StandardBlock* source) {
  ScheduleLocation* loc = AllocateOrGetLocationByBlock(source);
  Def().ActImportVariable(*loc->permaloc(), current_block_permaloc_);
  Def().ActImportVariable(*loc->routingloc(), current_block_routingloc_);
  current_location_ = loc;
}

TrainSchedule::ScheduleLocation* TrainSchedule::AllocateOrGetLocation(
    const void* ptr, const string& name) {
  auto& loc = location_map_[ptr];
  if (!loc.permaloc_bit) {
    loc.permaloc_bit.reset(permanent_alloc_->Allocate("loc." + name));
  }
  if (!loc.routingloc_bit) {
    loc.routingloc_bit.reset(alloc_->Allocate("rtloc." + name));
  }
  return &loc;
}

GlobalVariable* TrainSchedule::GetHelperBit(
    const void* ptr, const string& name) {
  auto& loc = helper_bits_[ptr];
  if (!loc.get()) {
    loc.reset(alloc_->Allocate(name));
  }
  return loc.get();
}

void TrainSchedule::AddDirectBlockTransition(StandardBlock* source,
                                             StandardBlock* dest,
                                             OpCallback* condition,
                                             bool eager) {
  MapCurrentBlockPermaloc(source);
  Def().IfReg1(current_block_permaloc_)
      .ActImportVariable(source->route_out(),
                         current_block_route_out_)
      .ActImportVariable(source->detector(),
                         current_block_detector_);
  if (!eager) {
    aut->DefCopy(current_block_permaloc_, &current_block_routingloc_);
  } else {
    // This will take care of permaloc bits that appear out of nowhere.
    Def().IfReg1(current_block_permaloc_)
        .IfReg0(current_block_routingloc_)
        .IfReg0(current_block_route_out_)
        .ActReg1(&current_block_routingloc_);
    // We also need to delete routingloc bits that are not corresponding to
    // routes.
    Def()
        .IfReg1(current_block_routingloc_)
        .ActImportVariable(source->route_in(), next_block_route_in_);
    Def()
        .IfState(StWaiting)
        .IfReg1(current_block_routingloc_)
        .IfReg0(current_block_permaloc_)
        .IfReg0(next_block_route_in_)
        .ActReg0(&current_block_routingloc_);
  }

  Def().IfReg1(current_block_permaloc_)
      .ActImportVariable(*AllocateOrGetLocationByBlock(dest)->permaloc(),
                         next_block_permaloc_);

  // Transition permaloc to next step if eager.
  if (eager) {
    Def()
        .IfReg1(current_block_permaloc_)
        .IfReg1(current_block_detector_)
        .IfReg1(current_block_route_out_)
        .ActReg0(&current_block_permaloc_)
        .ActReg1(&next_block_permaloc_)
        // This will make sure not to stop the train here.
        .ActImportVariable(dest->detector(),
                           current_block_detector_);
  }

  Def().IfReg1(current_block_routingloc_)
      .ActImportVariable(*source->request_green(),
                         current_block_request_green_)
      .ActImportVariable(source->route_out(),
                         current_block_route_out_);
  Def()
      .IfReg1(current_block_routingloc_)
      .ActImportVariable(*AllocateOrGetLocationByBlock(dest)->routingloc(),
                         next_block_routingloc_)
      .ActImportVariable(dest->detector(), next_block_detector_)
      .ActImportVariable(dest->route_in(), next_block_route_in_);

  Def().IfState(StWaiting)
      .IfReg1(current_block_routingloc_)
      .MaybeIfReg(!eager, current_block_detector_, true)
      .IfReg0(next_block_detector_)
      .IfReg0(current_block_route_out_)
      .IfReg0(next_block_route_in_)
      .IfReg0(aut->ImportVariable(*is_frozen_))
      .RunCallback(condition)
      .RunCallback(route_lock_acquire())
      .ActState(StReadyToGo);

  // Handles the frozen request.
  Def().IfState(StWaiting)
      .IfReg1(aut->ImportVariable(*is_frozen_))
      .ActState(StFrozen);
  Def().IfState(StFrozen)
      .IfReg0(aut->ImportVariable(*is_frozen_))
      .ActState(StWaiting);

  // TODO(balazs.racz): this needs to be revised when we move from permaloc to
  // routingloc.
  if (eager) {
    Def().IfState(StRequestTransition)
        .IfReg1(current_block_routingloc_)
        .RunCallback(route_lock_release())
        .ActReg0(&current_block_routingloc_)
        .ActReg1(&next_block_routingloc_)
        .ActState(StTransitionDone);
  } else {
    Def().IfState(StRequestTransition)
        .IfReg1(current_block_permaloc_)
        .RunCallback(route_lock_release())
        .ActReg0(&current_block_permaloc_)
        .ActReg0(&current_block_routingloc_)
        .ActReg1(&next_block_permaloc_)
        .ActReg1(&next_block_routingloc_)
        .ActState(StTransitionDone);
  }
}

void TrainSchedule::AddBlockTransitionOnPermit(StandardBlock* source,
                                               StandardBlock* dest,
                                               RequestClientInterface* client,
                                               OpCallback* condition) {
  MapCurrentBlockPermaloc(source);
  Def().ActImportVariable(
      *GetHelperBit(client->request(),
                    "transition_" + source->name() + "_" + dest->name()),
      current_direction_);
  current_location_->respect_direction_ = true;

  Def().IfReg1(current_block_permaloc_)
      .ActImportVariable(source->route_out(),
                         current_block_route_out_);
  Def()
      .IfReg1(current_block_permaloc_)
      .IfReg0(current_block_route_out_)
      .ActReg1(&current_block_routingloc_);

  // NOTE(balazs.racz) This ensures that the outgoing route setting does not
  // become "eager". Namely, we ignore the advance arriving routingloc bit and
  // just use permaloc everywhere.
  aut->DefCopy(current_block_permaloc_, &current_block_routingloc_);

  Def().IfReg1(current_block_routingloc_)
      .ActImportVariable(*source->request_green(),
                         current_block_request_green_)
      .ActImportVariable(source->route_out(),
                         current_block_route_out_);
  Def().IfReg1(current_block_permaloc_)
      .ActImportVariable(*AllocateOrGetLocationByBlock(dest)->permaloc(),
                         next_block_permaloc_)
      .ActImportVariable(source->detector(),
                         current_block_detector_);
  Def()
      .IfReg1(current_block_routingloc_)
      .ActImportVariable(dest->detector(), next_block_detector_)
      .ActImportVariable(*AllocateOrGetLocationByBlock(dest)->routingloc(),
                         next_block_routingloc_)
      .ActImportVariable(dest->route_in(), next_block_route_in_);

  Def().IfState(StWaiting)
      .IfReg1(current_block_routingloc_)
      .ActState(StTestCondition);

  Def()
      .ActImportVariable(*client->request(), permit_request_)
      .ActImportVariable(*client->granted(), permit_granted_)
      .ActImportVariable(*client->taken(), permit_taken_);

  Def().IfState(StTestCondition)
      .IfReg1(current_block_routingloc_)
      .IfReg1(current_block_detector_)
      .IfReg0(next_block_detector_)
      .IfReg0(current_block_route_out_)
      .IfReg0(next_block_route_in_)
      .IfReg0(aut->ImportVariable(*is_frozen_))
      .RunCallback(condition)
      .ActState(StWaiting)
      .ActReg1(&permit_request_);

  Def().IfState(StTestCondition)
      .ActState(StWaiting)
      .ActReg0(&permit_request_);

  // Removes the permit request in case another outgoing direction took place.
  // TODO(balazs.racz): Check if this is a good condition to use.
  Def().IfReg1(current_block_routingloc_)
      .IfReg1(current_block_route_out_)
      .ActReg0(&permit_request_);

  Def().IfState(StWaiting)
      .IfReg1(current_block_routingloc_)
      .IfReg1(permit_request_)
      .IfReg1(permit_granted_)
      .RunCallback(route_lock_acquire())
      .ActReg0(&permit_request_)
      .ActReg0(&permit_granted_)
      .ActReg1(&permit_taken_)
      .ActReg1(&current_direction_)
      .ActState(StReadyToGo);

  // This should catch the case when we set an outgoing route that failed. Then
  // we need to remove the routing bit or we'll get into big trouble.
  Def().IfState(StWaiting)
      .IfReg1(current_block_routingloc_)
      .IfReg1(current_direction_)
      .ActReg0(&current_direction_);

  // TODO(balazs.racz): this needs to be revised when we move from permaloc to
  // routingloc.
  Def().IfState(StRequestTransition)
      .IfReg1(current_block_permaloc_)
      .IfReg1(current_direction_)
      .RunCallback(route_lock_release())
      .ActReg0(&current_block_permaloc_)
      .ActReg0(&current_block_routingloc_)
      .ActReg1(&next_block_permaloc_)
      .ActReg1(&next_block_routingloc_)
      .ActReg0(&current_direction_)
      .ActState(StTransitionDone);
}

void TrainSchedule::StopTrainAt(StandardBlock* dest) {
  MapCurrentBlockPermaloc(dest);
  Def().IfReg1(current_block_permaloc_)
      .ActImportVariable(dest->detector(),
                         current_block_detector_);
  // We never transition to RequestGreen, thus we don't need all the other
  // import variables either.
}

void TrainSchedule::AddCurrentOutgoingConditions(Automata::Op* op) {
  op->IfReg1(current_block_routingloc_);
  HASSERT(current_location_);
  if (current_location_->respect_direction_) {
    op->IfReg1(current_direction_);
  }
}

void TrainSchedule::SwitchTurnout(MagnetBase* magnet, bool desired_state) {
  auto* magnets_ready = aut->ImportVariable(magnets_ready_.get());
  Def().IfState(StTurnout).RunCallback(outgoing_route_conditions_.get())
      .ActImportVariable(*magnet->command, magnet_command_)
      .ActImportVariable(*magnet->current_state, magnet_state_)
      /* TODO(balazs.racz) locked is currently ignored by the magnet automata. */
      .ActImportVariable(*magnet->locked, magnet_locked_);
  Def().IfState(StTurnout).RunCallback(outgoing_route_conditions_.get())
      .IfReg(magnet_state_, !desired_state)
      .ActReg0(magnets_ready);
  Def().IfState(StTurnout).RunCallback(outgoing_route_conditions_.get())
      .IfReg(magnet_command_, !desired_state)
      .ActReg0(magnets_ready);
  Def().IfState(StTurnout).RunCallback(outgoing_route_conditions_.get())
      .IfReg0(magnet_locked_)
      .ActReg(&magnet_command_, desired_state);
}

const AllocatorPtr& FlipFlopAutomata::AddClient(FlipFlopClient* client) {
  clients_.push_back(client);
  return alloc_;
}

/*
void FlipFlopAutomata::FlipFlopLogic(Automata* aut) {
  HASSERT(clients_.size() >= 2);
  Def().IfState(StInit).ActState(clients_[0]->client_state);
  for (unsigned i = 0; i < clients_.size(); ++i) {
    auto* c = clients_[i];
    auto* request = aut->ImportVariable(c->request());
    auto* granted = aut->ImportVariable(c->granted());
    auto* taken = aut->ImportVariable(c->taken());
    Def().IfState(c->client_state).IfReg1(*request).ActReg1(granted);
    Def().IfReg1(*granted).IfReg0(*request).IfReg0(*taken)
        .ActReg0(granted);
    Def().IfReg1(*taken).ActReg0(taken).ActState(
        clients_[(i + 1) % clients_.size()]->client_state);
  }
  }*/

void FlipFlopAutomata::FlipFlopLogic(Automata* aut) {
  HASSERT(clients_.size() >= 2);
  Def().IfState(StInit).ActState(StFree);
  auto* steal_request = aut->ImportVariable(steal_request_.get());
  auto* steal_granted = aut->ImportVariable(steal_granted_.get());
  for (unsigned i = 0; i < clients_.size(); ++i) {
    auto* c = clients_[i];
    auto* request = aut->ImportVariable(c->request());
    auto* granted = aut->ImportVariable(c->granted());
    auto* taken = aut->ImportVariable(c->taken());
    auto* next = aut->ImportVariable(c->next.get());
    Def().IfState(StFree).ActState(StLocked).ActReg1(next);
    Def()
        .IfState(StLocked)
        .IfReg1(*next)
        .IfReg1(*request)
        .ActState(StGranted)
        .ActReg1(granted);
    // Request redacted:
    Def().IfState(StGranted).IfReg1(*next)
        .IfReg1(*granted).IfReg0(*request)
        .ActState(StLocked).ActReg0(granted);
    // Permission taken -> wait.
    Def()
        .IfState(StGranted)
        .IfReg1(*next)
        .IfReg1(*taken)
        .ActReg0(taken)
        .ActState(StGrantWait)
        .ActTimer(kWaitTimeout);
    Def()
        .IfState(StGrantWait)
        .IfReg1(*next)
        .IfTimerDone()
        .ActReg0(next)
        .ActState(StFree);

    // Stealing support.
    Def().IfState(StLocked)
        .IfReg0(*next)
        .IfReg1(*request)
        .ActReg1(steal_request);
    Def().IfState(StGrantWait)
        .ActReg0(steal_request)
        .ActReg0(steal_granted);
    Def().IfState(StLocked)
        .IfReg0(*next)
        .IfReg1(*request)
        .IfReg1(*steal_granted)
        .ActReg0(steal_request)
        .ActReg0(steal_granted)
        .ActReg1(granted)
        .ActState(StStolen);
    // stolen but redacted.
    Def().IfState(StStolen)
        .IfReg1(*granted)
        .IfReg0(*request)
        .ActReg0(granted)
        .ActState(StLocked);
    // stolen and taken
    Def().IfState(StStolen)
        .IfReg1(*taken)
        .ActReg0(taken)
        .ActTimer(kWaitTimeout)
        .ActState(StStolenWait);
    // Stealing interface from the current user's perspective.
    Def().IfState(StLocked)
        .IfReg1(*next)
        .IfReg0(*request)
        .IfReg1(*steal_request)
        .ActTimer(2)
        .ActState(StStealOnHold);
    Def().IfState(StStealOnHold)
        .IfReg1(*next)
        .IfReg0(*request)
        .IfTimerDone()
        .ActState(StLocked)
        .ActReg1(steal_granted);
    // not granting steal.
    Def().IfState(StStealOnHold)
        .IfReg1(*next)
        .IfReg1(*request)
        .ActTimer(0)
        .ActState(StLocked);
    // Taking back control after a successful steal.
    Def().IfState(StStolenWait)
        .IfReg1(*next)
        .IfTimerDone()
        .ActState(StLocked)
        .ActReg0(steal_request)
        .ActReg0(steal_granted);
  }
}

constexpr StateRef FlipFlopAutomata::StFree;
constexpr StateRef FlipFlopAutomata::StLocked;
constexpr StateRef FlipFlopAutomata::StGranted;
constexpr StateRef FlipFlopAutomata::StGrantWait;
constexpr StateRef FlipFlopAutomata::StStolen;
constexpr StateRef FlipFlopAutomata::StStolenWait;
constexpr StateRef FlipFlopAutomata::StStealOnHold;


}  // namespace automata
