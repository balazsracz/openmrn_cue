#include "utils/macros.h"

#include "../automata/system.hxx"
#include "../automata/operations.hxx"
#include "../automata/variables.hxx"

#include "src/automata_runner.h"

#include "src/event_registry.hxx"
#include "src/automata_runner.h"

#include "nmranet_config.h"

#include "utils/gc_format.h"
#include "can_frame.h"

#include "utils/async_if_test_helper.hxx"

#include "nmranet/EventService.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/TractionTrain.hxx"
#include "nmranet/TractionTestTrain.hxx"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Mock;

class GMockBit : public ReadWriteBit {
 public:
  MOCK_METHOD3(Read, bool(uint16_t arg, openlcb::Node* node, Automata* aut));
  MOCK_METHOD4(Write, void(uint16_t arg, openlcb::Node* node, Automata* aut, bool value));
  MOCK_METHOD1(GetState, uint8_t(uint16_t arg));
  MOCK_METHOD2(SetState, void(uint16_t arg, uint8_t state));
  MOCK_METHOD1(Initialize, void(openlcb::Node* node));
};


class AutomataTests : public openlcb::AsyncNodeTest {
 protected:
  class FakeBitPointer : public ReadWriteBit {
   public:
    FakeBitPointer(bool* backend)
        : backend_(backend) {}
    virtual ~FakeBitPointer() {}
    virtual bool Read(uint16_t, openlcb::Node* node, Automata* aut) {
      return *backend_;
    }
    virtual void Write(uint16_t, openlcb::Node* node, Automata* aut, bool value) {
      *backend_ = value;
    }
    void Initialize(openlcb::Node*) OVERRIDE {}

   private:
    bool* backend_;
  };

  class FakeBitReadOnlyPointer : public ReadWriteBit {
   public:
    FakeBitReadOnlyPointer(bool* backend)
        : backend_(backend) {}
    virtual bool Read(uint16_t, openlcb::Node* node, Automata* aut) {
      return *backend_;
    }
    virtual void Write(uint16_t, openlcb::Node* node, Automata* aut, bool value) {
      // Ignored.
    }
    void Initialize(openlcb::Node*) OVERRIDE {}

   private:
    bool* backend_;
  };

  class InjectableVar : public automata::GlobalVariable {
   public:
    InjectableVar(AutomataTests* parent) {
      global_var_offset_ = parent->RegisterInjectableBit(this);
      SetId(global_var_offset_);
      fprintf(stderr, "new mock bit: %p\n", this);
    }

    ~InjectableVar() {
      fprintf(stderr, "destroying mock bit: %p\n", this);
    }

    /*virtual automata::GlobalVariableId GetId() const {
      automata::GlobalVariableId ret;
      ret.id = global_var_offset_;
      return ret;
      }*/

    virtual void Render(string* /*unused*/) {
      // Mock bits do not have any code in the board preamble.
    }

    // Creates a ReadWriteBit that accesses the mock bit stored in
    // *this. Transfers ownership of the object to the caller.
    virtual ReadWriteBit* CreateBit() = 0;

    int GetGlobalOffset() {
      return global_var_offset_;
    }
   private:
    int global_var_offset_;
  };

  class MockBit : public InjectableVar {
   public:
    MockBit(AutomataTests* parent) : InjectableVar(parent) {
      owned_mockbit_ = new StrictMock<GMockBit>;
      mock_bit_ = owned_mockbit_;
    }

    ~MockBit() {
      delete owned_mockbit_;
    }

    virtual ReadWriteBit* CreateBit() {
      owned_mockbit_ = NULL;
      return mock_bit_;
    }

    GMockBit& mock() {
      return *mock_bit_;
    }

    // These should never be called. This variable does not have eventids bound.
    virtual uint64_t event_on() const { HASSERT(false); }
    virtual uint64_t event_off() const { HASSERT(false); }

   private:
    GMockBit* owned_mockbit_;
    GMockBit* mock_bit_;
  };

  class FakeBit : public InjectableVar {
  public:
    FakeBit(AutomataTests* parent) : InjectableVar(parent), bit_(false) {}

    ~FakeBit() {
    }

    // Creates a ReadWriteBit that accesses the mock bit stored in
    // *this. Transfers ownership of the object to the caller. There can be an
    // arbitrary number of pointers, they will all modify the same state (also
    // accessed by Get and Set).
    virtual ReadWriteBit* CreateBit() {
      return new FakeBitPointer(&bit_);
    }

