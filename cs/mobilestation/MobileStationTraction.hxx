/** \copyright
 * Copyright (c) 2014, Balazs Racz
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
 * \file MobileStationTraction.hxx
 *
 * Translation service that listens to Marklin Mobile Station (1) style packets
 * and translates them to the NMRAnet traction protocol.
 *
 * @author Balazs Racz
 * @date 18 May 2014
 */

#ifndef _BRACZ_CUSTOM_MOBILESTATIONTRACTION_HXX_
#define _BRACZ_CUSTOM_MOBILESTATIONTRACTION_HXX_

#include "executor/Service.hxx"

class CanIf;
class Executable;

namespace nmranet {
class If;
class Node;
}

namespace commandstation {
class TrainDb;
}

namespace mobilestation {

class MobileStationTraction : public Service {
 public:
  MobileStationTraction(CanIf* mosta_if, nmranet::If* nmranet_if,
                        commandstation::TrainDb* train_db, nmranet::Node* query_node);
  ~MobileStationTraction();

  nmranet::If* nmranet_if() {
    return nmranetIf_;
  }

  CanIf* mosta_if() {
    return mostaIf_;
  }

  commandstation::TrainDb* train_db() {
    return trainDb_;
  }

  nmranet::Node* node() {
    return node_;
  }

  enum EstopSource {
    ESTOP_FROM_MOSTA = 1,
    ESTOP_FROM_OPENLCB = 2,
    ESTOP_FROM_LOCAL = 4,
  };
  /** Changes the estop state. This may trigger a message on one or more
   * buses
   * @param source is the bus from which the state change command arrived.
   * @param is_stopped if true, set state to emergency stop.*/
  void set_estop_state(EstopSource source, bool is_stopped);

  /** @returns the stored estop state for a given source. true = estop.*/
  bool get_estop_state(EstopSource source) {
    return estopState_ & source;
  }

 private:
  void update_estop_bit(EstopSource source, bool is_stopped) {
    if (is_stopped) {
      estopState_ |= source;
    } else {
      estopState_ &= ~source;
    }
  }

  /** Bitmask on EStopSource. If a bit is set, that state is in estop. */
  uint8_t estopState_;
  struct Impl;
  Impl* impl_;
  nmranet::If* nmranetIf_;
  CanIf* mostaIf_;
  commandstation::TrainDb* trainDb_;
  nmranet::Node* node_; //< will be used for traction query and set commands
};


}  // namespace mobilestation

#endif // _BRACZ_CUSTOM_MOBILESTATIONTRACTION_HXX_
