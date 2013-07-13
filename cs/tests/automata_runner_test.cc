#include <unistd.h>

#include <string>
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "src/automata_runner.h"
#include "src/event_registry.hxx"

#include "../automata/system.hxx"
#include "../automata/operations.hxx"
#include "../automata/variables.hxx"

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

using automata::Board;
using automata::StateRef;

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



TEST(TimerBitTest, Get) {
  Automata a(1, 0);
  ReadWriteBit* bit = a.GetTimerBit();
  EXPECT_TRUE(bit);
}

TEST(TimerBitTest, ShowsZeroNonZeroState) {
  Automata a(1, 0);
  ReadWriteBit* bit = a.GetTimerBit();
  ASSERT_TRUE(bit);
  a.SetTimer(13);
  EXPECT_EQ(13, a.GetTimer());
  EXPECT_TRUE(bit->Read(NULL, &a));
  a.SetTimer(0);
  EXPECT_EQ(0, a.GetTimer());
  EXPECT_FALSE(bit->Read(NULL, &a));
  a.SetTimer(1);
  EXPECT_TRUE(bit->Read(NULL, &a));
}

TEST(TimerBitTest, Tick) {
  Automata a(1, 0);
  ReadWriteBit* bit = a.GetTimerBit();
  ASSERT_TRUE(bit);
  a.SetTimer(3);
  EXPECT_TRUE(bit->Read(NULL, &a));
  a.Tick();
  EXPECT_EQ(2, a.GetTimer());
  EXPECT_TRUE(bit->Read(NULL, &a));
  a.Tick();
  EXPECT_EQ(1, a.GetTimer());
  EXPECT_TRUE(bit->Read(NULL, &a));
  a.Tick();
  EXPECT_EQ(0, a.GetTimer());
  EXPECT_FALSE(bit->Read(NULL, &a));
  a.Tick();
  EXPECT_EQ(0, a.GetTimer());
  EXPECT_FALSE(bit->Read(NULL, &a));
}

TEST(TimerBitTest, DifferentInitialize) {
  Automata a(10, 0);
  Automata b(30, 0);
  EXPECT_EQ(31, (0xff & (~_ACT_TIMER_MASK)));
  a.SetTimer(31);
  b.SetTimer(31);
  EXPECT_NE(a.GetTimer(), b.GetTimer());
}

TEST(TimerBitTest, DiesOnWrite) {
  Automata a(10, 0);
  ReadWriteBit* bit = a.GetTimerBit();
  EXPECT_DEATH({
      bit->Write(NULL, &a, false);
    }, "802AAC2A");
}

TEST(AutObjTest, IdAndStartingOffset) {
  Automata a(33, 1882);
  EXPECT_EQ(33, a.GetId());
  EXPECT_EQ(1882, a.GetStartingOffset());
}

TEST(AutObjTest, State) {
  Automata a(33, 1882);
  EXPECT_EQ(0, a.GetState());
  EXPECT_EQ(0, a.GetState());
  a.SetState(17);
  EXPECT_EQ(17, a.GetState());
  EXPECT_EQ(17, a.GetState());
}

TEST(RunnerTest, TrivialRunnerCreateDestroy) {
  automata::Board brd;
  string output;
  brd.Render(&output);
  {
    AutomataRunner (NULL, output.data());
  }
}

TEST(RunnerTest, TrivialRunnerRun) {
  automata::Board brd;
  string output;
  brd.Render(&output);
  {
    AutomataRunner r(NULL, output.data());
    r.RunAllAutomata();
  }
}

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

    virtual automata::GlobalVariableId GetId() {
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
    FakeBit(AutomataTests* parent) : InjectableVar(parent) {}

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
    next_bit_offset_ = 4242;
    runner_ = NULL;
    node_ = NULL;
  }

  void SetupRunner(automata::Board* brd) {
    string output;
    brd->Render(&output);
    memcpy(program_area_, output.data(), output.size());
    runner_ = new AutomataRunner(node_, program_area_);
    for (auto bit : mock_bits_) {
      runner_->InjectBit(bit->GetGlobalOffset(), bit->CreateBit());
    }
  }

  AutomataTests() {
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
  FakeBit* CreateFakeBit() {
    FakeBit* bit = new FakeBit(next_bit_offset_++);
    mock_bits_.push_back(bit);
    return bit;
  }

  static void* AutomataTests::DispatchThread(void* arg);


  // A loopback interace that reads/writes to can_pipe0.
  static NMRAnetIF* nmranet_if_;
  static EventRegistry registry_;
  // This node will be given to the AutomataRunner in SetupRunner.
  node_t node_;
  AutomataRunner* runner_;
  insn_t program_area_[10000];

private:
  // Stores the global variable offset for the next mockbit.
  int next_bit_offset_;
  // Declared mock bits. Not owned.
  vector<InjectableVar*> mock_bits_;
};


