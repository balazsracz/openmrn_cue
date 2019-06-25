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
 * \file OlcbBindings.hxx
 *
 * Implementation of external variables for OpenLCB bindings purposes.
 *
 * @author Balazs Racz
 * @date 25 June 2019
 */

#ifndef _LOGIC_OLCBBINDINGS_HXX_
#define _LOGIC_OLCBBINDINGS_HXX_

#include "logic/Variable.hxx"
#include "utils/ConfigUpdateListener.hxx"

namespace logic {

class OlcbVariableFactory : public VariableFactory,
                            public DefaultConfigUpdateListener {
 public:
  OlcbVariableFactory(openlcb::Node* node) : node_(node) {}
  ~OlcbVariableFactory();
  
  openlcb::Node* get_node() const { return node_; }

  std::unique_ptr<Variable> create_variable(
      VariableCreationRequest* request) const override;

  UpdateAction apply_configuration(int fd, bool initial_load,
                                   BarrierNotifiable* done) override;

  /// Clears configuration file and resets the configuration settings to
  /// factory value.
  ///
  /// @param fd is the file descriptor for the EEPROM file. The current
  /// offset in this file is unspecified, callees must do lseek.
  void factory_reset(int fd) override;

 private:
  /// Node object that will be used to communicate with the OpenLCB bus.
  openlcb::Node* node_;

  /// File descriptor of the CDI config file where our data resides.
  int config_fd_{-1};
};
}  // namespace logic

#endif  // _LOGIC_OLCBBINDINGS_HXX_
