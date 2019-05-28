// Declarations of unified structures used for control point logic, such as
// exlusive locks for blocks and turnouts, ABS, occupancy generation, etc.

#ifndef _AUTOMATA_CONTROL_LOGIC_HXX_
#define _AUTOMATA_CONTROL_LOGIC_HXX_

#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <cstddef>
#include <stdint.h>

#include "system.hxx"
#include "operations.hxx"
#include "variables.hxx"
#include "gtest/gtest_prod.h"

#ifndef OVERRIDE
#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)
#define OVERRIDE override
#else
#define OVERRIDE
#endif
#endif

namespace automata {

static constexpr StateRef StInit(0);
static constexpr StateRef StBase(1);

extern GlobalVariable* reset_routes;

void HandleInitState(Automata *aut);

struct PhysicalSignal {
  PhysicalSignal(const GlobalVariable *sensor, GlobalVariable *signal,
                 SignalVariable *main_sgn, SignalVariable *adv_sgn,
                 SignalVariable *r_main_sgn, SignalVariable *r_adv_sgn,
                 SignalVariable *in_adv_sgn, SignalVariable *r_in_adv_sgn)
      : sensor_raw(sensor),
        signal_raw(signal),
        main_sgn(main_sgn),
        adv_sgn(adv_sgn),
        r_main_sgn(r_main_sgn),
        r_adv_sgn(r_adv_sgn),
        in_adv_sgn(in_adv_sgn),
        r_in_adv_sgn(r_in_adv_sgn) {}
  const GlobalVariable *sensor_raw;
  GlobalVariable *signal_raw;
  SignalVariable *main_sgn;
  SignalVariable *adv_sgn;
  SignalVariable *r_main_sgn;
  SignalVariable *r_adv_sgn;
  SignalVariable *in_adv_sgn;
  SignalVariable *r_in_adv_sgn;
};

typedef Callback1<Automata *> AutomataCallback;

void ClearAutomataVariables(Automata *aut);

// This class allows registering multiple feature callbacks that will all be
// applied to an automata in one go, in user-specified ordering.
class AutomataPlugin {
 public:
  AutomataPlugin() {
    AddAutomataPlugin(10000, NewCallbackPtr(&HandleInitState));
  }
  virtual ~AutomataPlugin() {
    for (const auto &it : cb_) {
      delete it.second;
    }
  }

  virtual bool Validate() { return true; }

  void RunAll(Automata *aut) {
    for (const auto &it : cb_) {
      it.second->Run(aut);
    }
  }

  // The plugins will be called in increasing order of n.
  void AddAutomataPlugin(int n, AutomataCallback *cb) {
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
  std::multimap<int, AutomataCallback *> cb_;
};

// A straight automata definition that calls all the plugins.
class StandardPluginAutomata : public automata::Automata {
 public:
  StandardPluginAutomata(const string &name, Board *brd,
                         AutomataPlugin *plugins)
      : automata::Automata(name), plugins_(plugins), brd_(brd) {
    brd->AddAutomata(this);
  }

  virtual void Body() { plugins_->RunAll(this); }
  virtual bool Validate() { return plugins_->Validate(); }

  virtual Board *board() { return brd_; }

 private:
  AutomataPlugin *plugins_;
  Board *brd_;
};

class MagnetCommandAutomata;

struct MagnetBase {
  // These variables are allocated by the magnetcommandautomata.

  // This variable sets the commanded state.
  std::unique_ptr<GlobalVariable> command;
  // This variable tracks the current state.
  std::unique_ptr<GlobalVariable> current_state;

  GlobalVariable* locked;
};

struct MagnetDef : public MagnetBase {
  MagnetDef(MagnetCommandAutomata *aut, const string &name,
            GlobalVariable *closed, GlobalVariable *thrown, bool def_state);
  // This variable will be pulled when the command is zero (closed).
  GlobalVariable *set_0;
  // This variable will be pulled when the command is one (thrown).
  GlobalVariable *set_1;

  // True if the current state must not be changed
  std::unique_ptr<GlobalVariable> owned_locked;
  // Initialized by the magnetcommandautomata.
  StateRef aut_state;
  bool default_state;
  string name_;
};

struct CoupledMagnetDef : public MagnetBase {
  CoupledMagnetDef(MagnetCommandAutomata *aut, const string &name,
                   MagnetDef* original, bool invert);
  MagnetDef* original;
  bool invert;

  // True if the current state must not be changed
  std::unique_ptr<GlobalVariable> owned_locked;

  // This variable shadows the remote command to determine the direction of
  // command propagation. In a steady state this should be == command.
  std::unique_ptr<GlobalVariable> remote_command;
  string name_;
};

class MagnetCommandAutomata : public virtual AutomataPlugin {
 public:
  MagnetCommandAutomata(Board *brd, const AllocatorPtr& parent_alloc);

  // Adds a magnet to this automata.
  void AddMagnet(MagnetDef *def);

  // Adds a coupled magnet to this automata.
  void AddCoupledMagnet(CoupledMagnetDef* def);

  int NewUserState() {
    return aut_.NewUserState();
  }

 private:
  AllocatorPtr alloc_;

  StandardPluginAutomata aut_;
};

class CtrlTrackInterface;

// This class is the basis of sequential binding. It defines a run of track
// which has exactly two endpoints. Internally it might be made of one or more
// track pieces that are already bound.
class StraightTrackInterface {
 public:
  virtual CtrlTrackInterface *side_a() = 0;
  virtual CtrlTrackInterface *side_b() = 0;
};

// Bind together a sequence of straight tracks. The return value is always
// true. before and after can be interfaces of turnouts for example. If they
// are NULL, the first straight track piece is taken.
bool BindSequence(
    CtrlTrackInterface* before,
    const std::initializer_list<StraightTrackInterface *> &pieces,
    CtrlTrackInterface* after = nullptr);

inline bool BindSequence(
    const std::initializer_list<StraightTrackInterface *> &pieces) {
  return BindSequence(nullptr, pieces, nullptr);
}

// Binds together pairs of interfaces. The return value is always true.
bool BindPairs(const std::initializer_list<
    std::initializer_list<CtrlTrackInterface *> > &pieces);

// Interface for finding real occupancy detectors at a track piece.
class OccupancyLookupInterface {
 public:
  virtual ~OccupancyLookupInterface() {}

  // Returns the first (real) occupancy detector within train length from the
  // interface `from'. Returns NULL if no such detector was found. The interface
  // is interpreted by
  virtual const GlobalVariable *LookupNextDetector(
      const CtrlTrackInterface *from) const = 0;

  // Returns the first occupancy detector outside of train length from the
  // interface `from'. Returns NULL if no such detector was found.
  virtual const GlobalVariable *LookupFarDetector(
      const CtrlTrackInterface *from) const = 0;

};

class CtrlTrackInterface {
 public:
  CtrlTrackInterface(AllocatorPtr allocator,
                     OccupancyLookupInterface *parent)
      : out_try_set_route(allocator->Allocate("out_try_set_route")),
        in_route_set_success(allocator->Allocate("in_route_set_success")),
        in_route_set_failure(allocator->Allocate("in_route_set_failure")),
        out_route_released(allocator->Allocate("out_route_released")),
        in_next_signal_1(allocator->Allocate("in_next_signal_1")),
        in_next_signal_2(allocator->Allocate("in_next_signal_2")),
        name_(allocator->name()),
        lookup_if_(parent),
        binding_(nullptr) {}

  std::unique_ptr<GlobalVariable> out_try_set_route;
  std::unique_ptr<GlobalVariable> in_route_set_success;
  std::unique_ptr<GlobalVariable> in_route_set_failure;
  std::unique_ptr<GlobalVariable> out_route_released;

  std::unique_ptr<GlobalVariable> in_next_signal_1;
  std::unique_ptr<GlobalVariable> in_next_signal_2;


  // Occupancy value of the current block, or the neighboring block less than a
  // train-length away.
  virtual const GlobalVariable *LookupNextDetector() const {
    return lookup_if_->LookupNextDetector(this);
  }
  // Occupancy value of the block that is more than train-length away.
  virtual const GlobalVariable *LookupFarDetector() const {
    return lookup_if_->LookupFarDetector(this);
  }

  CtrlTrackInterface *binding() const { return binding_; }

  bool Bind(CtrlTrackInterface *other) {
    if (binding_ != nullptr) {
      Debug("Changing binding on interface on %s from %s to %s.", name_.c_str(),
            binding_->name_.c_str(), other->name_.c_str());
    }

    binding_ = other;
    other->binding_ = this;
    return true;
  }

  void Unbind() {
    binding_->binding_ = nullptr;
    binding_ = nullptr;
  }
  
  bool Validate() {
    if (has_binding_error_) return false;
    if (!binding_) {
      Debug("Unbound interface on %s.", name_.c_str());
      return false;
    }
    return true;
  }
  
 private:
  // User-readable name of this interface
  const string name_;
  
  // Where to ask for occupancy lookups.
  OccupancyLookupInterface *lookup_if_;

  // The other interface connected to this track end.
  CtrlTrackInterface *binding_;