NMRAnetIF* AutomataTests::nmranet_if_ = NULL;
EventRegistry AutomataTests::registry_;

static ssize_t mock_read(int, void*, size_t) {
  // Never returns.
  while(1) {
    sleep(1);
  }
}
static ssize_t mock_write(int fd, const void* buf, size_t n) {
  can_pipe0.WriteToAll(NULL, buf, n);
}

void* AutomataTests::DispatchThread(void* arg) {
  node_t node = (node_t)arg;
  while(1) {
    int result = nmranet_node_wait(node, MSEC_TO_NSEC(300));
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
}

static void AutomataTests::SetUpTestCase() {
  int fd[2] = {0, 0};
  // can_pipe0.AddVirtualDeviceToPipe("can_pipe", 2048, fd);

  nmranet_if_ = nmranet_can_if_fd_init(0x02010d000000ULL, fd[0], fd[1],
                                      "can_mock_if", mock_read, mock_write);
}

TEST(RunnerTest, SingleEmptyAutomataBoard) {
  static automata::Board brd;
  DefAut(testaut1, brd, {});
  string output;
  brd.Render(&output);
  {
    AutomataRunner r(NULL, output.data());
    const auto& all_automata = r.GetAllAutomatas();
    EXPECT_EQ(1, all_automata.size());
    EXPECT_EQ(0, all_automata[0]->GetId());
    EXPECT_EQ(5, all_automata[0]->GetStartingOffset());
    r.RunAllAutomata();
    r.RunAllAutomata();
    r.RunAllAutomata();
  }
}

TEST_F(AutomataTests, DefAct0) {
  automata::Board brd;
  static MockBit mb(this);
  EXPECT_CALL(mb.mock(), Write(_, _, false)).Times(1);
  EXPECT_CALL(mb.mock(), Read(_, _)).Times(0);
  DefAut(testaut1, brd, {
      auto v1 = ImportVariable(&mb);
      Def().ActReg0(v1);
    });
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, DefAct1) {
  automata::Board brd;
  static MockBit mb(this);
  EXPECT_CALL(mb.mock(), Write(_, _, true)).Times(1);
  EXPECT_CALL(mb.mock(), Read(_, _)).Times(0);
  DefAut(testaut1, brd, {
      auto v1 = ImportVariable(&mb);
      Def().ActReg1(v1);
    });
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, DefIfReg1) {
  automata::Board brd;
  static MockBit rb(this);
  static MockBit rrb(this);
  static MockBit wb(this);
  EXPECT_CALL(wb.mock(), Write(_, _, true));
  EXPECT_CALL(rb.mock(), Read(_, _)).WillOnce(Return(true));
  EXPECT_CALL(rrb.mock(), Read(_, _)).WillOnce(Return(false));
  DefAut(testaut1, brd, {
      auto rv = ImportVariable(&rb);
      auto rrv = ImportVariable(&rrb);
      auto wv = ImportVariable(&wb);
      Def().IfReg1(rv).ActReg1(wv);
      Def().IfReg1(rrv).ActReg1(wv);
    });
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, DefIfReg0) {
  automata::Board brd;
  static MockBit rb(this);
  static MockBit rrb(this);
  static MockBit wb(this);
  EXPECT_CALL(wb.mock(), Write(_, _, true));
  EXPECT_CALL(rb.mock(), Read(_, _)).WillOnce(Return(true));
  EXPECT_CALL(rrb.mock(), Read(_, _)).WillOnce(Return(false));
  DefAut(testaut1, brd, {
      auto rv = ImportVariable(&rb);
      auto rrv = ImportVariable(&rrb);
      auto wv = ImportVariable(&wb);
      Def().IfReg0(rv).ActReg1(wv);
      Def().IfReg0(rrv).ActReg1(wv);
    });
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, CopyAutomataBoard) {
  automata::Board brd;
  static FakeBit mbit1(this);
  static FakeBit mbit2(this);
  DefAut(testaut1, brd, {
      auto v1 = ImportVariable(&mbit1);
      auto v2 = ImportVariable(&mbit2);
      Def().IfReg1(v1).ActReg1(v2);
      Def().IfReg0(v1).ActReg0(v2);
    });
  SetupRunner(&brd);
  mbit1.Set(false);
  mbit2.Set(true);
  EXPECT_FALSE(mbit1.Get());
  EXPECT_TRUE(mbit2.Get());
  runner_->RunAllAutomata();
  EXPECT_FALSE(mbit1.Get());
  EXPECT_FALSE(mbit2.Get());

  // Second run should not change anything.
  runner_->RunAllAutomata();
  EXPECT_FALSE(mbit1.Get());
  EXPECT_FALSE(mbit2.Get());

  mbit1.Set(true);
  EXPECT_TRUE(mbit1.Get());
  EXPECT_FALSE(mbit2.Get());

  // Now it should be copied back to zero.
  runner_->RunAllAutomata();
  EXPECT_TRUE(mbit1.Get());
  EXPECT_TRUE(mbit2.Get());
}

TEST_F(AutomataTests, ActState) {
  Board brd;
  DefAut(testaut1, brd, {
      StateRef st1(11);
      Def().ActState(st1);
    });
  SetupRunner(&brd);
  Automata* aut = runner_->GetAllAutomatas()[0];
  aut->SetState(0);
  EXPECT_EQ(0, aut->GetState());
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
}

TEST_F(AutomataTests, IfState) {
  Board brd;
  static MockBit wb(this);
  DefAut(testaut1, brd, {
      auto wv = ImportVariable(&wb);
      StateRef st1(11);
      StateRef st2(12);
      Def().IfState(st1).ActReg0(wv);
      Def().IfState(st2).ActReg1(wv);
    });
  SetupRunner(&brd);
  Automata* aut = runner_->GetAllAutomatas()[0];
  aut->SetState(0);
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());

  EXPECT_CALL(wb.mock(), Write(_, _, false));
  aut->SetState(11);
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());

  EXPECT_CALL(wb.mock(), Write(_, _, false));
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());

  EXPECT_CALL(wb.mock(), Write(_, _, true));
  aut->SetState(12);
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());

  aut->SetState(3);
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());
}

