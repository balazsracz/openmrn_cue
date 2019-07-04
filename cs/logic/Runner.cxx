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

namespace logic {

/// Logic should run 10 times a second.
constexpr long AUTOMATA_TICK_MSEC = 100;


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

struct Runner::RunnerImpl {
  RunnerImpl(VariableFactory* vars, Runner* parent)
      : parent_(parent), vm_(vars) {
  }

  ~RunnerImpl();
  
  Runner* parent_;

  /// Execution engine.
  VM vm_;

  /// Engine that compiles the source code into binary instructions.
  Driver compile_driver_;

  /// Thread context in which all the operations take place.
  Executor<1> automata_thread_{"logic", 0, 3000};

  /// Responsible for repeated execution of the automata tick.
  RunnerTimer* timer_{nullptr};
};

Runner::Runner(OlcbVariableFactory* vars)
    : impl_(new RunnerImpl(vars, this)) {}

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
  // Signals the timer to stop running. Will prevent all further callbacks.
  impl()->timer_->parent_ = nullptr;
  impl()->timer_ = nullptr;
}


}  // namespace logic

