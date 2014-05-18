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
 * \file MobileStationSlave.hxx
 *
 * Service that connects to a Marklin MobileStation (I) via CANbus as a slave.
 *
 * @author Balazs Racz
 * @date 16 May 2014
 */

#ifndef _MOBILESTATION_MOBILESTATIONSLAVE_HXX_
#define _MOBILESTATION_MOBILESTATIONSLAVE_HXX_

#include "executor/notifiable.hxx"
#include "executor/Service.hxx"
#include "executor/StateFlow.hxx"
#include "utils/CanIf.hxx"

class CanHubFlow;

namespace mobilestation {

class MobileStationSlave;

class SlaveKeepaliveFlow : public StateFlowBase {
 public:
  SlaveKeepaliveFlow(MobileStationSlave* service);
  ~SlaveKeepaliveFlow();

  inline MobileStationSlave* service();

 private:
  Action call_delay();
  Action maybe_send_keepalive();
  Action send_discover();
  Action send_ping();

  StateFlowTimer timer_;
};

class SlaveInitFlow : public IncomingFrameFlow {
 public:
  SlaveInitFlow(MobileStationSlave* service);
  ~SlaveInitFlow();

  inline MobileStationSlave* service();

  const struct can_frame& frame() {
    return message()->data()->frame();
  }

 private:
  Action entry() OVERRIDE;
  Action send_aliveack();
  Action send_login();
  Action send_sequence();
  Action next_sequence();

  BarrierNotifiable cb_;
};

class MobileStationSlave : public Service {
 public:
  /** Initializes a mobile station client.
   * @param e is the executor on which the mobile staiton client should run.
   * @param device is the CANbus device with the mobile station.
   */
  MobileStationSlave(ExecutorBase* e, CanIf* device);
  ~MobileStationSlave();

  CanIf* device() {
    return device_;
  }

  struct StateImpl;
  StateImpl* state_;

 private:
  CanIf* device_;
  SlaveKeepaliveFlow keepalive_flow_;
  SlaveInitFlow init_flow_;
};

}  // namespace mobilestation


#endif // _MOBILESTATION_MOBILESTATIONSLAVE_HXX_