TEST_F(AutomataTests, MultipleAutomata) {
  Board brd;
  static MockBit wb(this);
  DefAut(testaut1, brd, {
      auto wv = ImportVariable(&wb);
      Def().ActReg0(wv);
    });
  DefAut(testaut2, brd, {
      auto wv = ImportVariable(&wb);
      Def().ActReg1(wv);
    });
  SetupRunner(&brd);
  EXPECT_CALL(wb.mock(), Write(_, _, true));
  EXPECT_CALL(wb.mock(), Write(_, _, false));

  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, StateFlipFlop) {
  Board brd;
  static MockBit wb(this);
  DefAut(testaut1, brd, {
      StateRef st1(11);
      StateRef st2(12);
      // In order to flip-flop between states we need a temporary state.
      StateRef sttmp(13);
      Def().IfState(st2).ActState(sttmp);
      Def().IfState(st1).ActState(st2);
      Def().IfState(sttmp).ActState(st1);
    });
  SetupRunner(&brd);
  Automata* aut = runner_->GetAllAutomatas()[0];
  aut->SetState(0);

  runner_->RunAllAutomata();
  EXPECT_EQ(0, aut->GetState());

  aut->SetState(11);
  runner_->RunAllAutomata();
  EXPECT_EQ(12, aut->GetState());
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  runner_->RunAllAutomata();
  EXPECT_EQ(12, aut->GetState());
}

TEST_F(AutomataTests, TimerBitCountdown) {
  Board brd;
  DefAut(testaut1, brd, {
      StateRef st1(11);
      StateRef st2(12);
      Def().IfState(st1).IfTimerDone().ActState(st2);
      Def().IfState(st2).ActTimer(3).ActState(st1);
    });
  SetupRunner(&brd);
  Automata* aut = runner_->GetAllAutomatas()[0];
  aut->SetState(0);

  runner_->RunAllAutomata();
  EXPECT_EQ(0, aut->GetState());
  EXPECT_EQ(0, aut->GetTimer());
  
  aut->SetState(11);
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(3, aut->GetTimer());
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(3, aut->GetTimer());
  aut->Tick();
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(2, aut->GetTimer());
  aut->Tick();
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(1, aut->GetTimer());
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(1, aut->GetTimer());
  aut->Tick();
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(3, aut->GetTimer());
}