    bool Get() { return bit_; }
    void Set(bool b) { bit_ = b; }

    // These should never be called. This variable does not have eventids bound.
    virtual uint64_t event_on() const { HASSERT(false); }
    virtual uint64_t event_off() const { HASSERT(false); }

  private:
    bool bit_;
  };

  class FakeROBit : public InjectableVar {
  public:
    FakeROBit(AutomataTests* parent) : InjectableVar(parent), bit_(false) {}

    // Creates a ReadWriteBit that accesses the mock bit stored in
    // *this. Transfers ownership of the object to the caller. There can be an
    // arbitrary number of pointers, they will all modify the same state (also
    // accessed by Get and Set).
    virtual ReadWriteBit* CreateBit() {
      return new FakeBitReadOnlyPointer(&bit_);
    }

    bool Get() { return bit_; }
    void Set(bool b) { bit_ = b; }

    // These should never be called. This variable does not have eventids bound.
    virtual uint64_t event_on() const { HASSERT(false); }
    virtual uint64_t event_off() const { HASSERT(false); }

  private:
    bool bit_;
  };

  void SetupRunner(automata::Board* brd) {
    reset_all_state();
    current_program_.clear();
    brd->Render(&current_program_);
    fprintf(stderr, "program size: %zd\n", current_program_.size());
    HASSERT(sizeof(program_area_) >= current_program_.size());
    memcpy(program_area_, current_program_.data(), current_program_.size());
    runner_ = new AutomataRunner(node_, program_area_, false);
    for (auto bit : mock_bits_) {
      runner_->InjectBit(bit->GetGlobalOffset(), bit->CreateBit());
    }
  }

  AutomataTests() {
    automata::ClearEventMap();
    next_bit_offset_ = 4242;
    runner_ = NULL;
  }

  ~AutomataTests() {
    wait_for_event_thread();
    openlcb::AsyncNodeTest::wait();
    delete runner_;
  }

  // Registers a new variable for injection into the current runner. Returns a
  // unique global offset that this variable will be injected as.
  int RegisterInjectableBit(InjectableVar* var) {
    mock_bits_.push_back(var);
    return next_bit_offset_++;
  }

protected:
  AutomataRunner* runner_;
  insn_t program_area_[30000];
  string current_program_;

private:
  // Stores the global variable offset for the next mockbit.
  int next_bit_offset_;
  // Declared mock bits. Not owned.
  vector<InjectableVar*> mock_bits_;
};

class AutomataNodeTests : public AutomataTests {
protected:
  AutomataNodeTests() {
  }

  ~AutomataNodeTests() {
    wait_for_event_thread();
    AutomataTests::wait();
  }

  void Run(int count = 1) {
    extern int debug_variables;
    for (int i = 0; i < count; ++i) {
      wait_for_event_thread();
      if ((i % 3) == 2) {
        if (debug_variables) fprintf(stderr, ",");
        runner_->AddPendingTick();
      } else {
        if (debug_variables) fprintf(stderr, ".");
      }
      runner_->RunAllAutomata();
    }
    wait_for_event_thread();
    if (debug_variables) fprintf(stderr, "\n");
  }

  void SetVar(const automata::GlobalVariable& var, bool value) {
    uint64_t eventid = value ? var.event_on() : var.event_off();
    ProduceEvent(eventid);
  }

  friend class EventListener;

  class EventListener {
   public:
    EventListener(openlcb::Node* node, const automata::GlobalVariable& var)
        : memoryBit_(node, var.event_on(), var.event_off(), &value_, 1), consumer_(&memoryBit_), value_(0) {
    }

    bool value() {
      return value_ & 1;
    }

   private:
    openlcb::MemoryBit<uint8_t> memoryBit_;
    openlcb::BitEventConsumer consumer_;
    uint8_t value_;
  };

  // Returns the current value of the event based variable var. This is
  // independent of the internal event representation (state bits) and only
  // listens to OpenLCB events. If the variable is undefined, it is assumed to
  // be false (default state of internal bit).
  bool QueryVar(const automata::GlobalVariable& var) {
    return all_listener_.Query(var);
  }

  bool QueryVar(const string& var) {
    return all_listener_.Query(var);
  }

  // Like QueryVar, but assert fails if the variable value is not defined yet.
  bool SQueryVar(const automata::GlobalVariable& var) {
    return all_listener_.StrictQuery(var);
  }

