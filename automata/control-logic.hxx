// Declarations of unified structures used for control point logic, such as
// exlusive locks for blocks and turnouts, ABS, occupancy generation, etc.

#ifndef _AUTOMATA_CONTROL_LOGIC_HXX_
#define _AUTOMATA_CONTROL_LOGIC_HXX_

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"
#include "gtest/gtest_prod.h"

namespace automata {

extern StateRef StInit, StBase;

void HandleInitState(Automata* aut);

struct PhysicalSignal {
  PhysicalSignal(const GlobalVariable* sensor, GlobalVariable* signal)
      : sensor_raw(sensor), signal_raw(signal) {}
  const GlobalVariable* sensor_raw;
  GlobalVariable* signal_raw;
};

class CtrlTrackInterface;

class OccupancyLookupInterface {
public:
  virtual ~OccupancyLookupInterface() {}

  // Returns the first (real) occupancy detector within train length from the
  // interface `from'. Returns NULL if no such detector was found. The interface is interpreted by 
  virtual const GlobalVariable* LookupCloseDetector(
      const CtrlTrackInterface* from) = 0;

  // Returns the first occupancy detector outside of train length from the
  // interface `from'. Returns NULL if no such detector was found.
  virtual const GlobalVariable* LookupFarDetector(
      const CtrlTrackInterface* from) = 0;
};

class CtrlTrackInterface {
 public:
  CtrlTrackInterface(Board* brd, OccupancyLookupInterface* parent,
                     uint64_t event_base, int counter)
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
        lookup_if_(parent),
        binding_(nullptr) {}


  EventBasedVariable out_try_set_route;
  EventBasedVariable in_route_set_success;
  EventBasedVariable in_route_set_failure;
  EventBasedVariable out_route_released;

  const GlobalVariable* LookupNextDetector() const {
    const GlobalVariable* ret = LookupCloseDetector();
    if (ret != nullptr) {
      return ret;
    }
    ret = LookupFarDetector();
    if (ret != nullptr) {
      return ret;
    }
    extern bool could_not_retrieve_next_occupancy_variable();
    HASSERT(false && could_not_retrieve_next_occupancy_variable());
  }

  // Occupancy value of the current block, or the neighboring block less than a
  // train-length away.
  const GlobalVariable* LookupCloseDetector() const {
    return lookup_if_->LookupCloseDetector(this);
  }
  // Occupancy value of the block that is more than train-length away.
  const GlobalVariable* LookupFarDetector() const {
    return lookup_if_->LookupFarDetector(this);
  }

  CtrlTrackInterface* binding() const {
    return binding_;
  }

  void Bind(CtrlTrackInterface* other) {
    binding_ = other;
    other->binding_ = this;
  }

 private:
  // Where to ask for occupancy lookups.
  OccupancyLookupInterface* lookup_if_;

  // The other interface connected to this track end.
  CtrlTrackInterface* binding_;
};

void SimulateOccupancy(Automata* aut,
                       Automata::LocalVariable* sim_occ,
                       Automata::LocalVariable* tmp_seen_train_in_next,
                       const Automata::LocalVariable& route_set,
                       CtrlTrackInterface* past_if,
                       CtrlTrackInterface* next_if,\
                       OpCallback* release_cb);

class StraightTrack : public OccupancyLookupInterface {
 public:
  StraightTrack(Board* brd, uint64_t event_base, int counter_base)
      : side_a_(brd, this, event_base, counter_base),
        side_b_(brd, this, event_base + 16, counter_base + 8),
        simulated_occupancy_(brd, event_base + 32, event_base + 32 + 1,
                             counter_base + 16),
        route_set_ab_(brd, event_base + 32 + 2, event_base + 32 + 3,
                      counter_base + 17),
        route_set_ba_(brd, event_base + 32 + 4, event_base + 32 + 5,
                      counter_base + 18),
        route_pending_ab_(brd, event_base + 32 + 6, event_base + 32 + 7,
                      counter_base + 19),
        route_pending_ba_(brd, event_base + 32 + 8, event_base + 32 + 9,
                      counter_base + 20),
        tmp_seen_train_in_next_(brd, event_base + 32 + 10, event_base + 32 + 11,
                                counter_base + 21),
        tmp_route_setting_in_progress_(
            brd, event_base + 32 + 12, event_base + 32 + 13, counter_base + 22)
  {}

