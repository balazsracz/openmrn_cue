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
  MOCK_METHOD3(Read, bool(uint16_t arg, nmranet::Node* node, Automata* aut));
  MOCK_METHOD4(Write, void(uint16_t arg, nmranet::Node* node, Automata* aut, bool value));
  MOCK_METHOD1(GetState, uint8_t(uint16_t arg));
  MOCK_METHOD2(SetState, void(uint16_t arg, uint8_t state));
  MOCK_METHOD1(Initialize, void(nmranet::Node* node));
};


class AutomataTests : public nmranet::AsyncNodeTest {
 protected:
  class FakeBitPointer : public ReadWriteBit {
   public:
    FakeBitPointer(bool* backend)
        : backend_(backend) {}
    virtual ~FakeBitPointer() {}
    virtual bool Read(uint16_t, nmranet::Node* node, Automata* aut) {
      return *backend_;
    }
    virtual void Write(uint16_t, nmranet::Node* node, Automata* aut, bool value) {
      *backend_ = value;
    }
    void Initialize(nmranet::Node*) OVERRIDE {}

   private:
    bool* backend_;
  };

  class FakeBitReadOnlyPointer : public ReadWriteBit {
   public:
    FakeBitReadOnlyPointer(bool* backend)
        : backend_(backend) {}
    virtual bool Read(uint16_t, nmranet::Node* node, Automata* aut) {
      return *backend_;
    }
    virtual void Write(uint16_t, nmranet::Node* node, Automata* aut, bool value) {
      // Ignored.
    }
    void Initialize(nmranet::Node*) OVERRIDE {}

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
    fprintf(stderr, "program size: %d\n", current_program_.size());
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
    nmranet::AsyncNodeTest::wait();
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
  insn_t program_area_[10000];
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
    for (int i = 0; i < count; ++i) {
      wait_for_event_thread();
      extern int debug_variables;
      if ((i % 3) == 2) {
        if (debug_variables) fprintf(stderr, ",");
        runner_->AddPendingTick();
      } else {
        if (debug_variables) fprintf(stderr, ".");
      }
      runner_->RunAllAutomata();
    }
    wait_for_event_thread();
  }

  void SetVar(const automata::GlobalVariable& var, bool value) {
    uint64_t eventid = value ? var.event_on() : var.event_off();
    ProduceEvent(eventid);
  }

  friend class EventListener;

  class EventListener {
   public:
    EventListener(nmranet::Node* node, const automata::GlobalVariable& var)
        : memoryBit_(node, var.event_on(), var.event_off(), &value_, 1), consumer_(&memoryBit_), value_(0) {
    }

    bool value() {
      return value_ & 1;
    }

   private:
    nmranet::MemoryBit<uint8_t> memoryBit_;
    nmranet::BitEventConsumer consumer_;
    uint8_t value_;
  };

  // Returns the current value of the event based variable var. This is
  // independent of the internal event representation (state bits) and only
  // listens to OpenLCB events. If the variable is undefined, it is assumed to
  // be false (default state of internal bit).
  bool QueryVar(const automata::GlobalVariable& var) {
    return all_listener_.Query(var);
  }

  // Like QueryVar, but assert fails if the variable value is not defined yet.
  bool SQueryVar(const automata::GlobalVariable& var) {
    return all_listener_.StrictQuery(var);
  }

  void ProduceEvent(uint64_t event_id) {
    fprintf(stderr, "Producing event %016llx on node %p\n", event_id, node_);
    SyncNotifiable n;
    writeHelper_.WriteAsync(
        node_, nmranet::Defs::MTI_EVENT_REPORT, nmranet::WriteHelper::global(),
        nmranet::eventid_to_buffer(event_id), &n);
    n.wait_for_notification();
  }

 private:
  friend class GlobalEventListener;

  class GlobalEventListenerBase {
   public:
    GlobalEventListenerBase() : tick_(0) {}

    bool StrictQuery(const automata::GlobalVariable& var) {
      int t_on = event_last_seen_[var.event_on()];
      int t_off = event_last_seen_[var.event_off()];
      if (!t_on && !t_off) {
        fprintf(stderr,"tick %d, not seen: %llx and %llx (bit %llx)\n", tick_, var.event_on(), var.event_off(), (var.event_on() - BRACZ_LAYOUT - 0xc000)/2);
      }
      HASSERT(t_on || t_off);
      return t_on > t_off;
    }

    bool Query(const automata::GlobalVariable& var) {
      int t_on = event_last_seen_[var.event_on()];
      int t_off = event_last_seen_[var.event_off()];
      // If they are both zero, we return false.
      return t_on > t_off;
    }

   protected:
    void IncomingEvent(uint64_t event) {
      event_last_seen_[event] = ++tick_;
    }

   private:
    map<uint64_t, int> event_last_seen_;
    int tick_;
  };

  class GlobalEventListener : private nmranet::SimpleEventHandler, public GlobalEventListenerBase {
   public:
    GlobalEventListener() {
      nmranet::EventRegistry::instance()->register_handlerr(this, 0, 63);
    }

    ~GlobalEventListener() {
      nmranet::EventRegistry::instance()->unregister_handlerr(this, 0, 63);
    }
   private:
    virtual void HandleEventReport(nmranet::EventReport* event, BarrierNotifiable* done) {
      IncomingEvent(event->event);
      done->notify();
    }

    virtual void HandleIdentifyGlobal(nmranet::EventReport* event, BarrierNotifiable* done) {
      done->notify();
    }
  };

  GlobalEventListener all_listener_;
  nmranet::WriteHelper writeHelper_;
};

class TrainTestHelper {
 protected:
  TrainTestHelper(nmranet::AsyncIfTest* test_base)
      : testBase_(test_base),
        trainService_(test_base->ifCan_.get()), trainImpl_(0x1384) {
    expect_packet_(":X1070133AN060100001384;");
    expect_packet_(":X1910033AN060100001384;");
    test_base->create_allocated_alias();
    test_base->expect_next_alias_allocation();
    trainNode_.reset(new nmranet::TrainNode(&trainService_, &trainImpl_));
    test_base->wait();
  } 

  void expect_packet_(const string& gc_packet) {
    EXPECT_CALL(testBase_->canBus_, mwrite(StrCaseEq(gc_packet)));
  }

  ~TrainTestHelper() {
    testBase_->wait();
  }  

  nmranet::AsyncIfTest* testBase_;
  nmranet::TrainService trainService_;
  nmranet::LoggingTrain trainImpl_;
  std::unique_ptr<nmranet::TrainNode> trainNode_;
};

class AutomataTrainTest : public AutomataNodeTests, protected TrainTestHelper {
 protected:
  AutomataTrainTest() : TrainTestHelper(this) {}
};