  // Whether we experienced a previous binding error that needs to be told the
  // validation.
  bool has_binding_error_{false};
};

void SimulateOccupancy(Automata *aut, Automata::LocalVariable *sim_occ,
                       Automata::LocalVariable *tmp_seen_train_in_next,
                       Automata::LocalVariable* tmp_shadow_train_in_far,
                       const Automata::LocalVariable &route_set,
                       CtrlTrackInterface *past_if, CtrlTrackInterface *next_if,
                       OpCallback *release_cb);

class StraightTrack : public StraightTrackInterface,
                      public OccupancyLookupInterface,
                      public virtual AutomataPlugin {
 public:
  StraightTrack(AllocatorPtr allocator)
      : side_a_(allocator->Allocate("a", 8), this),
        side_b_(allocator->Allocate("b", 8), this),
        simulated_occupancy_(allocator->Allocate("simulated_occ")),
        route_set_ab_(allocator->Allocate("route_set_ab")),
        route_set_ba_(allocator->Allocate("route_set_ba")),
        route_pending_ab_(allocator->Allocate("route_pending_ab")),
        route_pending_ba_(allocator->Allocate("route_pending_ba")),
        tmp_seen_train_in_next_(allocator->Allocate("tmp_seen_train_in_next")),
        tmp_route_setting_in_progress_(
            allocator->Allocate("tmp_route_setting_in_progress")),
        tmp_shadow_train_in_far_(
            allocator->Allocate("tmp_shadow_train_in_far")) {
    AddAutomataPlugin(
        20, NewCallbackPtr(this, &StraightTrack::SimulateAllOccupancy));
    AddAutomataPlugin(
        23, NewCallbackPtr(this, &StraightTrack::CopySignalState));
  }

  virtual CtrlTrackInterface *side_a() { return &side_a_; }
  virtual CtrlTrackInterface *side_b() { return &side_b_; }

  const GlobalVariable& route_set_ab() { return *route_set_ab_; }
  const GlobalVariable& route_set_ba() { return *route_set_ba_; }

  void SimulateAllOccupancy(Automata *aut);
  void SimulateAllRoutes(Automata *aut);
  void CopySignalState(Automata *aut);
  void CopyOccupancy(Automata *aut);

  static const int kAllocSize = 24;

  bool Validate() override {
    return side_a_.Validate() && side_b_.Validate();
  }

 protected:
  bool Bind(CtrlTrackInterface *me, CtrlTrackInterface *opposite);

  CtrlTrackInterface side_a_;
  CtrlTrackInterface side_b_;

  // If you give this side_a() it will return side_b() and vice versa.
  const CtrlTrackInterface *FindOtherSide(const CtrlTrackInterface *s) const;

  FRIEND_TEST(LogicTest, SimulatedOccupancy_SingleShortPiece);
  FRIEND_TEST(LogicTest, SimulatedOccupancy_MultipleShortPiece);
  FRIEND_TEST(LogicTest, SimulatedOccupancy_ShortAndLongPieces);
  FRIEND_TEST(LogicTest, SimulatedOccupancy_RouteSetting);
  FRIEND_TEST(LogicTest, SimulatedOccupancy_SimultSetting);
  FRIEND_TEST(LogicTest, ReverseRoute);
  FRIEND_TEST(LogicTest, MultiRoute);
  FRIEND_TEST(LogicTest, Signal0);
  FRIEND_TEST(LogicTest, Signal);
  FRIEND_TEST(LogicTest, 100trainz);
  FRIEND_TEST(LogicTest, DISABLED_100trainz);
  FRIEND_TEST(LogicTest, FixedTurnout);
  FRIEND_TEST(LogicTest, MovableTurnout);
  FRIEND_TEST(LogicTrainTest, ScheduleStraight);
  FRIEND_TEST(SampleLayoutLogicTrainTest, ScheduleStraight);
  FRIEND_TEST(SampleLayoutLogicTrainTest, ScheduleConditional);

  friend class StandardBlock;
  friend class StandardBidirBlock;
  friend class StandardMiddleSignal;
  friend class StubBlock;

  // If not-null, our occupancy is coming from an external variable -- typically
  // a neighboring sensor track.
  const GlobalVariable* external_occupancy_{nullptr};
  // true if we have (computed) that the current track is occupied
  std::unique_ptr<GlobalVariable> simulated_occupancy_;
  // route from A [in] to B [out]
  std::unique_ptr<GlobalVariable> route_set_ab_;
  // route from B [in] to A [out]
  std::unique_ptr<GlobalVariable> route_set_ba_;
  // temp var for route AB
  std::unique_ptr<GlobalVariable> route_pending_ab_;
  // temp var for route BA
  std::unique_ptr<GlobalVariable> route_pending_ba_;
  // Helper variable for simuating occupancy.
  std::unique_ptr<GlobalVariable> tmp_seen_train_in_next_;
  // Helper variable for excluding parallel route setting requests.
  std::unique_ptr<GlobalVariable> tmp_route_setting_in_progress_;
  // Another helper variable for simuating occupancy.
  std::unique_ptr<GlobalVariable> tmp_shadow_train_in_far_;
};

class StraightTrackWithDetector : public StraightTrack {
 public:
  StraightTrackWithDetector(AllocatorPtr allocator,
                            GlobalVariable *detector)
      : StraightTrack(std::move(allocator)), detector_(detector) {
    // No occupancy simulation needed.
    RemoveAutomataPlugins(20);
    AddAutomataPlugin(
        20,
        NewCallbackPtr(this, &StraightTrackWithDetector::DetectorOccupancy));
    AddAutomataPlugin(
        30, NewCallbackPtr(this, &StraightTrackWithDetector::DetectorRoute));
  }

  const GlobalVariable *LookupNextDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    return detector_;
  }

  // Returns the first occupancy detector outside of train length from the
  // interface `from'. Returns NULL if no such detector was found.
  const GlobalVariable *LookupFarDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    if (!FindOtherSide(from)->binding()) return nullptr;
    return FindOtherSide(from)->binding()->LookupFarDetector();
  }

 protected:
  void DetectorRoute(Automata *aut);
  void DetectorOccupancy(Automata *aut);

  GlobalVariable *detector_;
};

class StraightTrackWithRawDetector : public StraightTrackWithDetector {
 public:
  StraightTrackWithRawDetector(AllocatorPtr allocator,
                               const GlobalVariable *raw_detector,
                               int min_occupied_time = 0)
      : StraightTrackWithDetector(
            allocator->Forward(2),
            nullptr),
        raw_detector_(raw_detector),
        debounce_temp_var_(allocator->Allocate("tmp_debounce")),
        output_route_temp_var_(allocator->Allocate("tmp_outputroute")) {
    detector_ = simulated_occupancy_.get();
    RemoveAutomataPlugins(20);
    AddAutomataPlugin(
        18,
        NewCallbackPtr(this, &StraightTrackWithRawDetector::VirtualRouteOut));
    AddAutomataPlugin(19, NewCallbackPtr(&ClearAutomataVariables));
    AddAutomataPlugin(
        20, NewCallbackPtr(this,
                           &StraightTrackWithRawDetector::RawDetectorOccupancy,
                           min_occupied_time));
  }

  // Call this function with the outgoing route_ab_ variable for the same
  // logical block before rendering the automata. This is typically the signal
  // piece's route_ab. As a result the occupancy will never go low if there was
  // an input route unless output_route is set.
  void SetOutputRouteVariable(
      std::initializer_list<const GlobalVariable *> output_routes) {
    output_routes_ = output_routes;
  }
  
 protected:
  // Computes the output_route_temp_var value.
  void VirtualRouteOut(Automata *aut);
  void RawDetectorOccupancy(int min_occupied_time, Automata *aut);

  const GlobalVariable *raw_detector_;
  std::vector<const GlobalVariable *> output_routes_;
  std::unique_ptr<GlobalVariable> debounce_temp_var_;
  std::unique_ptr<GlobalVariable> output_route_temp_var_;
};

class StraightTrackShort : public StraightTrack {
 public:
  StraightTrackShort(AllocatorPtr allocator)
      : StraightTrack(std::move(allocator)) {
    AddAutomataPlugin(30, NewCallbackPtr((StraightTrack *)this,
                                         &StraightTrack::SimulateAllRoutes));
  }

  const GlobalVariable *LookupNextDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    if (!FindOtherSide(from)->binding()) return nullptr;
    return FindOtherSide(from)->binding()->LookupNextDetector();
  }

  // Returns the first occupancy detector outside of train length from the
  // interface `from'. Returns NULL if no such detector was found.
  const GlobalVariable *LookupFarDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    if (!FindOtherSide(from)->binding()) return nullptr;
    return FindOtherSide(from)->binding()->LookupFarDetector();
  }
};

class StraightTrackLong : public StraightTrack {
 public:
  StraightTrackLong(AllocatorPtr allocator)
      : StraightTrack(std::move(allocator)) {
    AddAutomataPlugin(30,
                      NewCallbackPtr(this, &StraightTrack::SimulateAllRoutes));
  }

  const GlobalVariable *LookupNextDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    return LookupFarDetector(from);
  }

  // Returns the first occupancy detector outside of train length from the
  // interface `from'. Returns NULL if no such detector was found.
  const GlobalVariable *LookupFarDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    if (!FindOtherSide(from)->binding()) return nullptr;
    return FindOtherSide(from)->binding()->LookupNextDetector();
  }
};

