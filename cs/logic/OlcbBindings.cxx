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

namespace logic {

OlcbVariableFactory::~OlcbVariableFactory() {}

OlcbVariableFactory::UpdateAction OlcbVariableFactory::apply_configuration(
    int fd, bool initial_load, BarrierNotifiable* done) {
  config_fd_ = fd;
  return UPDATED;
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
    VariableCreationRequest* request) const {
  /// @todo
  return nullptr;
}



} // namespace logic