  CtrlTrackInterface* side_a() { return &side_a_; }
  CtrlTrackInterface* side_b() { return &side_b_; }

  void SimulateAllOccupancy(Automata* aut);
  void SimulateAllRoutes(Automata* aut);

 protected:
  void Bind(CtrlTrackInterface* me, CtrlTrackInterface* opposite);

  CtrlTrackInterface side_a_;
  CtrlTrackInterface side_b_;

  // If you give this side_a() it will return side_b() and vice versa.
  const CtrlTrackInterface* FindOtherSide(const CtrlTrackInterface* s);
  
  // This funciton will be called from SimulateOccupancy when the simulated
  // occupancy goes to zero and thus the route should be released.
  void ReleaseRouteCallback(CtrlTrackInterface* side_out,
                            Automata::LocalVariable* route_set,
                            Automata::Op* op);

  FRIEND_TEST(AutomataNodeTests, SimulatedOccupancy_SingleShortPiece);
  FRIEND_TEST(AutomataNodeTests, SimulatedOccupancy_MultipleShortPiece);
  FRIEND_TEST(AutomataNodeTests, SimulatedOccupancy_ShortAndLongPieces);
  FRIEND_TEST(AutomataNodeTests, SimulatedOccupancy_RouteSetting);
  FRIEND_TEST(AutomataNodeTests, SimulatedOccupancy_SimultSetting);
  FRIEND_TEST(AutomataNodeTests, ReverseRoute);
  FRIEND_TEST(AutomataNodeTests, MultiRoute);

  EventBasedVariable simulated_occupancy_;
  // route from A [in] to B [out]
  EventBasedVariable route_set_ab_;
  // route from B [in] to A [out]
  EventBasedVariable route_set_ba_;
  // temp var for route AB
  EventBasedVariable route_pending_ab_;
  // temp var for route BA
  EventBasedVariable route_pending_ba_;
  // Helper variable for simuating occupancy.
  EventBasedVariable tmp_seen_train_in_next_;
  // Helper variable for excluding parallel route setting requests.
  EventBasedVariable tmp_route_setting_in_progress_;
};

class StraightTrackWithDetector : public StraightTrack {
public:
  StraightTrackWithDetector(Board* brd,
                            uint64_t event_base, int counter_base,
                            GlobalVariable* detector)
    : StraightTrack(brd, event_base, counter_base),
      detector_(detector) {}

  virtual const GlobalVariable* LookupCloseDetector(
      const CtrlTrackInterface* from) {
    return detector_;
  }

  // Returns the first occupancy detector outside of train length from the
  // interface `from'. Returns NULL if no such detector was found.
  virtual const GlobalVariable* LookupFarDetector(
      const CtrlTrackInterface* from) {
    return NULL;
  }

protected:
  GlobalVariable* detector_;    
};


class StraightTrackShort : public StraightTrack {
public:
  StraightTrackShort(Board* brd,
                     uint64_t event_base, int counter_base)
    : StraightTrack(brd, event_base, counter_base) {}

  virtual const GlobalVariable* LookupCloseDetector(
      const CtrlTrackInterface* from) {
    return FindOtherSide(from)->binding()->LookupCloseDetector();
  }

  // Returns the first occupancy detector outside of train length from the
  // interface `from'. Returns NULL if no such detector was found.
  virtual const GlobalVariable* LookupFarDetector(
      const CtrlTrackInterface* from) {
    return FindOtherSide(from)->binding()->LookupFarDetector();
  }
};

class StraightTrackLong : public StraightTrack {
public:
  StraightTrackLong(Board* brd,
                     uint64_t event_base, int counter_base)
    : StraightTrack(brd, event_base, counter_base) {}

  virtual const GlobalVariable* LookupCloseDetector(
      const CtrlTrackInterface* from) {
    return nullptr;
  }

  // Returns the first occupancy detector outside of train length from the
  // interface `from'. Returns NULL if no such detector was found.
  virtual const GlobalVariable* LookupFarDetector(
      const CtrlTrackInterface* from) {
    return FindOtherSide(from)->binding()->LookupNextDetector();
  }
};


                       
}  // namespace automata

#endif // _AUTOMATA_CONTROL_LOGIC_HXX_