// A signal is a piece where trains coming from side_a towards side_b can (and
// shall) stop if the signal is red.
class SignalPiece : public StraightTrackShort {
 public:
  SignalPiece(AllocatorPtr allocator, GlobalVariable *request_green,
              GlobalVariable *signal, GlobalVariable *route_no_stop)
      : StraightTrackShort(std::move(allocator)),
        request_green_(request_green),
        signal_(signal),
        route_no_stop_(route_no_stop) {
    // We "override" the occupancy detection by just copying the occupancy
    // value from the previous piece of track.
    RemoveAutomataPlugins(20);
    AddAutomataPlugin(20, NewCallbackPtr(this, &SignalPiece::SignalOccupancy));

    RemoveAutomataPlugins(23);
    AddAutomataPlugin(23, NewCallbackPtr(this, &SignalPiece::SetSignalState));

    // Signals have custom route setting logic in the forward direction.
    RemoveAutomataPlugins(30);
    AddAutomataPlugin(30, NewCallbackPtr(this, &SignalPiece::SignalRoute));
  }

  const GlobalVariable *LookupNextDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    if (!FindOtherSide(from)->binding()) return nullptr;
    return FindOtherSide(from)->binding()->LookupNextDetector();
  }

  // Returns the first occupancy detector outside of train length from the
  // interface `from'. Returns NULL if no such detector was found.
  const GlobalVariable *LookupFarDetector(const CtrlTrackInterface *from) const
      OVERRIDE {
    if (!FindOtherSide(from)->binding()) return nullptr;
    // Signal pieces do not let the FarDetector signal through in the direction
    // of the signal through. Basically the body piece should not be able to
    // see a train beyond the signal, since that might be the previous train
    // that has passed here.
    if (from == &side_a_) return nullptr;
    return FindOtherSide(from)->binding()->LookupFarDetector();
  }

  void SignalRoute(Automata *aut);
  void SetSignalState(Automata *aut);
  void SignalOccupancy(Automata *aut);

 private:
  GlobalVariable *request_green_;
  // Output bit. Turns power on or off to the track.
  GlobalVariable *signal_;
  // Input bit. If set, the signal will not "stop" trains' route setting
  // commands, but propagate through.
  const GlobalVariable *route_no_stop_;
};

// Allows putting a signal into mainline that will never stop an automated
// train.
class StandardMiddleSignal : public StraightTrackInterface {
 public:
  StandardMiddleSignal(Board *brd, const AllocatorPtr &parent_alloc,
                       const string &base_name)
      : alloc_(parent_alloc->Allocate(base_name, 32)),
        base_name_(base_name),
        aut_signal_(name() + ".signal", brd, &signal_) {
    signal_.AddAutomataPlugin(
        131, NewCallbackPtr(this, &StandardMiddleSignal::InitSignalNoStop));
  }

  CtrlTrackInterface *side_a() override { return signal_.side_a(); }
  CtrlTrackInterface *side_b() override { return signal_.side_b(); }

  const GlobalVariable &route_out() const { return *signal_.route_set_ab_; }

  /** Returns the full name of the block (including path). */
  const string& name() {
    return alloc_->name();
  }

 private:
  void InitSignalNoStop(Automata* aut) {
    Def().ActReg1(aut->ImportVariable(no_stop_.get()));
  }

  AllocatorPtr alloc_;
  string base_name_;
  std::unique_ptr<GlobalVariable> request_green_{
      alloc_->Allocate("request_green")};
  std::unique_ptr<GlobalVariable> no_stop_{alloc_->Allocate("no_stop")};
  std::unique_ptr<GlobalVariable> signal_out_{alloc_->Allocate("signal_out")};

  SignalPiece signal_{alloc_->Allocate("signal", 24, 8), request_green_.get(),
                      signal_out_.get(),                 no_stop_.get()};

  StandardPluginAutomata aut_signal_;
};

// Abstract class that represents a single signal on the layout. This concept
// is used in the train schedules to mark where a train may need to stop.
class SignalBlock {
 public:
  // basename of the signal location.
  virtual const string& name() const = 0;

  // This detector goes from empty to occupied when the traion arrives at the
  // signal and needs to be stopped. Empty == 0, occupied == 1.
  virtual const GlobalVariable &dst_detector() const = 0;
  // This detector is occupied when a train is here and ready to leave.
  virtual const GlobalVariable &src_detector() const = 0;

  // The route variable for going beyond this signal (which sets the signal to
  // green).
  virtual const GlobalVariable &route_out() const = 0;

  // The incoming route variable; set when a train is coming into this block
  // (but is not yet occupying the block).
  virtual const GlobalVariable &route_in() const = 0;

  // Request to set the route_out to green.
  virtual GlobalVariable *request_green() const = 0;

  // This variable blocks automatically routed trains from entering the block.
  virtual const GlobalVariable& auto_blocked() const = 0;
};

class StandardBlock : public StraightTrackInterface {
 public:
  StandardBlock(Board *brd, PhysicalSignal *physical,
                const AllocatorPtr &parent_alloc, const string &base_name,
                int num_to_allocate = 112)
      : alloc_(parent_alloc->Allocate(base_name, num_to_allocate, 8)),
        base_name_(base_name),
        request_green_(alloc_->Allocate("request_green")),
        signal_no_stop_(alloc_->Allocate("signal_no_stop")),
        rrequest_green_(alloc_->Allocate("rev_request_green")),
        rsignal_no_stop_(alloc_->Allocate("rsignal_no_stop")),
        auto_blocked_(alloc_->Allocate("auto_blocked")),
        body_(alloc_->Allocate("body", 24, 8)),
        body_det_(alloc_->Allocate("body_det", 32, 8), physical->sensor_raw),
        signal_(alloc_->Allocate("signal", 24, 8), request_green_.get(),
                physical->signal_raw, signal_no_stop_.get()),
        rsignal_(alloc_->Allocate("rsignal", 24, 8), rrequest_green_.get(),
                 nullptr, rsignal_no_stop_.get()),
        physical_(physical),
        aut_body_(name() + ".body", brd, &body_),
        aut_body_det_(name() + ".body_det", brd, &body_det_),
        aut_signal_(name() + ".signal", brd, &signal_),
        aut_rsignal_(name() + ".rsignal", brd, &rsignal_) {
    BindSequence(rsignal_.side_a(), {&body_, &body_det_, &signal_});
    body_det_.SetOutputRouteVariable({&route_out(), &rev_route_out()});
  }

  operator const SignalBlock&() const {
    return fwd_signal;
  }

 protected:
  AllocatorPtr alloc_;
  string base_name_;
  friend class StubBlock;

  class FwdSignal : public SignalBlock {
   public:
    FwdSignal(StandardBlock* parent) : parent_(parent) {}
    const string& name() const override {
      return parent_->base_name();
    }

    const GlobalVariable &dst_detector() const override {
      return parent_->detector();
    }
    const GlobalVariable &src_detector() const override {
      return dst_detector();
    }
    const GlobalVariable &route_out() const override {
      return parent_->route_out();
    }
    const GlobalVariable &route_in() const override {
      return parent_->route_in();
    }
    GlobalVariable *request_green() const override {
      return parent_->request_green();
    }
    const GlobalVariable& auto_blocked() const override {
      return *parent_->auto_blocked();
    }

   private:
    StandardBlock* parent_;
  };

  class RevSignal : public SignalBlock {
   public:
    RevSignal(StandardBlock* parent) : parent_(parent) {}
    const string& name() const override {
      return parent_->base_name();
    }

    const GlobalVariable &dst_detector() const override {
      return parent_->detector();
    }
    const GlobalVariable &src_detector() const override {
      return dst_detector();
    }
    const GlobalVariable &route_out() const override {
      return parent_->rev_route_out();
    }
    const GlobalVariable &route_in() const override {
      return parent_->route_in();
    }
    GlobalVariable *request_green() const override {
      return parent_->rrequest_green();
    }
    const GlobalVariable& auto_blocked() const override {
      return *parent_->auto_blocked();
    }

   protected:
    StandardBlock* parent_;
  };

 public:
  CtrlTrackInterface *side_a() override { return rsignal_.side_b(); }
  CtrlTrackInterface *side_b() override { return signal_.side_b(); }

  const FwdSignal fwd_signal{this};
  const RevSignal rev_signal{this};

  /// @return variable to initiate the route reservation algorithm going
  /// outbounds of the forward signal.
  GlobalVariable *request_green() { return request_green_.get(); }
  /// @return variable to initiate the route reservation algorithm going
  /// outbounds of the reverse signal.
  GlobalVariable *rrequest_green() { return rrequest_green_.get(); }
  /// @return variable causing automatically routed trains to skip stopping at
  /// the forward signal (usually default false, i.e. trains stop).
  GlobalVariable *signal_no_stop() { return signal_no_stop_.get(); }
  /// @return variable causing automatically routed trains to skip stopping at
  /// the reverse signal (usually default true, i.e. trains do not stop).
  GlobalVariable *rsignal_no_stop() { return rsignal_no_stop_.get(); }
  /// @return variable causing automatically routed trains to avoid this block.
  GlobalVariable *auto_blocked() { return auto_blocked_.get(); }
  const GlobalVariable &route_in() const { return *body_det_.route_set_ab_; }
  const GlobalVariable &route_out() const { return *signal_.route_set_ab_; }
  const GlobalVariable &rev_route_out() const { return *rsignal_.route_set_ab_; }
  const GlobalVariable &detector() const { return *body_det_.simulated_occupancy_; }

  PhysicalSignal *p() { return physical_; }

  /** Returns the basename of the block (not including path). */
  const string& base_name() {
    return base_name_;
  }

  /** Returns the full name of the block (including path). */
  const string& name() {
    return alloc_->name();
  }

