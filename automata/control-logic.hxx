// Declarations of unified structures used for control point logic, such as
// exlusive locks for blocks and turnouts, ABS, occupancy generation, etc.

#ifndef _AUTOMATA_CONTROL_LOGIC_HXX_
#define _AUTOMATA_CONTROL_LOGIC_HXX_

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"
#include "gtest/gtest_prod.h"

namespace automata {

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

  const CtrlTrackInterface* binding() const {
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
                       CtrlTrackInterface* next_if);

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
        tmp_seen_train_in_next_(brd, event_base + 32 + 6, event_base + 32 + 7,
                                counter_base + 19)
  {}

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

  FRIEND_TEST(AutomataNodeTests, SimulatedOccupancy_SingleShortPiece);
  FRIEND_TEST(AutomataNodeTests, SimulatedOccupancy_MultipleShortPiece);

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

 protected:
  const CtrlTrackInterface* FindOtherSide(const CtrlTrackInterface* s) {
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
};

                       
}  // namespace automata

#endif // _AUTOMATA_CONTROL_LOGIC_HXX_
