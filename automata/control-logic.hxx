// Declarations of unified structures used for control point logic, such as
// exlusive locks for blocks and turnouts, ABS, occupancy generation, etc.

#ifndef _AUTOMATA_CONTROL_LOGIC_HXX_
#define _AUTOMATA_CONTROL_LOGIC_HXX_

#include <map>
#include <algorithm>

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"
#include "gtest/gtest_prod.h"

class StandardBlock;

namespace automata {

extern StateRef StInit, StBase;

void HandleInitState(Automata* aut);

struct PhysicalSignal {
  PhysicalSignal(const GlobalVariable* sensor, GlobalVariable* signal)
      : sensor_raw(sensor), signal_raw(signal) {}
  const GlobalVariable* sensor_raw;
  GlobalVariable* signal_raw;
};

typedef Callback1<Automata*> AutomataCallback;

// This class allows registering multiple feature callbacks that will all be
// applied to an automata in one go, in user-specified ordering.
class AutomataPlugin {
 public:
  AutomataPlugin() {
    AddAutomataPlugin(10000, NewCallbackPtr(&HandleInitState));
  }
  virtual ~AutomataPlugin() {
    for (const auto& it : cb_) {
      delete it.second;
    }
  }

  void RunAll(Automata* aut) {
    for (const auto& it : cb_) {
      it.second->Run(aut);
    }
  }

  // The plugins will be called in increasing order of n.
  void AddAutomataPlugin(int n, AutomataCallback* cb) {
    cb_.insert(std::make_pair(n, cb));
  }

  // Removes all plugins with the given priority.
  void RemoveAutomataPlugins(int n) {
    auto range = cb_.equal_range(n);
    for (auto r = range.first; r != range.second; r++) {
      delete r->second;
    }
    cb_.erase(range.first, range.second);
  }

 private:
  std::multimap<int, AutomataCallback*> cb_;
};

// A straight automata definition that calls all the plugins.
class StandardPluginAutomata : public automata::Automata {
 public:
  StandardPluginAutomata(Board* brd, AutomataPlugin* plugins)
      : plugins_(plugins), brd_(brd) {
    brd->AddAutomata(this);
  }

  virtual void Body() {
    plugins_->RunAll(this);
  }

  virtual Board* board() { return brd_; }