 private:
  std::unique_ptr<GlobalVariable> request_green_;
  std::unique_ptr<GlobalVariable> signal_no_stop_;
  std::unique_ptr<GlobalVariable> rrequest_green_;
  std::unique_ptr<GlobalVariable> rsignal_no_stop_;
  std::unique_ptr<GlobalVariable> auto_blocked_;

 public:
  StraightTrackLong body_;
  StraightTrackWithRawDetector body_det_;
  SignalPiece signal_;
  SignalPiece rsignal_;

 private:
  PhysicalSignal *physical_;

  StandardPluginAutomata aut_body_;
  StandardPluginAutomata aut_body_det_;
  StandardPluginAutomata aut_signal_;
  StandardPluginAutomata aut_rsignal_;

  DISALLOW_COPY_AND_ASSIGN(StandardBlock);
};

class StandardBidirBlock : public StandardBlock {
 public:
  StandardBidirBlock(Board *brd, PhysicalSignal *physical,
                     const GlobalVariable *rsensor_raw,
                     const AllocatorPtr &parent_alloc, const string &base_name,
                     int num_to_allocate = 144)
      : StandardBlock(brd, physical, parent_alloc, base_name, num_to_allocate),
        rbody_det_(alloc_->Allocate("rbody_det", 32, 8), rsensor_raw),
        aut_rbody_det_(name() + ".rbody_det", brd, &rbody_det_) {
    rsignal_.side_a()->Unbind();
    BindSequence(body_.side_a(), {&rbody_det_, &rsignal_});
    rbody_det_.SetOutputRouteVariable({&route_out(), &rev_route_out()});
  }

  class RevSignal : public StandardBlock::RevSignal {
   public:
    RevSignal(StandardBidirBlock* parent) : StandardBlock::RevSignal(parent) {}

    const GlobalVariable &dst_detector() const override {
      return parent()->rev_detector();
    }
    const GlobalVariable &src_detector() const override {
      return dst_detector();
    }
    
   private:
    StandardBidirBlock* parent() const {
      return static_cast<StandardBidirBlock*>(parent_);
    }
  };
  
 public:
  const RevSignal rev_signal{this};
  
  const GlobalVariable &rev_detector() const { return *rbody_det_.simulated_occupancy_; }

 private:
  StraightTrackWithRawDetector rbody_det_;
  StandardPluginAutomata aut_rbody_det_;

  DISALLOW_COPY_AND_ASSIGN(StandardBidirBlock);
};

class StandardMiddleDetector : public StraightTrackWithRawDetector {
 public:
  StandardMiddleDetector(Board *brd, const GlobalVariable *sensor_raw,
                         AllocatorPtr alloc)
      : StraightTrackWithRawDetector(alloc->Allocate("body_det", 32, 8),
                                     sensor_raw, 8 /*min_occupied_time*/),
        aut_(alloc->name() + ".det", brd, this) {}

 private:
  StandardPluginAutomata aut_;
};

class StandardMiddleLongTrack : public StraightTrackLong {
 public:
  StandardMiddleLongTrack(Board *brd, AllocatorPtr alloc)
      : StraightTrackLong(alloc->Allocate("body", 24, 8)),
        aut_(alloc->name() + ".body", brd, this) {}

 private:
  StandardPluginAutomata aut_;
};

class TurnoutInterface {
 public:
  virtual CtrlTrackInterface *side_points() = 0;
  virtual CtrlTrackInterface *side_closed() = 0;
  virtual CtrlTrackInterface *side_thrown() = 0;
};


class FakeStraightTrack : public StraightTrackInterface {
 public:
  FakeStraightTrack(CtrlTrackInterface *a, CtrlTrackInterface *b)
      : side_a_(a), side_b_(b) {}
  
  CtrlTrackInterface *side_a() OVERRIDE { return side_a_; }
  CtrlTrackInterface *side_b() OVERRIDE { return side_b_; }

 private:
  CtrlTrackInterface* side_a_;
  CtrlTrackInterface* side_b_;
};

class ReverseTrackWrap : public FakeStraightTrack {
 public:
  ReverseTrackWrap(StraightTrackInterface* track)
      : FakeStraightTrack(track->side_b(), track->side_a()) {}
};

// Allows a turnout be included in a BindSequence({tack, track, turnout, track,
// track})
class TurnoutWrap : public FakeStraightTrack {
 public:
  struct PointToClosed {};
  struct PointToThrown {};
  struct ClosedToPoint {};
  struct ThrownToPoint {};

  TurnoutWrap(TurnoutInterface* t, const PointToClosed&)
      : FakeStraightTrack(t->side_points(), t->side_closed()) {}

  TurnoutWrap(TurnoutInterface* t, const ClosedToPoint&)
      : FakeStraightTrack(t->side_closed(), t->side_points()) {}

  TurnoutWrap(TurnoutInterface* t, const ThrownToPoint&)
      : FakeStraightTrack(t->side_thrown(), t->side_points()) {}

  TurnoutWrap(TurnoutInterface* t, const PointToThrown&)
      : FakeStraightTrack(t->side_points(), t->side_thrown()) {}
};

constexpr TurnoutWrap::PointToClosed kPointToClosed;
constexpr TurnoutWrap::ClosedToPoint kClosedToPoint;
constexpr TurnoutWrap::PointToThrown kPointToThrown;
constexpr TurnoutWrap::ThrownToPoint kThrownToPoint;

class TurnoutBase : public TurnoutInterface,
                    private OccupancyLookupInterface,
                    public virtual AutomataPlugin {
 public:
  enum State { TURNOUT_CLOSED, TURNOUT_THROWN, TURNOUT_DONTCARE };

  TurnoutBase(AllocatorPtr allocator)
      : side_points_(allocator->Allocate("points", 8), this),
        side_closed_(allocator->Allocate("closed", 8), this),
        side_thrown_(allocator->Allocate("thrown", 8), this),
        simulated_occupancy_(allocator->Allocate("simulated_occ")),
        route_set_PC_(allocator->Allocate("route_set_PC")),
        route_set_CP_(allocator->Allocate("route_set_CP")),
        route_set_PT_(allocator->Allocate("route_set_PT")),
        route_set_TP_(allocator->Allocate("route_set_TP")),
        route_pending_PC_(allocator->Allocate("route_pending_PC")),
        route_pending_CP_(allocator->Allocate("route_pending_CP")),
        route_pending_PT_(allocator->Allocate("route_pending_PT")),
        route_pending_TP_(allocator->Allocate("route_pending_TP")),
        any_route_set_(allocator->Allocate("any_route_set")),
        turnout_state_(allocator->Allocate("turnout_state")),
        tmp_seen_train_in_next_(allocator->Allocate("tmp_seen_train_in_next")),
        tmp_route_setting_in_progress_(
            allocator->Allocate("tmp_route_setting_in_progress")),
        detector_next_(allocator->Allocate("detector_next")),
        detector_far_(allocator->Allocate("detector_far")),
        tmp_shadow_train_in_far_(
            allocator->Allocate("tmp_shadow_train_in_far"))
  {
    directions_.push_back(Direction(&side_points_, &side_closed_,
                                    route_set_PC_.get(),
                                    route_pending_PC_.get(), TURNOUT_CLOSED));
    directions_.push_back(Direction(&side_points_, &side_thrown_,
                                    route_set_PT_.get(),
                                    route_pending_PT_.get(), TURNOUT_THROWN));
    directions_.push_back(Direction(&side_closed_, &side_points_,
                                    route_set_CP_.get(),
                                    route_pending_CP_.get(), TURNOUT_DONTCARE));
    directions_.push_back(Direction(&side_thrown_, &side_points_,
                                    route_set_TP_.get(),
                                    route_pending_TP_.get(), TURNOUT_DONTCARE));
    AddAutomataPlugin(20, NewCallbackPtr(this, &TurnoutBase::TurnoutOccupancy));
    AddAutomataPlugin(23, NewCallbackPtr(this, &TurnoutBase::ProxySignals));
    AddAutomataPlugin(24, NewCallbackPtr(&ClearAutomataVariables));
    AddAutomataPlugin(25, NewCallbackPtr(this, &TurnoutBase::ProxyDetectors));
    AddAutomataPlugin(29, NewCallbackPtr(&ClearAutomataVariables));
    AddAutomataPlugin(30, NewCallbackPtr(this, &TurnoutBase::TurnoutRoute));
    AddAutomataPlugin(35,
                      NewCallbackPtr(this, &TurnoutBase::PopulateAnyRouteSet));
  }

  virtual CtrlTrackInterface *side_points() { return &side_points_; }
  virtual CtrlTrackInterface *side_closed() { return &side_closed_; }
  virtual CtrlTrackInterface *side_thrown() { return &side_thrown_; }

  const GlobalVariable *any_route() { return any_route_set_.get(); }

 protected:
  FRIEND_TEST(LogicTest, FixedTurnout);
  FRIEND_TEST(LogicTest, MovableTurnout);

  CtrlTrackInterface side_points_;
  CtrlTrackInterface side_closed_;
  CtrlTrackInterface side_thrown_;

  bool Validate() override {
    return side_points_.Validate() && side_closed_.Validate() &&
           side_thrown_.Validate();
  }
  
  struct Direction {
    Direction(CtrlTrackInterface *f, CtrlTrackInterface *t, GlobalVariable *r,
              GlobalVariable *rp, State state_cond)
        : from(f),
          to(t),
          route(r),
          route_pending(rp),
          state_condition(state_cond) {}
    CtrlTrackInterface *from;
    CtrlTrackInterface *to;
    GlobalVariable *route;
    GlobalVariable *route_pending;
    State state_condition;
  };

  vector<Direction> directions_;

  std::unique_ptr<GlobalVariable> simulated_occupancy_;
  // route from closed/thrown/points [in] to closed/thrown/points [out]
  std::unique_ptr<GlobalVariable> route_set_PC_;
  std::unique_ptr<GlobalVariable> route_set_CP_;
  std::unique_ptr<GlobalVariable> route_set_PT_;
  std::unique_ptr<GlobalVariable> route_set_TP_;
  // temp var for routes
  std::unique_ptr<GlobalVariable> route_pending_PC_;
  std::unique_ptr<GlobalVariable> route_pending_CP_;
  std::unique_ptr<GlobalVariable> route_pending_PT_;
  std::unique_ptr<GlobalVariable> route_pending_TP_;

  // Helper variable that is 1 iff any of the four routes are set.
  std::unique_ptr<GlobalVariable> any_route_set_;

  // Is zero if turnout is closed, 1 if turnout is thrown.
  std::unique_ptr<GlobalVariable> turnout_state_;

  // Helper variable for simuating occupancy.
  std::unique_ptr<GlobalVariable> tmp_seen_train_in_next_;
  // Helper variable for excluding parallel route setting requests.
  std::unique_ptr<GlobalVariable> tmp_route_setting_in_progress_;

  // The "close detector" bit looking from the points.
  std::unique_ptr<GlobalVariable> detector_next_;
  // The "far detector" bit looking from the points.
  std::unique_ptr<GlobalVariable> detector_far_;

  // Another helper variable for simuating occupancy.
  std::unique_ptr<GlobalVariable> tmp_shadow_train_in_far_;

 private:
  const GlobalVariable *LookupNextDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    if (from == &side_points_) {
      return detector_next_.get();
    } else {
      return side_points_.binding()->LookupNextDetector();
    }
  }

  const GlobalVariable *LookupFarDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    if (from == &side_points_) {
      return detector_far_.get();
    } else {
      return side_points_.binding()->LookupFarDetector();
    }
  }

  void PopulateAnyRouteSet(Automata *aut);
  void ProxyDetectors(Automata *aut);
  void ProxySignals(Automata *aut);
  void TurnoutOccupancy(Automata *aut);
  void TurnoutRoute(Automata *aut);
};

