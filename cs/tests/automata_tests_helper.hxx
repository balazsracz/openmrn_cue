#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "utils/macros.h"

#include "../automata/system.hxx"
#include "../automata/operations.hxx"
#include "../automata/variables.hxx"

#include "src/automata_runner.h"

#include "src/event_registry.hxx"
#include "src/automata_runner.h"

#include "nmranet_config.h"
#include "core/nmranet_event.h"
#include "core/nmranet_datagram.h"
#include "if/nmranet_can_if.h"

#include "pipe/pipe.hxx"
#include "pipe/gc_format.h"
#include "nmranet_can.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Mock;


DEFINE_PIPE(can_pipe0, sizeof(struct can_frame));

const char *nmranet_manufacturer = "Stuart W. Baker";
const char *nmranet_hardware_rev = "N/A";
const char *nmranet_software_rev = "0.1";

const size_t main_stack_size = 2560;
const size_t ALIAS_POOL_SIZE = 2;
const size_t DOWNSTREAM_ALIAS_CACHE_SIZE = 2;
const size_t UPSTREAM_ALIAS_CACHE_SIZE = 2;
const size_t DATAGRAM_POOL_SIZE = 10;
const size_t CAN_RX_BUFFER_SIZE = 1;
const size_t CAN_TX_BUFFER_SIZE = 32;
const size_t SERIAL_RX_BUFFER_SIZE = 16;
const size_t SERIAL_TX_BUFFER_SIZE = 16;
const size_t DATAGRAM_THREAD_STACK_SIZE = 512;
const size_t CAN_IF_READ_THREAD_STACK_SIZE = 1024;


class GMockBit : public ReadWriteBit {
 public:
  MOCK_METHOD2(Read, bool(node_t node, Automata* aut));
  MOCK_METHOD3(Write, void(node_t node, Automata* aut, bool value));
};


class AutomataTests : public testing::Test {
 public:
  static void SetUpTestCase();


 protected:
  class FakeBitPointer : public ReadWriteBit {
   public:
    FakeBitPointer(bool* backend)
        : backend_(backend) {}
    virtual ~FakeBitPointer() {}
    virtual bool Read(node_t node, Automata* aut) {
      return *backend_;
    }
    virtual void Write(node_t node, Automata* aut, bool value) {
      *backend_ = value;
    }

   private:
    bool* backend_;
  };

  class InjectableVar : public automata::GlobalVariable {
   public:
    InjectableVar(AutomataTests* parent) {
      global_var_offset_ = parent->RegisterInjectableBit(this);
      fprintf(stderr, "new mock bit: %p\n", this);
    }

    ~InjectableVar() {
      fprintf(stderr, "destroying mock bit: %p\n", this);
    }

    virtual automata::GlobalVariableId GetId() const {
      automata::GlobalVariableId ret;
      ret.id = global_var_offset_;
      return ret;
    }

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

  private:
    bool bit_;
  };

  virtual void SetUp() {
  }

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
    next_bit_offset_ = 4242;
    runner_ = NULL;
    node_ = NULL;
  }

  ~AutomataTests() {
    delete runner_;
  }

  // Registers a new variable for injection into the current runner. Returns a
  // unique global offset that this variable will be injected as.
  int RegisterInjectableBit(InjectableVar* var) {
    mock_bits_.push_back(var);
    return next_bit_offset_++;
  }

protected:
  /* FakeBit* CreateFakeBit() {
    FakeBit* bit = new FakeBit(next_bit_offset_++);
    mock_bits_.push_back(bit);
    return bit;
    }*/

  static void* DispatchThread(void* arg);
  static bool dispatch_thread_waiting_;

  void WaitForEventThread() {
    while (nmranet_event_pending(node_) || !dispatch_thread_waiting_);
  }

  // A loopback interace that reads/writes to can_pipe0.
  static NMRAnetIF* nmranet_if_;
  static EventRegistry registry_;
  // This node will be given to the AutomataRunner in SetupRunner.
  node_t node_;
  AutomataRunner* runner_;
  insn_t program_area_[10000];
  string current_program_;

  void SetupStaticNode() {
    static node_t static_node = CreateNewNode();
    node_ = static_node;
  }

  node_t CreateNewNode() {
    node_t node = nmranet_node_create(0x02010d000003ULL, nmranet_if_,
                                      "Test Node2", NULL);
    fprintf(stderr,"node_=%p\n", node);
    nmranet_node_user_description(node, "Test Node2");
    nmranet_node_initialized(node);
    os_thread_t thread;
    os_thread_create(&thread, "event_process_thread",
                     0, 2048, &AutomataTests::DispatchThread,
                     node);
    return node;
  }

private:
  // Stores the global variable offset for the next mockbit.
  int next_bit_offset_;
  // Declared mock bits. Not owned.
  vector<InjectableVar*> mock_bits_;
};