 private:
  AutomataPlugin* plugins_;
  Board* brd_;
};

class CtrlTrackInterface;

// THis class is the casis of sequential binding. It defines a run of track
// which has exactly two endpoints. Internally it might be made of one or more
// teack pieces that are already bound.
class StraightTrackInterface {
 public:
  virtual CtrlTrackInterface* side_a() = 0;
  virtual CtrlTrackInterface* side_b() = 0;
};

// Bind together a sequence of straight tracks. The return value is always
// true.
bool BindSequence(const std::initializer_list<StraightTrackInterface*>& pieces);

// Interface for finding real occupancy detectors at a track piece.
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
            event_base | 6, event_base | 6 | 1,
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

class StraightTrack : public StraightTrackInterface,
                      public OccupancyLookupInterface,
                      public virtual AutomataPlugin {
 public:
  StraightTrack(Board* brd, uint64_t event_base, int counter_base)
      : side_a_(brd, this, event_base, counter_base),
        side_b_(brd, this, event_base + 8, counter_base + 4),
        simulated_occupancy_(brd, event_base + 16, event_base + 16 + 1,
                             counter_base + 8),
        route_set_ab_(brd, event_base + 16 + 2, event_base + 16 + 3,
                      counter_base + 8 + 1),
        route_set_ba_(brd, event_base + 16 + 4, event_base + 16 + 5,
                      counter_base + 8 + 2),
        route_pending_ab_(brd, event_base + 16 + 6, event_base + 16 + 7,
                      counter_base + 8 + 3),
        route_pending_ba_(brd, event_base + 16 + 8, event_base + 16 + 9,
                      counter_base + 8 + 4),
        tmp_seen_train_in_next_(brd, event_base + 16 + 10, event_base + 16 + 11,
                                counter_base + 8 + 5),
        tmp_route_setting_in_progress_(
            brd, event_base + 16 + 12, event_base + 16 + 13, counter_base + 8 + 6)
  {
    AddAutomataPlugin(20, NewCallbackPtr(
        this, &StraightTrack::SimulateAllOccupancy));
  }

  virtual CtrlTrackInterface* side_a() { return &side_a_; }
  virtual CtrlTrackInterface* side_b() { return &side_b_; }

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
  FRIEND_TEST(AutomataNodeTests, Signal);
  FRIEND_TEST(AutomataNodeTests, 100trainz);

  friend class ::StandardBlock;

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
      detector_(detector) {
    // No occupancy simulation needed.
    RemoveAutomataPlugins(20);
    AddAutomataPlugin(20, NewCallbackPtr(
        this, &StraightTrackWithDetector::DetectorOccupancy));
    AddAutomataPlugin(30, NewCallbackPtr(
        this, &StraightTrackWithDetector::DetectorRoute));
  }

  virtual const GlobalVariable* LookupCloseDetector(
      const CtrlTrackInterface* from) {
    return detector_;
  }

  // Returns the first occupancy detector outside of train length from the
  // interface `from'. Returns NULL if no such detector was found.
  virtual const GlobalVariable* LookupFarDetector(
      const CtrlTrackInterface* from) {
    // TODO(bracz): this should actually be propagated instead of NULLing.
    return NULL;
  }

protected:
  void DetectorRoute(Automata* aut);
  void DetectorOccupancy(Automata* aut);

  GlobalVariable* detector_;
};

class StraightTrackWithRawDetector : public StraightTrackWithDetector {
public:
  StraightTrackWithRawDetector(Board* brd,
                               uint64_t event_base, int counter_base,
                               const GlobalVariable* raw_detector)
      : StraightTrackWithDetector(brd, event_base, counter_base,
                                  &simulated_occupancy_),
        raw_detector_(raw_detector),
        debounce_temp_var_(brd, event_base + 16 + 14, event_base + 16 + 15, counter_base + 8 + 7) {
    RemoveAutomataPlugins(20);
    AddAutomataPlugin(20, NewCallbackPtr(
        this, &StraightTrackWithRawDetector::RawDetectorOccupancy));
  }

protected:
  void RawDetectorOccupancy(Automata* aut);

  const GlobalVariable* raw_detector_;
  EventBasedVariable debounce_temp_var_;
};

class StraightTrackShort : public StraightTrack {
public:
  StraightTrackShort(Board* brd,
                     uint64_t event_base, int counter_base)
    : StraightTrack(brd, event_base, counter_base) {
    AddAutomataPlugin(30, NewCallbackPtr(
        (StraightTrack*)this, &StraightTrack::SimulateAllRoutes));
  }

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
    : StraightTrack(brd, event_base, counter_base) {
    AddAutomataPlugin(30, NewCallbackPtr(
        this, &StraightTrack::SimulateAllRoutes));
  }

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

// A signal is a piece where trains coming from side_a towards side_b can (and
// shall) stop if the signal is red.
class SignalPiece : public StraightTrackShort {
 public:
  SignalPiece(Board* brd,
              uint64_t event_base, int counter_base,
              GlobalVariable* request_green,
              GlobalVariable* signal)
      : StraightTrackShort(brd, event_base, counter_base),
        request_green_(request_green),
        signal_(signal) {
    // We "override" the occupancy detection by just copying the occupancy
    // value from the previous piece of track.
    RemoveAutomataPlugins(20);
    AddAutomataPlugin(20, NewCallbackPtr(
        this, &SignalPiece::SignalOccupancy));
    // Signals have custom route setting logic in the forward direction.
    RemoveAutomataPlugins(30);
    AddAutomataPlugin(30, NewCallbackPtr(
        this, &SignalPiece::SignalRoute));
  }

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

  void SignalRoute(Automata* aut);
  void SignalOccupancy(Automata* aut);

 private:
  GlobalVariable* request_green_;
  GlobalVariable* signal_;
};

}  // namespace automata

#endif // _AUTOMATA_CONTROL_LOGIC_HXX_
