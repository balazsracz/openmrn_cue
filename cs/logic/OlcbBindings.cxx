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
 * \file OlcbBindings.cxx
 *
 * Implementation of external variables for OpenLCB bindings purposes.
 *
 * @author Balazs Racz
 * @date 25 June 2019
 */

#include "logic/OlcbBindings.hxx"
#include "logic/OlcbBindingsImpl.hxx"

namespace logic {

OlcbVariableFactory::~OlcbVariableFactory() {}

OlcbVariableFactory::UpdateAction OlcbVariableFactory::apply_configuration(
    int fd, bool initial_load, BarrierNotifiable* done) {
  AutoNotify an(done);
  config_fd_ = fd;
  runner_.compile(done->new_child());
  if (initial_load) {
    runner_.start_running();
    return UPDATED;
  } else {
    /// @todo check if we actually have changed anything about variables.
    return REINIT_NEEDED;
  }
}

void OlcbVariableFactory::factory_reset(int fd) {
  for (unsigned bl = 0; bl < cfg_.blocks().num_repeats(); ++bl) {
    const auto& block = cfg_.blocks().entry(bl);
    block.filename().write(fd, "");
    CDI_FACTORY_RESET(block.enabled);
    block.body().text().write(fd, "");
    block.body().status().write(fd, "Disabled");
    for (unsigned imp = 0; imp < block.body().imports().num_repeats(); ++imp) {
      const auto& e = block.body().imports().entry(imp);
      e.name().write(fd, "");
      // event on and off will be automatically reset.
    }
  }
}


std::unique_ptr<Variable> OlcbVariableFactory::create_variable(
    VariableCreationRequest* request) {
  HASSERT(request->block_num < cfg_.blocks().num_repeats());
  const auto& body = cfg_.blocks().entry(request->block_num).body();
  int freeidx = -1;
  for (unsigned varidx = 0; varidx < body.imports().num_repeats(); ++varidx) {
    const auto& e = body.imports().entry(varidx);
    auto name = e.name().read(config_fd_);
    if (name == request->name) {
      // Found variable.
      /// @todo define non-boolean variables as well.
      auto event_on = e.event_on().read(config_fd_);
      auto event_off = e.event_off().read(config_fd_);
      return std::unique_ptr<Variable>(new OlcbBoolVariable(event_on, event_off, this));
    } else if (name.empty() && freeidx < 0) {
      freeidx = varidx;
    }
  }
  if (freeidx < 0) {
    // No space left for variable creation.
    return nullptr;
  }
  /// @todo instead of taking a free index we should really use the original
  /// indexes defined by the program.
  const auto& e = body.imports().entry(freeidx);
  e.name().write(config_fd_, request->name);
  auto event_on = e.event_on().read(config_fd_);
  auto event_off = e.event_off().read(config_fd_);
  return std::unique_ptr<Variable>(new OlcbBoolVariable(event_on, event_off, this));
}



} // namespace logic
