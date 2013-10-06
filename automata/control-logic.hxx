// Declarations of unified structures used for control point logic, such as
// exlusive locks for blocks and turnouts, ABS, occupancy generation, etc.

#ifndef _AUTOMATA_CONTROL_LOGIC_HXX_
#define _AUTOMATA_CONTROL_LOGIC_HXX_

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"

struct PhysicalSignal {
  PhysicalSignal(EventBasedVariable* sensor, EventBasedVariable* signal)
      : sensor_raw(sensor), signal_raw(signal) {}
  EventBasedVariable* sensor_raw;
  EventBasedVariable* signal_raw;
};



class CtrlTrackInterface {
 public:
  CtrlTrackInterface(uint64_t event_base,
                     int counter)
      : out_try_set_route(
            event_base | 0, event_base | 0 | 1,
            counter),
        in_route_set_success(
            event_base | 2, event_base | 2 | 1,
            counter + 1),
        in_route_set_success(
            event_base | 4, event_base | 4 | 1,
            counter + 2),
        out_route_released(
            event_base | 4, event_base | 6 | 1,
            counter + 3) {}


  EventBasedVariable out_try_set_route;
  EventBasedVariable in_route_set_success;
  EventBasedVariable in_route_set_failure;
  EventBasedVariable out_route_released;

  // Occupancy value of the current block.
  EventBasedVariable* in_close_occ;
  // Occupancy value of the block that is more than train-length away.
  EventBasedVariable* in_trainlength_occ;

  // The interface connected opposite to this track end.
  CtrlTrackInterface* opposite;
};


class StraightTrack {
 public:
  StraightTrack(uint64_t event_base, int counter_base)
      : side_a(event_base, counter_base),
        side_b(event_base + 16, counter_base + 8),
        simulated_occupancy(event_base + 32, counter_base + 16),
        route_set_ab(event_base + 32 + 2, counter_base + 17),
        route_set_ab(event_base + 32 + 4, counter_base + 18) {}

 protected:
  CtrlTrackInterface side_a;
  CtrlTrackInterface side_b;

  EventBasedVariable simulated_occupancy;
  // route from A to B
  EventBasedVariable route_set_ab;
  // route from B to A
  EventBasedVariable route_set_ba;
};

void BindTrack


#endif // _AUTOMATA_CONTROL_LOGIC_HXX_