class MovableTurnout : public TurnoutBase {
 public:
  MovableTurnout(AllocatorPtr allocator, MagnetBase *def)
      : TurnoutBase(std::move(allocator)), magnet_(def) {
    AddAutomataPlugin(110, NewCallbackPtr(this, &MovableTurnout::CopyState));
    AddAutomataPlugin(120, NewCallbackPtr(this, &MovableTurnout::CopyLocked));
  }

  MagnetBase *magnet() { return magnet_; }

  static const bool kThrown = true;
  static const bool kClosed = false;

 private:
  // Copies the turnout state from the physical turnout's actual state.
  void CopyState(Automata *aut);

  // Copies the route set any bit to the magnet's locked bit.
  void CopyLocked(Automata *aut);

  MagnetBase *magnet_;
};

class FixedTurnout : public TurnoutBase {
 public:
  FixedTurnout(State state, AllocatorPtr allocator)
      : TurnoutBase(std::move(allocator)), state_(state) {
    AddAutomataPlugin(9, NewCallbackPtr(this, &FixedTurnout::FixTurnoutState));

    directions_.clear();
    directions_.push_back(Direction(&side_closed_, &side_points_,
                                    route_set_CP_.get(),
                                    route_pending_CP_.get(), TURNOUT_DONTCARE));
    directions_.push_back(Direction(&side_thrown_, &side_points_,
                                    route_set_TP_.get(),
                                    route_pending_TP_.get(), TURNOUT_DONTCARE));
    switch (state) {
      case TURNOUT_CLOSED:
        directions_.push_back(
            Direction(&side_points_, &side_closed_, route_set_PC_.get(),
                      route_pending_PC_.get(), TURNOUT_CLOSED));
        break;
      case TURNOUT_THROWN:
        directions_.push_back(
            Direction(&side_points_, &side_thrown_, route_set_PT_.get(),
                      route_pending_PT_.get(), TURNOUT_THROWN));
        break;
      case TURNOUT_DONTCARE:
        assert(false);
        break;
    }
  }

 private:
  void FixTurnoutState(Automata *aut);

  State state_;
};

class DKW : private OccupancyLookupInterface,
            public virtual AutomataPlugin {
 public:
  enum State { DKW_STRAIGHT = false, DKW_CURVED = true };

  static const bool kDKWStateCross = false;
  static const bool kDKWStateCurved = true;

  // The connections are as follows: if DKW_STRAIGHT, POINT_A1--POINT_B1 and
  // POINT_A2--POINT_B2. If DKW_CURVED, then A1--B2 and A2--B1.
  enum Point {
    POINT_A1,
    POINT_A2,
    POINT_B1,
    POINT_B2,
    POINT_MIN = POINT_A1,
    POINT_MAX = POINT_B2,
  };

  enum Route {
    ROUTE_MIN = 0,
    ROUTE_MAX = 7,
  };

  DKW(AllocatorPtr allocator)
      : routes_({RouteInfo(POINT_A1, POINT_B1, DKW_STRAIGHT),
                 RouteInfo(POINT_A1, POINT_B2, DKW_CURVED),
                 RouteInfo(POINT_A2, POINT_B2, DKW_STRAIGHT),
                 RouteInfo(POINT_A2, POINT_B1, DKW_CURVED),
                 RouteInfo(POINT_B1, POINT_A1, DKW_STRAIGHT),
                 RouteInfo(POINT_B1, POINT_A2, DKW_CURVED),
                 RouteInfo(POINT_B2, POINT_A2, DKW_STRAIGHT),
                 RouteInfo(POINT_B2, POINT_A1, DKW_CURVED)}) {
    for (int i = POINT_MIN; i <= POINT_MAX; ++i) {
      points_[i].interface.reset(new CtrlTrackInterface(
          allocator->Allocate(string("point_") + point_name(i), 8, 8), this));
    }
    for (int i = ROUTE_MIN; i <= ROUTE_MAX; ++i) {
      routes_[i].route_set.reset(allocator->Allocate(
          string("route_set_") + point_name(routes_[i].from) + "_" +
          point_name(routes_[i].to)));
    }
    for (int i = ROUTE_MIN; i <= ROUTE_MAX; ++i) {
      routes_[i].route_pending.reset(allocator->Allocate(
          string("route_pending_") + point_name(routes_[i].from) + "_" +
          point_name(routes_[i].to)));
    }
    for (int i = POINT_MIN; i <= POINT_MAX; ++i) {
      points_[i].detector_next.reset(
          allocator->Allocate("detector_next_" + point_name(i)));
      points_[i].detector_far.reset(
          allocator->Allocate("detector_far_" + point_name(i)));
    }
    any_route_set_.reset(allocator->Allocate("any_route_set"));
    simulated_occupancy_.reset(allocator->Allocate("simulated_occ"));
    turnout_state_.reset(allocator->Allocate("turnout_state"));
    tmp_seen_train_in_next_.reset(allocator->Allocate("tmp_seen_train_in_next"));
    tmp_route_setting_in_progress_.reset(
        allocator->Allocate("tmp_route_setting_in_progress"));
    tmp_shadow_train_in_far_.reset(allocator->Allocate("tmp_shadow_train_in_far"));

    AddAutomataPlugin(20, NewCallbackPtr(this, &DKW::DKWOccupancy));
    AddAutomataPlugin(23, NewCallbackPtr(this, &DKW::ProxySignals));
    AddAutomataPlugin(25, NewCallbackPtr(this, &DKW::ProxyDetectors));
    AddAutomataPlugin(29, NewCallbackPtr(&ClearAutomataVariables));
    AddAutomataPlugin(30, NewCallbackPtr(this, &DKW::DKWRoute));
    AddAutomataPlugin(35,
                      NewCallbackPtr(this, &DKW::PopulateAnyRouteSet));
  }

  string point_name(int p) {
    return point_name(static_cast<Point>(p));
  }

  string point_name(Point p) {
    switch (p) {
    case POINT_A1: return "A1";
    case POINT_A2: return "A2";
    case POINT_B1: return "B1";
    case POINT_B2: return "B2";
    default:
      HASSERT(0 && "Unknown point to name");
    }
  }

  CtrlTrackInterface* GetPoint(Point p) {
    return points_[p].interface.get();
  }

  CtrlTrackInterface* point_a1() { return GetPoint(POINT_A1); }
  CtrlTrackInterface* point_a2() { return GetPoint(POINT_A2); }
  CtrlTrackInterface* point_b1() { return GetPoint(POINT_B1); }
  CtrlTrackInterface* point_b2() { return GetPoint(POINT_B2); }

  bool Validate() override {
    return point_a1()->Validate() && point_a2()->Validate() &&
           point_b1()->Validate() && point_b2()->Validate();
  }

  Point FindPoint(const CtrlTrackInterface* from) const {
    for (int i = POINT_MIN; i <= POINT_MAX; ++i) {
      if (points_[i].interface.get() == from) return static_cast<Point>(i);
    }
    HASSERT(0 && "Tried to lookup an unknown point.");
  }

  const GlobalVariable *any_route() const { return any_route_set_.get(); }

  const GlobalVariable *LookupNextDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    auto* info = &points_[FindPoint(from)];
    return info->detector_next.get();
  }

  const GlobalVariable *LookupFarDetector(
      const CtrlTrackInterface *from) const OVERRIDE {
    auto* info = &points_[FindPoint(from)];
    return info->detector_far.get();
  }

