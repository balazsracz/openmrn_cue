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
 * \file Runner.hxx
 *
 * Control point for operating the automata compilation and VMs.
 *
 * @author Balazs Racz
 * @date 3 July 2019
 */

#ifndef _LOGIC_RUNNER_HXX_
#define _LOGIC_RUNNER_HXX_

#include "logic/OlcbBindings.hxx"
#include "logic/VM.hxx"

namespace logic {

class Runner {
  /// Owner of all variables. Factored out to reduce dependencies.
  struct RunnerImpl;

 public:
  Runner(OlcbVariableFactory* parent);

  ~Runner();
  
  /// Compiles all automata, creates variables and prepares for running. Also
  /// inquires all external states. Notifies done when completed.
  void compile(Notifiable* done);
  
  /// Stops the periodic execution of the virtual machines.
  void stop_running();
  /// Starts the periodic execution of virtual machines.
  void start_running();
  /// Synchronously runs one execution of the virtual machines. This is called
  /// by the automata timer but can also be used by unit tests.
  void single_step();

  RunnerImpl* impl() {
    return impl_;
  }
  
 private:
  /// Implementation of the compilation method.
  void compile_impl(Notifiable* done);
  
  /// pimpl pointer. Public so that Impl can access, but since Impl object is
  /// defined in the .cxx this is not really public.
  RunnerImpl* impl_;
  
  /// Binding for OpenLCB to variables. Externally owned.
  OlcbVariableFactory* variable_factory_;
};



}  // namespace logic


#endif // _LOGIC_RUNNER_HXX_
