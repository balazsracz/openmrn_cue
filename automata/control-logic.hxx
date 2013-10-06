// Declarations of unified structures used for control point logic, such as
// exlusive locks for blocks and turnouts, ABS, occupancy generation, etc.

#ifndef _AUTOMATA_CONTROL_LOGIC_HXX_
#define _AUTOMATA_CONTROL_LOGIC_HXX_

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"

namespace automata {

struct PhysicalSignal {
  PhysicalSignal(const EventBasedVariable* sensor, EventBasedVariable* signal)
      : sensor_raw(sensor), signal_raw(signal) {}
  const EventBasedVariable* sensor_raw;
  EventBasedVariable* signal_raw;
};

class CtrlTrackInterface {
 public:
  CtrlTrackInterface(Board* brd, uint64_t event_base, int counter)
      : out_try_set_route(
            brd,
            event_base | 0, event_base | 0 | 1,
            counter),
        in_route_set_success(
            brd,
            event_base | 2, event_base | 2 | 1,
            counter + 1),
        in_route_set_failure(
            brd,
            event_base | 4, event_base | 4 | 1,
            counter + 2),
        out_route_released(
            brd,
            event_base | 4, event_base | 6 | 1,
            counter + 3),
        in_close_occ(nullptr),
        in_trainlength_occ(nullptr),
        binding(nullptr) {}


  EventBasedVariable out_try_set_route;
  EventBasedVariable in_route_set_success;
  EventBasedVariable in_route_set_failure;
  EventBasedVariable out_route_released;

  const EventBasedVariable& in_next_occ() const {
    if (in_close_occ != nullptr) {
      return *in_close_occ;
    }
    if (in_trainlength_occ != nullptr) {
      return *in_trainlength_occ;
    }
    extern bool could_not_retrieve_next_occupancy_variable();
    HASSERT(false && could_not_retrieve_next_occupancy_variable());
  }

  // Occupancy value of the current block.
  EventBasedVariable* in_close_occ;
  // Occupancy value of the block that is more than train-length away.
  EventBasedVariable* in_trainlength_occ;

  // The other interface connected to this track end.
  CtrlTrackInterface* binding;
};

void SimulateOccupancy(Automata* aut,
                       Automata::LocalVariable* sim_occ,
                       Automata::LocalVariable* tmp_seen_train_in_next,
                       const Automata::LocalVariable& route_set,
                       CtrlTrackInterface* past_if,
                       CtrlTrackInterface* next_if);

class StraightTrack {
 public:
  StraightTrack(Board* brd, uint64_t event_base, int counter_base)
    : side_a_(brd, event_base, counter_base),
      side_b_(brd, event_base + 16, counter_base + 8),
      simulated_occupancy_(brd, event_base + 32, event_base + 32 + 1,
                           counter_base + 16),
      route_set_ab_(brd, event_base + 32 + 2, event_base + 32 + 3,
                    counter_base + 17),
      route_set_ba_(brd, event_base + 32 + 4, event_base + 32 + 5,
                    counter_base + 18),
      tmp_seen_train_in_next_(brd, event_base + 32 + 6, event_base + 32 + 7,
                              counter_base + 19)
  {}

  void BindA(CtrlTrackInterface* opposite) {
    Bind(&side_a_, opposite);
  }
  void BindB(CtrlTrackInterface* opposite) {
    Bind(&side_b_, opposite);
  }

  CtrlTrackInterface* side_a() { return &side_a_; }
  CtrlTrackInterface* side_b() { return &side_b_; }

  void SimulateAllOccupancy(Automata* aut) {
    auto* sim_occ = aut->ImportVariable(&simulated_occupancy_);
    auto* tmp = aut->ImportVariable(&tmp_seen_train_in_next_);
    SimulateOccupancy(aut,
                      sim_occ,
                      tmp,
                      aut->ImportVariable(route_set_ab_),
                      side_a(),
                        side_b());
    SimulateOccupancy(aut,
                      sim_occ,
                      tmp,
                      aut->ImportVariable(route_set_ba_),
                      side_b(),
                      side_a());
  }

 protected:
  void Bind(CtrlTrackInterface* me, CtrlTrackInterface* opposite);

  CtrlTrackInterface side_a_;
  CtrlTrackInterface side_b_;

  EventBasedVariable simulated_occupancy_;
  // route from A [in] to B [out]
  EventBasedVariable route_set_ab_;
  // route from B [in] to A [out]
  EventBasedVariable route_set_ba_;
  // Helper variable for simuating occupancy.
  EventBasedVariable tmp_seen_train_in_next_;
};

class StraightTrackWithDetector : public StraightTrack {
public:
  StraightTrackWithDetector(Board* brd,
                            uint64_t event_base, int counter_base,
                            EventBasedVariable* detector)
    : StraightTrack(brd, event_base, counter_base),
      detector_(detector) {
    side_a_.in_close_occ = detector;
    side_b_.in_close_occ = detector;
  }

protected:
  EventBasedVariable* detector_;    
};

                       
}  // namespace automata

#endif // _AUTOMATA_CONTROL_LOGIC_HXX_