protected:
  struct PointInfo {
    std::unique_ptr<CtrlTrackInterface> interface;
    // The "close detector" bit looking from the points.
    std::unique_ptr<GlobalVariable> detector_next;
    // The "far detector" bit looking from the points.
    std::unique_ptr<GlobalVariable> detector_far;
  };

  struct RouteInfo {
    RouteInfo(Point _from, Point _to, State _state)
        : from(_from), to(_to), state(_state) {}
    Point from;
    Point to;
    State state;
    std::unique_ptr<GlobalVariable> route_set;
    std::unique_ptr<GlobalVariable> route_pending;
  };

  unsigned get_route_number(Point from, Point to) {
    for (int i = ROUTE_MIN; i <= ROUTE_MAX; ++i) {
      if (routes_[i].from == from && routes_[i].to == to) {
        return i;
      }
    }
    HASSERT(0 && "requested unknown route");
  }

  RouteInfo* get_route(Point from, Point to) {
    return &routes_[get_route_number(from, to)];
  }

  void DKWOccupancy(Automata *aut);
  void PopulateAnyRouteSet(Automata *aut);
  void ProxyDetectors(Automata *aut);
  void ProxySignals(Automata *aut);
  void DKWRoute(Automata *aut);

  PointInfo points_[4];
  RouteInfo routes_[8];
  // Helper variable that is 1 iff any of the eight routes are set.
  std::unique_ptr<GlobalVariable> any_route_set_;
  std::unique_ptr<GlobalVariable> simulated_occupancy_;
  // Is zero if turnout is closed, 1 if turnout is thrown.
  std::unique_ptr<GlobalVariable> turnout_state_;
  // Helper variable for simuating occupancy.
  std::unique_ptr<GlobalVariable> tmp_seen_train_in_next_;
  // Helper variable for excluding parallel route setting requests.
  std::unique_ptr<GlobalVariable> tmp_route_setting_in_progress_;
  // Helper variable for simuating occupancy.
  std::unique_ptr<GlobalVariable> tmp_shadow_train_in_far_;


  FRIEND_TEST(LogicTest, FixedDKW);
  FRIEND_TEST(LogicTest, MovableDKW);
};

class DKWWrap : public FakeStraightTrack {
 public:
  struct WrapA1toB1 {};
  struct WrapB1toA1 {};
  struct WrapA2toB2 {};
  struct WrapB2toA2 {};

  struct WrapA2toB1 {};
  
  //
  //  B2--\                   /--A1
  //  B1---\----A1      B2---/---A2
  //        \---A2      B1--/
  // The connections are as follows: if DKW_STRAIGHT, POINT_A1--POINT_B1 and
  // POINT_A2--POINT_B2. If DKW_CURVED, then A1--B2 and A2--B1.

  DKWWrap(DKW* dkw, const WrapA1toB1&)
      : FakeStraightTrack(dkw->point_a1(), dkw->point_b1()) {}

  DKWWrap(DKW* dkw, const WrapB1toA1&)
      : FakeStraightTrack(dkw->point_b1(), dkw->point_a1()) {}

  DKWWrap(DKW* dkw, const WrapA2toB2&)
      : FakeStraightTrack(dkw->point_a2(), dkw->point_b2()) {}

  DKWWrap(DKW* dkw, const WrapB2toA2&)
      : FakeStraightTrack(dkw->point_b2(), dkw->point_a2()) {}

  DKWWrap(DKW* dkw, const WrapA2toB1&)
      : FakeStraightTrack(dkw->point_a2(), dkw->point_b1()) {}
};

constexpr DKWWrap::WrapA1toB1 kA1toB1;
constexpr DKWWrap::WrapB1toA1 kB1toA1;
constexpr DKWWrap::WrapA2toB2 kA2toB2;
constexpr DKWWrap::WrapB2toA2 kB2toA2;
constexpr DKWWrap::WrapA2toB1 kA2toB1;

class FixedDKW : public DKW {
public:
  FixedDKW(State state, AllocatorPtr allocator)
      : DKW(std::move(allocator)), fixed_state_(state) {
    AddAutomataPlugin(9, NewCallbackPtr(this, &FixedDKW::FixDKWState));
  }

  void FixDKWState(Automata *aut) {
    auto *turnoutstate = aut->ImportVariable(turnout_state_.get());
    if (fixed_state_ == DKW_STRAIGHT) {
      Def().ActReg(turnoutstate, kDKWStateCross);
    } else {
      Def().ActReg(turnoutstate, kDKWStateCurved);
    }
  }

  State fixed_state_;
};

class MovableDKW : public DKW {
 public:
  MovableDKW(AllocatorPtr allocator, MagnetBase *def)
      : DKW(std::move(allocator)), magnet_(def) {
    AddAutomataPlugin(110, NewCallbackPtr(this, &MovableDKW::CopyState));
    AddAutomataPlugin(120, NewCallbackPtr(this, &MovableDKW::CopyLocked));
  }

  MagnetBase *magnet() { return magnet_; }

 private:
  // Copies the turnout state from the physical turnout's actual state.
  void CopyState(Automata *aut) {
    auto *turnoutstate = aut->ImportVariable(turnout_state_.get());
    const auto &any_route_set = aut->ImportVariable(*any_route_set_);
    const auto &physical_state =
        aut->ImportVariable(*magnet_->current_state);
    Def().IfReg0(any_route_set).IfReg1(physical_state).ActReg1(turnoutstate);
    Def().IfReg0(any_route_set).IfReg0(physical_state).ActReg0(turnoutstate);
  }

  void CopyLocked(Automata* aut) {
    ClearAutomataVariables(aut);
    auto* locked = aut->ImportVariable(magnet_->locked);
    const auto& route_any = aut->ImportVariable(*any_route_set_);
    aut->DefCopy(route_any, locked);
  }

  MagnetBase *magnet_;
};

class StandardMovableDKW {
 public:
  StandardMovableDKW(Board *brd, AllocatorPtr alloc,
                     MagnetBase *magnet)
      : b(alloc->Forward(), magnet), aut_(alloc->name(), brd, &b) {}

  GlobalVariable *command() { return b.magnet()->command.get(); }
  const GlobalVariable &state() { return *b.magnet()->current_state; }

  MovableDKW b;

  // This variable can be used in BindSequence. It is one of the straight
  // connected pieces.
  FakeStraightTrack c1{b.point_a1(), b.point_b1()};
  // This variable can be used in BindSequence. It is the other of the straight
  // connected pieces. The "start" of c1 and c2 is the same side.
  FakeStraightTrack c2{b.point_a2(), b.point_b2()};
  
 private:
  StandardPluginAutomata aut_;
};

// for the moment we map the stub track into a fixed turnout and parts for a
// standardblock that is arranged as a loop from the closed to the thrown side.
class StubBlock {
 public:
  StubBlock(Board *brd, PhysicalSignal *physical,
            GlobalVariable *entry_sensor_raw, const AllocatorPtr &parent_alloc,
            const string &base_name, int num_to_allocate = 192)
      : b_(brd, physical, parent_alloc, base_name, num_to_allocate),
        fake_turnout_(FixedTurnout::TURNOUT_THROWN,
                      b_.alloc_->Allocate("fake_turnout", 40, 8)),
        aut_fake_turnout_(name() + ".fake_turnout", brd, &fake_turnout_) {
    if (entry_sensor_raw) {
      entry_det_.reset(new StraightTrackWithRawDetector(
          b_.alloc_->Allocate("entry_det", 32, 8), entry_sensor_raw));
      BindSequence(fake_turnout_.side_thrown(), {entry_det_.get(), &b_},
                   fake_turnout_.side_closed());
      aut_entry_det_.reset(new StandardPluginAutomata(name() + ".entry_det",
                                                      brd, entry_det_.get()));
    } else {
      BindSequence(fake_turnout_.side_thrown(), {&b_},
                   fake_turnout_.side_closed());
    }
  }

  operator const SignalBlock &() const { return b_.fwd_signal; }

 public:
  virtual CtrlTrackInterface *entry() { return fake_turnout_.side_points(); }

  GlobalVariable *request_green() { return b_.request_green(); }
  const GlobalVariable &route_in() const { return b_.route_in(); }
  const GlobalVariable &route_out() const { return b_.route_out(); }
  const GlobalVariable &detector() const { return b_.detector(); }
  const GlobalVariable &entry_detector() const {
    assert(entry_det_.get());
    return *entry_det_->simulated_occupancy_;
  }

  /** Returns the basename of the block (not including path). */
  const string& base_name() {
    return b_.base_name();
  }

  /** Returns the full name of the block (including path). */
  const string& name() {
    return b_.name();
  }

 public:
  StandardBlock b_;
 private:
  FixedTurnout fake_turnout_;
  std::unique_ptr<StraightTrackWithRawDetector> entry_det_;

 private:
  StandardPluginAutomata aut_fake_turnout_;
  std::unique_ptr<StandardPluginAutomata> aut_entry_det_;

  DISALLOW_COPY_AND_ASSIGN(StubBlock);
};

class StandardMovableTurnout {
 public:
  StandardMovableTurnout(Board *brd, AllocatorPtr alloc,
                         MagnetBase *magnet)
      : b(alloc->Forward(), magnet), aut_(alloc->name(), brd, &b) {}