TEST_F(AutomataTests, LoadEventId) {
  Board brd;
  DefAut(testaut1, brd, {
    });
  SetupRunner(&brd);
  runner_->RunAllAutomata();
  Automata* aut = runner_->GetAllAutomatas()[0];
  runner_->RunAllAutomata();
  string temp;
  automata::EventBasedVariable::CreateEventId(0, 0x554433221166ULL, &temp);
  temp.push_back(0);
  memcpy(program_area_ + aut->GetStartingOffset(),
         temp.data(),
         temp.size());
  runner_->RunAllAutomata();
  EXPECT_EQ(0x554433221166ULL, runner_->GetEventId(0));
  temp.clear();
  automata::EventBasedVariable::CreateEventId(1, 0x443322116655ULL, &temp);
  temp.push_back(0);
  memcpy(program_area_ + aut->GetStartingOffset(),
         temp.data(),
         temp.size());
  runner_->RunAllAutomata();
  EXPECT_EQ(0x443322116655ULL, runner_->GetEventId(1));
}

TEST_F(AutomataTests, EventVar) {
  node_ = nmranet_node_create(0x02010d000001ULL, nmranet_if_,
                              "Test Node", NULL);
  ASSERT_TRUE(node_);
  nmranet_node_user_description(node_, "Test Node");
  nmranet_node_initialized(node_);

  Board brd;
  using automata::EventBasedVariable;
  EventBasedVariable led(&brd,
                         0x0502010202650012ULL,
                         0x0502010202650013ULL,
                         0, OFS_GLOBAL_BITS, 1);
  static automata::GlobalVariable* var;
  var = &led;
  DefAut(testaut1, brd, {
      auto wv = ImportVariable(var);
    });
  string output;
  //brd.Render(&output);
  //EXPECT_EQ("", output);
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}


class CanDebugPipeMember : public PipeMember {
 public:
  CanDebugPipeMember(Pipe* parent)
      : parent_(parent) {
    parent_->RegisterMember(this);
  }

  virtual ~CanDebugPipeMember() {
    parent_->UnregisterMember(this);
  }

  virtual void write(const void* buf, size_t count) {
    if (!count) return;
    char outbuf[100];
    const struct can_frame* frame = static_cast<const struct can_frame*>(buf);
    while (count) {
      assert(count >= sizeof(struct can_frame));
      *gc_format_generate(frame, outbuf, 0) = '\0';
      fprintf(stdout,"%s\n", outbuf);
      count -= sizeof(*frame);
    }
  }
 private:
  Pipe* parent_;
};

CanDebugPipeMember printer(&can_pipe0);






TEST_F(AutomataTests, EventVar2) {
  node_ = nmranet_node_create(0x02010d000002ULL, nmranet_if_,
                              "Test Node2", NULL);
  ASSERT_TRUE(node_);
  fprintf(stderr,"node_=%p\n", node_);
  nmranet_node_user_description(node_, "Test Node2");

  nmranet_event_producer(node_, 0x0502010202650012ULL, EVENT_STATE_INVALID);
  nmranet_event_producer(node_, 0x0502010202650013ULL, EVENT_STATE_VALID);
  nmranet_node_initialized(node_);

  os_thread_t thread;
  os_thread_create(&thread, "event_process_thread",
                   0, 2048, &AutomataTests::DispatchThread,
                   node_);

  nmranet_event_produce(node_, 0x0502010202650012ULL, EVENT_STATE_INVALID);
  nmranet_event_produce(node_, 0x0502010202650012ULL, EVENT_STATE_VALID);
  nmranet_event_produce(node_, 0x0502010202650013ULL, EVENT_STATE_INVALID);
  nmranet_event_produce(node_, 0x0502010202650013ULL, EVENT_STATE_VALID);

  Board brd;
  using automata::EventBasedVariable;
  EventBasedVariable led(&brd,
                         0x0502010202650012ULL,
                         0x0502010202650013ULL,
                         0, OFS_GLOBAL_BITS, 1);
  static automata::GlobalVariable* var;
  var = &led;
  DefAut(testaut1, brd, {
      auto wv = ImportVariable(var);
      Def().ActReg0(wv);
      Def().ActReg1(wv);
    });
  string output;
  //brd.Render(&output);
  //EXPECT_EQ("", output);
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}


TEST_F(AutomataTests, EmptyTest) {
}


int appl_main(int argc, char* argv[]) {
  //__libc_init_array();
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
