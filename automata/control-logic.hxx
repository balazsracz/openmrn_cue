// Declarations of unified structures used for control point logic, such as
// exlusive locks for blocks and turnouts, ABS, occupancy generation, etc.

#ifndef _AUTOMATA_CONTROL_LOGIC_HXX_
#define _AUTOMATA_CONTROL_LOGIC_HXX_

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"

using automata::EventBasedVariable;

struct PhysicalSignal {
  PhysicalSignal(const EventBasedVariable* sensor, EventBasedVariable* signal)
      : sensor_raw(sensor), signal_raw(signal) {}
  const EventBasedVariable* sensor_raw;
  EventBasedVariable* signal_raw;
};



class CtrlTrackInterface {
 public:
  CtrlTrackInterface(automata::Board* brd, uint64_t event_base, int counter)
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

  // Occupancy value of the current block.
  EventBasedVariable* in_close_occ;
  // Occupancy value of the block that is more than train-length away.
  EventBasedVariable* in_trainlength_occ;

  // The other interface connected to this track end.
  CtrlTrackInterface* binding;
};


class StraightTrack {
 public:
  StraightTrack(automata::Board* brd, uint64_t event_base, int counter_base)
    : side_a_(brd, event_base, counter_base),
      side_b_(brd, event_base + 16, counter_base + 8),
      simulated_occupancy_(brd, event_base + 32, event_base + 32 | 1,
                           counter_base + 16),
      route_set_ab_(brd, event_base + 32 + 2, event_base + 32 + 3,
                    counter_base + 17),
      route_set_ba_(brd, event_base + 32 + 4, event_base + 32 + 5,
                    counter_base + 18) {}

  void BindA(CtrlTrackInterface* opposite) {
    Bind(&side_a_, opposite);
  }
  void BindB(CtrlTrackInterface* opposite) {
    Bind(&side_b_, opposite);
  }

 protected:
  void Bind(CtrlTrackInterface* me, CtrlTrackInterface* opposite);

  CtrlTrackInterface side_a_;
  CtrlTrackInterface side_b_;

  EventBasedVariable simulated_occupancy_;
  // route from A to B
  EventBasedVariable route_set_ab_;
  // route from B to A
  EventBasedVariable route_set_ba_;
};

class StraightTrackWithDetector : public StraightTrack {
public:
  StraightTrackWithDetector(automata::Board* brd,
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


void SimulateOccupancy(automata::Automata::LocalVariable* target,
                       const automata::Automata::LocalVariable& route_set);
                       


#endif // _AUTOMATA_CONTROL_LOGIC_HXX_