  GlobalVariable *command() { return b.magnet()->command.get(); }
  const GlobalVariable &state() { return *b.magnet()->current_state; }

  MovableTurnout b;

 private:
  StandardPluginAutomata aut_;
};

class StandardFixedTurnout {
 public:
  StandardFixedTurnout(Board *brd, AllocatorPtr alloc,
                       FixedTurnout::State state)
      : b(state, alloc->Forward()), aut_(alloc->name(), brd, &b) {}

  FixedTurnout b;

 private:
  StandardPluginAutomata aut_;
};

// This is the interface between train automatas and flip-flop automatas that
// grant permission to proceed to alternating trains.
class RequestClientInterface {
 public:
  RequestClientInterface(const string &name,
                         const AllocatorPtr& parent_alloc)
      : name_(name),
        alloc_(parent_alloc->Allocate("client." + name, 3, 4)),
        request_(alloc_->Allocate("request")),
        granted_(alloc_->Allocate("granted")),
        taken_(alloc_->Allocate("taken")) {}

  // This bit will be set by the client (the train automata) if the train
  // routing is at the present location, and all other necessary conditions are
  // fulfilled for making the train progress. Other conditions include the
  // destination track being free, the intermediate blocks leading to the
  // destination track being all available, as well as any timer blocking the
  // train from proceeding being also fulfilled, etc.
  //
  // This bit will be cleared by the client when either the conditions do not
  // hold anymore, or when the granted permission is taken.
  GlobalVariable* request() { return request_.get(); }

  // This bit will be set by the flip-flop server automata. It will be cleared
  // by the server automata if it revokes the grant, or by the client automata
  // when it sets the taken bit.
  GlobalVariable* granted() { return granted_.get(); }

  // This bit will be set by the client when it sees that the permission is
  // granted and it removes the request bit. It will be cleared by the flipflop
  // automata together with the granted bit.
  GlobalVariable* taken() { return taken_.get(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestClientInterface);

  string name_;
  AllocatorPtr alloc_;
  std::unique_ptr<GlobalVariable> request_;
  std::unique_ptr<GlobalVariable> granted_;
  std::unique_ptr<GlobalVariable> taken_;
};

class FlipFlopClient;

class FlipFlopAutomata : public AutomataPlugin {
 public:
  FlipFlopAutomata(Board *brd, const string &name,
                   const AllocatorPtr &parent_alloc,
                   int num_to_allocate = 16)
      : name_(name),
        alloc_(parent_alloc->Allocate(name, num_to_allocate, 8)),
        aut_(name, brd, this),
        aut(&aut_),
        steal_request_(alloc_->Allocate("steal_request")),
        steal_granted_(alloc_->Allocate("steal_granted")) {
    AddAutomataPlugin(100,
                      NewCallbackPtr(this, &FlipFlopAutomata::FlipFlopLogic));
  }

  const AllocatorPtr& AddClient(FlipFlopClient* client);
  const AllocatorPtr& alloc() {
    return alloc_;
  }

  StateRef GetNewClientState() {
    return aut_.NewUserState();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FlipFlopAutomata);

  static constexpr StateRef StFree = 2;
  static constexpr StateRef StLocked = 3;
  static constexpr StateRef StGranted = 4;
  static constexpr StateRef StGrantWait = 5;
  static constexpr StateRef StStolen = 6;
  static constexpr StateRef StStolenWait = 7;
  static constexpr StateRef StStealOnHold = 8;
  static constexpr int kWaitTimeout = 3;

  void FlipFlopLogic(Automata* aut);

  string name_;
  AllocatorPtr alloc_;
  StandardPluginAutomata aut_;
  Automata* aut;
  std::unique_ptr<GlobalVariable> steal_request_;
  std::unique_ptr<GlobalVariable> steal_granted_;

  // Members are externally owned.
  vector<FlipFlopClient*> clients_;
};

struct FlipFlopClient : public RequestClientInterface {
 public:
  FlipFlopClient(const string &name, FlipFlopAutomata *parent)
      : RequestClientInterface(name, parent->AddClient(this)),
        client_state(parent->GetNewClientState()),
        next(parent->alloc()->Allocate("next." + name)) {}

 private:
  friend class FlipFlopAutomata;
  StateRef client_state;
  // Thjis variable will be set to 1 if the current client is the next-in-line
  // to give out preferential permission.
  std::unique_ptr<GlobalVariable> next;
  DISALLOW_COPY_AND_ASSIGN(FlipFlopClient);
};

// next state: 18.
static constexpr StateRef StWaiting(2);
static constexpr StateRef StReadyToGo(3);
static constexpr StateRef StRequestGreen(4);
static constexpr StateRef StGreenRequested(13);
static constexpr StateRef StGreenWait(5);
static constexpr StateRef StGreenFailed(6);

static constexpr StateRef StRequestTransition(9);
static constexpr StateRef StTransitionDone(10);

static constexpr StateRef StTurnout(11);
static constexpr StateRef StTurnoutFailed(7);
static constexpr StateRef StTestCondition(12);
static constexpr StateRef StTestCondition2(8);

static constexpr StateRef StBeforeReverseWait(14);
static constexpr StateRef StReverseSendCommand(15);
static constexpr StateRef StReverseDoneWait(16);
static constexpr StateRef StFrozen(17);

class TrainSchedule : public virtual AutomataPlugin {
 public:
  TrainSchedule(const string &name, Board *brd, uint64_t train_node_id,
                AllocatorPtr perm_alloc,
                AllocatorPtr alloc,
                GlobalVariable* speed_var = nullptr)
      : train_node_id_(train_node_id),
        permanent_alloc_(std::move(perm_alloc)),
        alloc_(std::move(alloc)),
        aut_(name, brd, this),
        aut(&aut_),
        is_reversed_(permanent_alloc_->Allocate("is_reversed")),
        last_set_reversed_(alloc_->Allocate("last_reversed")),
        is_frozen_(permanent_alloc_->Allocate("do_not_move")),
        current_block_detector_(aut_.ReserveVariable()),
        routing_block_request_green_(aut_.ReserveVariable()),
        perma_block_route_out_(aut_.ReserveVariable()),
        routing_block_route_out_(aut_.ReserveVariable()),
        current_block_permaloc_(aut_.ReserveVariable()),
        current_block_routingloc_(aut_.ReserveVariable()),
        current_direction_(aut_.ReserveVariable()),
        next_block_permaloc_(aut_.ReserveVariable()),
        next_block_routingloc_(aut_.ReserveVariable()),
        next_block_route_in_(aut_.ReserveVariable()),
        next_block_detector_(aut_.ReserveVariable()),
        next_block_blocked_(aut_.ReserveVariable()),
        magnet_command_(aut_.ReserveVariable()),
        magnet_state_(aut_.ReserveVariable()),
        magnet_locked_(aut_.ReserveVariable()),
        permit_request_(aut_.ReserveVariable()),
        permit_granted_(aut_.ReserveVariable()),
        permit_taken_(aut_.ReserveVariable()),
        need_reverse_(alloc_->Allocate("need_reverse")),
        req_stop_(alloc_->Allocate("req_stop")),
        req_go_(alloc_->Allocate("req_go")),
        is_moving_(alloc_->Allocate("is_moving")),
        outgoing_route_conditions_(
            NewCallbackPtr(this, &TrainSchedule::AddCurrentOutgoingConditions)),
        speed_var_(speed_var),
        current_location_(nullptr) {
    AddAutomataPlugin(9900, NewCallbackPtr(this, &TrainSchedule::HandleInit));
    AddAutomataPlugin(
        99, NewCallbackPtr(this, &TrainSchedule::ClearCurrentLocation));
    AddAutomataPlugin(100, NewCallbackPtr(this, &TrainSchedule::RunTransition));
    // 200 already needs the imported local variables
    AddAutomataPlugin(199, NewCallbackPtr(&ClearAutomataVariables));
    AddAutomataPlugin(200,
                      NewCallbackPtr(this, &TrainSchedule::HandleBaseStates));
    AddAutomataPlugin(210,
                      NewCallbackPtr(this, &TrainSchedule::SendTrainCommands));
  }

  // Performs the steps needed for the transition of train to the next location.
  virtual void RunTransition(Automata* aut) = 0;

  // ImportNextBlock -> sets localvariables based on the current state.
  // ImportLastBlock -> sets localvariables based on the current state.
  //

  GlobalVariable* TEST_GetPermalocBit(const SignalBlock& source) {
    size_t s = location_map_.size();
    auto* r = AllocateOrGetLocationByBlock(source)->permaloc();
    HASSERT(s == location_map_.size());
    return r;
  }

  GlobalVariable* TEST_GetRoutinglocBit(const SignalBlock& source) {
    size_t s = location_map_.size();
    auto* r = AllocateOrGetLocationByBlock(source)->permaloc();
    HASSERT(s == location_map_.size());
    return r;
  }

 protected:
  struct ScheduleLocation {
    ScheduleLocation()
        : respect_direction_(false) {}

    GlobalVariable* permaloc() {
      return permaloc_bit.get();
    }

    GlobalVariable* routingloc() {
      return routingloc_bit.get();
    }

    // This bit will be 1 if the train is right now physically in (or
    // approaching) this signal.
    std::unique_ptr<GlobalVariable> permaloc_bit;
    // Currently unused. Eventually this will be 1 if the automata is currently
    // setting the route from this block outwards.
    std::unique_ptr<GlobalVariable> routingloc_bit;