  void ProduceEvent(uint64_t event_id) {
    fprintf(stderr, "Producing event %016" PRIx64 " on node %p\n", event_id, node_);
    SyncNotifiable n;
    writeHelper_.WriteAsync(
        node_, openlcb::Defs::MTI_EVENT_REPORT, openlcb::WriteHelper::global(),
        openlcb::eventid_to_buffer(event_id), &n);
    n.wait_for_notification();
  }

 private:
  friend class GlobalEventListener;

  class GlobalEventListenerBase {
   public:
    GlobalEventListenerBase() : tick_(0) {}

    bool StrictQuery(const automata::GlobalVariable& var) {
      return StrictQuery(var.event_on(), var.event_off());
    }

    bool StrictQuery(uint64_t event_on, uint64_t event_off) {
      int t_on = event_last_seen_[event_on];
      int t_off = event_last_seen_[event_off];
      if (!t_on && !t_off) {
        uint64_t v = (event_on - BRACZ_LAYOUT - 0xc000)/2;
        fprintf(stderr,"tick %d, not seen: %" PRIx64 " and %" PRIx64 " (bit %" PRIx64 ")\n", tick_, event_on, event_off, v);
      }
      HASSERT(t_on || t_off);
      return t_on > t_off;
    }

    const automata::RegisteredVariable& GetVarByName(const string& name) {
      auto& v = known_var_[name];
      if (v.name != name) {
        auto* p = automata::registered_variables();
        for (auto& entry : *p) {
          if (entry.name == name) {
            v = entry;
            return v;
          }
        }
        EXPECT_TRUE(false) << "Unknown variable name " << name;
      }
      return v;
    }

    bool Query(const string& name) {
      auto& v = GetVarByName(name);
      return Query(v.event_on, v.event_off);
    }

    bool Query(const automata::GlobalVariable& var) {
      return Query(var.event_on(), var.event_off());
    }

    bool Query(uint64_t event_on, uint64_t event_off) {
      int t_on = event_last_seen_[event_on];
      int t_off = event_last_seen_[event_off];
      // If they are both zero, we return false.
      return t_on > t_off;
    }

   protected:
    void IncomingEvent(uint64_t event) {
      event_last_seen_[event] = ++tick_;
    }

   private:
    map<uint64_t, int> event_last_seen_;
    map<string, automata::RegisteredVariable> known_var_;
    int tick_;
  };

  class GlobalEventListener : private openlcb::SimpleEventHandler, public GlobalEventListenerBase {
   public:
    GlobalEventListener() {
      openlcb::EventRegistry::instance()->register_handler(openlcb::EventRegistryEntry(this, 0), 63);
    }

    ~GlobalEventListener() {
      openlcb::EventRegistry::instance()->unregister_handler(this);
    }
   private:
    virtual void HandleEventReport(const openlcb::EventRegistryEntry&, openlcb::EventReport* event, BarrierNotifiable* done) {
      IncomingEvent(event->event);
      done->notify();
    }

    virtual void HandleIdentifyGlobal(const openlcb::EventRegistryEntry&, openlcb::EventReport* event, BarrierNotifiable* done) {
      done->notify();
    }
  };

  GlobalEventListener all_listener_;
  openlcb::WriteHelper writeHelper_;
};

class TrainTestHelper {
 protected:
  TrainTestHelper(openlcb::AsyncIfTest* test_base)
      : testBase_(test_base),
        trainService_(test_base->ifCan_.get()), trainImpl_(0x1384) {
    expect_packet_(":X1070133AN060100001384;");
    expect_packet_(":X1910033AN060100001384;");
    test_base->create_allocated_alias();
    test_base->expect_next_alias_allocation();
    trainNode_.reset(new openlcb::TrainNodeForProxy(&trainService_, &trainImpl_));
    test_base->wait();
  } 

  void expect_packet_(const string& gc_packet) {
    EXPECT_CALL(testBase_->canBus_, mwrite(StrCaseEq(gc_packet)));
  }

  ~TrainTestHelper() {
    testBase_->wait();
  }  

  openlcb::AsyncIfTest* testBase_;
  openlcb::TrainService trainService_;
  openlcb::LoggingTrain trainImpl_;
  std::unique_ptr<openlcb::TrainNode> trainNode_;
};

class AutomataTrainTest : public AutomataNodeTests, protected TrainTestHelper {
 protected:
  AutomataTrainTest() : TrainTestHelper(this) {}
};