NMRAnetIF* AutomataTests::nmranet_if_ = NULL;
EventRegistry AutomataTests::registry_;
bool AutomataTests::dispatch_thread_waiting_ = false;

static ssize_t mock_read(int, void*, size_t) {
  // Never returns.
  while(1) {
    sleep(1);
  }
  return 0;
}
static ssize_t mock_write(int fd, const void* buf, size_t n) {
  can_pipe0.WriteToAll(NULL, buf, n);
  return n;
}

void* AutomataTests::DispatchThread(void* arg) {
  dispatch_thread_waiting_ = false;
  node_t node = (node_t)arg;
  while(1) {
    dispatch_thread_waiting_ = true;
    int result = nmranet_node_wait(node, MSEC_TO_NSEC(300));
    dispatch_thread_waiting_ = false;
    if (result) {
      for (size_t i = nmranet_event_pending(node); i > 0; i--) {
        node_handle_t node_handle;
        uint64_t event = nmranet_event_consume(node, &node_handle);
        registry_.HandleEvent(event);
      }
      for (size_t i = nmranet_datagram_pending(node); i > 0; i--) {
        datagram_t datagram = nmranet_datagram_consume(node);
        // We release unknown datagrams iwthout processing them.
        nmranet_datagram_release(datagram);
      }
    }
  }
  return NULL;
}

void AutomataTests::SetUpTestCase() {
  int fd[2] = {0, 0};
  // can_pipe0.AddVirtualDeviceToPipe("can_pipe", 2048, fd);

  nmranet_if_ = nmranet_can_if_fd_init(0x02010d000000ULL, fd[0], fd[1],
                                      "can_mock_if", mock_read, mock_write);
}


class AutomataNodeTests : public AutomataTests {
protected:
  AutomataNodeTests() {
    SetupStaticNode();
  }

  ~AutomataNodeTests() {
    WaitForEventThread();
  }

  void Run(int count = 1) {
    for (int i = 0; i < count; ++i) {
      WaitForEventThread();
      runner_->RunAllAutomata();
    }
    WaitForEventThread();
  }

  void SetVar(const automata::EventBasedVariable& var, bool value) {
    uint64_t eventid = value ? var.event_on() : var.event_off();
    fprintf(stderr, "Producing event %016llx on node %p\n", eventid, node_);
    nmranet_event_produce(node_, eventid, EVENT_STATE_VALID);
  }

  friend class EventListener;

  static EventRegistry* registry() {
    return &registry_;
  }

  class EventListener : public EventHandler {
   public:
    EventListener(const automata::EventBasedVariable& var) 
        : var_(var), bit_(false) {
      AutomataNodeTests::registry()->RegisterHandler(this, var_.event_on());
      AutomataNodeTests::registry()->RegisterHandler(this, var_.event_off());
    }

    virtual ~EventListener() {
      AutomataNodeTests::registry()->UnregisterHandler(this, var_.event_on());
      AutomataNodeTests::registry()->UnregisterHandler(this, var_.event_off());
    }

    virtual void HandleEvent(uint64_t event) {
      if (event == var_.event_on()) {
        bit_ = true;
      } else if (event == var_.event_off()) {
        bit_ = false;
      } else {
        extern bool unknown_event_arrived();
        HASSERT(false && unknown_event_arrived());
      }
    }

    bool value() {
      return bit_;
    }

   private:
    const automata::EventBasedVariable& var_;
    bool bit_;
  };

  // Returns the current value of the event based variable var. This is
  // independent of the internal event representation (state bits) and only
  // listens to OpenLCB events. If the variable is undefined, it is assumed to
  // be false (default state of internal bit).
  bool QueryVar(const automata::EventBasedVariable& var) {
    return all_listener_.Query(var);
  } 

  // Like QueryVar, but assert fails if the variable value is not defined yet.
  bool SQueryVar(const automata::EventBasedVariable& var) {
    return all_listener_.StrictQuery(var);
  } 

 private:
  friend class GlobalEventListener;

  class GlobalEventListener : public EventHandler {
   public:
    GlobalEventListener() : tick_(0) {
      AutomataNodeTests::registry()->RegisterGlobalHandler(this);
    }

    virtual ~GlobalEventListener() {
      AutomataNodeTests::registry()->UnregisterGlobalHandler(this);
    }

    virtual void HandleEvent(uint64_t event) {
      event_last_seen_[event] = ++tick_;
    }

    bool StrictQuery(const automata::EventBasedVariable& var) {
      int t_on = event_last_seen_[var.event_on()];
      int t_off = event_last_seen_[var.event_off()];
      HASSERT(t_on || t_off);
      return t_on > t_off;
    }

    bool Query(const automata::EventBasedVariable& var) {
      int t_on = event_last_seen_[var.event_on()];
      int t_off = event_last_seen_[var.event_off()];
      // If they are both zero, we return false.
      return t_on > t_off;
    }

   private:
    map<uint64_t, int> event_last_seen_;
    int tick_;
  };

  GlobalEventListener all_listener_;
};