    // Number of outgoing transitions from this location. Updated during the
    // time of the first run.
    int num_directions_{0};

    // Specifies whether the direction bit should be taken into account as
    // well. If this is true, there are multiple outgoing possibilities from
    // this location, and each has its own direction bit. Automata transition
    // states will also have to match the current direction bit. If this is
    // false, the direction bit is not imported.
    bool respect_direction_;
  };

  // Makes a train transferfrom block 'source' to block 'dest'. The transition
  // happens when condition is true. If eager==false, then the transition
  // happens as soon as the train arrives physically (i.e. the permaloc == true
  // and occupied == true). If eager==true, then the outgoing route will be
  // requested whenever the location's routingloc==true and condition holds,
  // irrespectively of the permaloc. This means that the train will set the
  // route multiple blocks ahead.
  void AddDirectBlockTransition(const SignalBlock &source,
                                const SignalBlock &dest,
                                OpCallback *condition = nullptr,
                                bool eager = false);

  // Simplification of an "eager block transition", meaning open track with
  // multiple blocks one behind the other.
  void AddEagerBlockTransition(const SignalBlock &source,
                               const SignalBlock &dest) {
    extern CallbackSpec1_0<Automata::Op*> g_not_paused_condition;
    AddDirectBlockTransition(source, dest, &g_not_paused_condition, true);
  }

  // Adds an eager block transition between each successive pair of blocks in
  // this list.
  void AddEagerBlockSequence(
      const std::initializer_list<StandardBlock *> &blocks,
      OpCallback *condition = nullptr, bool is_eager = false) {
    auto ia = blocks.begin();
    if (ia == blocks.end()) return;
    auto ib = blocks.begin(); ++ib;
    while (ib != blocks.end()) {
      AddDirectBlockTransition((*ia)->fwd_signal, (*ib)->fwd_signal, condition,
                               is_eager);
      ++ia;
      ++ib;
    }
  }

  // Adds a block transition that, when the evaluated condition is true, will
  // communicate with a flipflop automata to request permission to
  // proceed. Only when the permission si granted, will the train proceed with
  // requesting green from the control point logic.
  void AddBlockTransitionOnPermit(const SignalBlock &source,
                                  const SignalBlock &dest,
                                  RequestClientInterface *client,
                                  OpCallback *condition = nullptr,
                                  bool eager = false);

  // Moves the train into a stub block and turns the direction around. This
  // should be called after the necessary turnout commands; nothing related to
  // the previous block should be left around anymore. This must also appear
  // before any state transitions outwards of the destination block.
  void StopAndReverseAtStub(const SignalBlock& dest);

  // Stops the train at the destination block. This is a terminal state and
  // probably should be used for testing only.
  void StopTrainAt(const SignalBlock& dest);

  // Before requesting green outbounds from this location, sets the particular
  // turnout to the desired state.
  //
  // @param magnet is the turnout to set.
  // @param desired_state if true, the turnout will be thrown, if false, will
  // be set to closed.
  void SwitchTurnout(MagnetBase* magnet, bool desired_state);

 private:
  // Handles state == StInit. Sets the train into a known state at startup,
  // reading the externally provided bits for figuring out where the train is.
  void HandleInit(Automata *aut);

  void ClearCurrentLocation(Automata* aut) {
    current_location_ = nullptr;
  }


  void SendTrainCommands(Automata *aut);
  void HandleBaseStates(Automata *aut);

  // This helper function will add all IfXxx conditions to a command that are
  // needed to restrict it to the currently set route. It will restrict on the
  // permaloc or routingloc, and the outgoing route bit if necessary.
  void AddCurrentOutgoingConditions(Automata::Op* op);

 protected:
  // Allocates the permaloc bit for the current block if it does not exist
  // yet. Maps the current_block_permaloc_ bit onto it.
  void MapCurrentBlockPermaloc(const SignalBlock& source);

  // Allocates the location bits for a particular block if does not exist;
  // returns the location structure. Does NOT transfer ownership.
  //
  // @param name references the location. Will also be used to mark the
  // location bits for debugging.
  ScheduleLocation* AllocateOrGetLocation(const string& name);

  // Allocates the location bits for a particular block if does not exist;
  // returns the location structure. Does NOT transfer ownership.
  ScheduleLocation* AllocateOrGetLocationByBlock(const SignalBlock& block) {
    return AllocateOrGetLocation(block.name());
  }

  OpCallback* route_lock_acquire() { return route_lock_acquire_.get(); }
  OpCallback* route_lock_release() { return route_lock_release_.get(); }

  // Allocates a new or returns an existing helper bit. id has to be different
  // for each different helper bit to be requested. name will be used in the
  // debugging name of the new bit.
  GlobalVariable* GetHelperBit(const void* id, const string& name);

  // The train that we are driving.
  uint64_t train_node_id_;

  // These bits should be used for non-reconstructible state.
  AllocatorPtr permanent_alloc_;
  // These bits should be used for transient state.
  AllocatorPtr alloc_;

  StandardPluginAutomata aut_;
  // Alias to the above object for using in Def().
  Automata* aut;

  // 1 if the train is currently in reverse mode (loco pushing at the end).
  std::unique_ptr<GlobalVariable> is_reversed_;
  // 1 if we have last set the train to reversed mode.
  std::unique_ptr<GlobalVariable> last_set_reversed_;

  // 1 if the train is requested not to be moved (by the operator).
  std::unique_ptr<GlobalVariable> is_frozen_;

  Automata::LocalVariable current_block_detector_;
  Automata::LocalVariable routing_block_request_green_;
  Automata::LocalVariable perma_block_route_out_;
  Automata::LocalVariable routing_block_route_out_;
  // The location bit for the current block will be mapped here.
  Automata::LocalVariable current_block_permaloc_;
  // The routing location for the current state. This bit will be 1 if the head
  // of the route set is at the current location.
  Automata::LocalVariable current_block_routingloc_;
  // If the current state transition is direction sensitive (that is,
  // current_location_->respect_direction_ == true), then this bit will be 1 if
  // we are at the chosen transition direction.
  Automata::LocalVariable current_direction_;

  // The location bit for the next block will be mapped here.
  Automata::LocalVariable next_block_permaloc_;
  Automata::LocalVariable next_block_routingloc_;
  Automata::LocalVariable next_block_route_in_;
  Automata::LocalVariable next_block_detector_;
  // Maps the destination location's auto_blocked() variable.
  Automata::LocalVariable next_block_blocked_;

  // These variables are used by magnet settings.
  Automata::LocalVariable magnet_command_;
  Automata::LocalVariable magnet_state_;
  Automata::LocalVariable magnet_locked_;

  // These variables are used by request-granted protocol.
  Automata::LocalVariable permit_request_;
  Automata::LocalVariable permit_granted_;
  Automata::LocalVariable permit_taken_;

  // Used for the turnaround logic to figure out if we still need to switch the
  // direction.
  std::unique_ptr<GlobalVariable> need_reverse_;

  // If 1, instructs the train control part of the automata to start the
  // engine.
  std::unique_ptr<GlobalVariable> req_stop_;
  // If 1, instructs the train control part of the automata to stop the
  // engine.
  std::unique_ptr<GlobalVariable> req_go_;
  // Defines whether the train is moving right now.
  std::unique_ptr<GlobalVariable> is_moving_;


  // Callback that will add all necessary checks for outgoing route setting.
  std::unique_ptr<OpCallback> outgoing_route_conditions_;

  // If non-null, specifies a variable where to load the speed data from.
  GlobalVariable* speed_var_;

  struct WithRouteLock {
    WithRouteLock(TrainSchedule* parent, GlobalVariable* v) : parent_(parent) {
      HASSERT(!parent->route_set_lock_var_);
      parent_->route_set_lock_var_ = v;
    }
    ~WithRouteLock() {
      parent_->route_set_lock_var_ = nullptr;
    }
   private:
    TrainSchedule* parent_;
  };

 private:
  void RouteLockAcquire(Automata::Op *op) {
    if (!route_set_lock_var_) return;
    op->IfReg0(op->parent()->ImportVariable(*route_set_lock_var_))
        .ActReg1(op->parent()->ImportVariable(route_set_lock_var_));
  }
  void RouteLockRelease(Automata::Op *op) {
    if (!route_set_lock_var_) return;
    op->ActReg0(op->parent()->ImportVariable(route_set_lock_var_));
  }

  std::unique_ptr<OpCallback> route_lock_acquire_{
      NewCallbackPtr(this, &TrainSchedule::RouteLockAcquire)};
  std::unique_ptr<OpCallback> route_lock_release_{
      NewCallbackPtr(this, &TrainSchedule::RouteLockRelease)};

  // If non-null, specifies a global lock to acquire before starting to set
  // routes, and release after the route setting is complete.
  GlobalVariable* route_set_lock_var_{nullptr};

  // This will be set temporarily as the processing goes down the body of
  // RunTransition. It always points to the location structure for the source
  // of the current transition.
  ScheduleLocation* current_location_;

  // This map goes from the detector bit of the current train location (aka
  // state) to the struct that holds the allocated train location bits.
  std::map<string, ScheduleLocation> location_map_;

  // These helper bits are used by turnout setting commands.
  std::map<string, std::unique_ptr<GlobalVariable> > helper_bits_;
};

}  // namespace automata

#endif  // _AUTOMATA_CONTROL_LOGIC_HXX_
