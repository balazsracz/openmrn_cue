/** \copyright
 * Copyright (c) 2019, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file Runner.cxx
 *
 * Control point for operating the automata compilation and VMs.
 *
 * @author Balazs Racz
 * @date 3 July 2019
 */

#include "logic/Runner.hxx"

#include "logic/Driver.hxx"
#include "logic/VM.hxx"
#include "utils/FileUtils.hxx"
#include "executor/Executor.hxx"
#include "logic/OlcbBindingsConfig.hxx"
#include "logic/OlcbBindings.hxx"

#ifdef __linux__
#include "os/TempFile.hxx"

TempFile compile_tmp(*TempDir::instance(), "compile");

#elif defined(__FreeRTOS__)
#include "freertos_drivers/common/RamDisk.hxx"
RamDisk* rdisk = nullptr;

#else
#error dont know how to make temp files

#endif

namespace logic {

/// Logic should run 10 times a second.
constexpr long AUTOMATA_TICK_MSEC = 100;
constexpr unsigned MAX_SOURCE_SIZE = 4096;

class RunnerTimer : public ::Timer {
 public:
  RunnerTimer(ExecutorBase* e, Runner* parent) : ::Timer(e->active_timers()), parent_(parent) {}

  long long timeout() override {
    auto* p = parent_;
    if (!p) return DELETE;
    p->single_step();
    return RESTART;
  }
    
  Runner* parent_;
};

struct BlockInfo {
  /// true if the block is operational
  bool enabled;
  /// Compiled bytecode.
  std::string bytecode;
};

struct Runner::RunnerImpl {
  RunnerImpl(VariableFactory* vars, Runner* parent)
      : parent_(parent), vm_(vars) {
  }

  ~RunnerImpl();
  
  Runner* parent_;

  /// Compilation engine.
  Driver compile_driver_;
  
  /// Execution engine.
  VM vm_;

  /// Thread context in which all the operations take place.
  Executor<1> automata_thread_{"logic", 0, 3000};

  /// Responsible for repeated execution of the automata tick.
  RunnerTimer* timer_{nullptr};

  /// Stores information on a per-block basis: compiled bytecode for example.
  BlockInfo logic_blocks_[LogicConfig(0).blocks().num_repeats()];
};

Runner::Runner(OlcbVariableFactory* vars)
    : variable_factory_(vars), impl_(new RunnerImpl(vars, this)) {}

Runner::~Runner() {
  stop_running();
  while (!impl()->automata_thread_.active_timers()->empty()) {
    usleep(20000);
  }
  delete impl_;
}

Runner::RunnerImpl::~RunnerImpl() {
}

void Runner::start_running() {
  HASSERT(!impl()->timer_);
  impl()->timer_ = new RunnerTimer(&impl()->automata_thread_, this);
  impl()->timer_->start(MSEC_TO_NSEC(AUTOMATA_TICK_MSEC));
}

void Runner::stop_running() {
  if (!impl()->timer_) return; // nothing to stop
  // Signals the timer to stop running. Will prevent all further callbacks.
  impl()->timer_->parent_ = nullptr;
  impl()->timer_ = nullptr;
}

void Runner::single_step() {
  for (unsigned i = 0; i < variable_factory_->cfg().blocks().num_repeats();
       ++i) {
    auto* bi = impl()->logic_blocks_ + i;
    if (!bi->enabled) continue;
    impl()->vm_.clear();
    impl()->vm_.set_preamble(false);
    impl()->vm_.set_block_num(i);
    if (!impl()->vm_.execute(bi->bytecode)) {
      std::string status = "Error in running: " + impl()->vm_.get_error();
      int fd = variable_factory_->fd();
      const auto& bl = variable_factory_->cfg().blocks().entry(i);
      bi->enabled = false;
      if (status != bl.body().status().read(fd)) {
        bl.body().status().write(fd, status);
      }
    }
  }
}

void Runner::compile(Notifiable* done) {
  impl()->automata_thread_.add(
      new CallbackExecutable(std::bind(&Runner::compile_impl, this, done)));
}

void Runner::compile_impl(Notifiable* done) {
  AutoNotify an(done);
  string tempfilename;
#ifdef __FreeRTOS__
  tempfilename = "/tmp/compile";
  if (!rdisk) {
    rdisk = new RamDisk(tempfilename.c_str(), MAX_SOURCE_SIZE);
  }
#else
  tempfilename = compile_tmp.name();
#endif

  int fd = variable_factory_->fd();
  for (unsigned i = 0; i < variable_factory_->cfg().blocks().num_repeats();
       ++i) {
    auto* bi = impl()->logic_blocks_ + i;
    const auto& bl = variable_factory_->cfg().blocks().entry(i);
    
    std::string source = bl.body().text().read(fd);
    write_string_to_file(tempfilename, source);
    std::string status;
    impl()->compile_driver_.clear();
    if (impl()->compile_driver_.parse_file(tempfilename) != 0) {
      status = "Compile failed.\n";
      status += impl()->compile_driver_.error_output_;
      /// @todo get error message from compile driver.
      bi->enabled = false;
    } else {
      status = "Compile OK. ";
      bi->enabled = CDI_READ_TRIMMED(bl.enabled, fd);
      if (!bi->enabled) status += "Disabled.";
    }
    if (bi->enabled) {
      std::string bc;
      impl()->compile_driver_.serialize(&bc);
      // purposefully not move to get the storage reallocated to match size.
      bi->bytecode = bc;
      impl()->vm_.clear();
      impl()->vm_.set_preamble(true);
      impl()->vm_.set_block_num(i);
      if (!impl()->vm_.execute(bi->bytecode)) {
        status = "Error in preamble: " + impl()->vm_.get_error();
        bi->enabled = false;
      }
    } else {
      std::string s;
      s.swap(bi->bytecode);
    }
    if (status != bl.body().status().read(fd)) {
      bl.body().status().write(fd, status);
    }
  }
}


}  // namespace logic

