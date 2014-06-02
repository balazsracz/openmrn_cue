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

#include "mobilestation/MobileStationTraction.hxx"

#include "mobilestation/TrainDb.hxx"
#include "nmranet/If.hxx"
#include "nmranet/Node.hxx"
#include "nmranet/Velocity.hxx"
#include "nmranet/TractionDefs.hxx"

#include "utils/CanIf.hxx"

namespace mobilestation {

class TractionImpl : public IncomingFrameFlow {
 public:
  TractionImpl(MobileStationTraction* s) : IncomingFrameFlow(s) {
    service()->mosta_if()->frame_dispatcher()->register_handler(
        this, TRACTION_SET_ID, TRACTION_SET_MASK);
  }
  ~TractionImpl() {
    service()->mosta_if()->frame_dispatcher()->unregister_handler(
        this, TRACTION_SET_ID, TRACTION_SET_MASK);
  }

  MobileStationTraction* service() {
    return static_cast<MobileStationTraction*>(IncomingFrameFlow::service());
  }

 private:
  enum {
    TRACTION_CMD_SHIFT = 7,
    TRACTION_CMD_MASK = 7,  // apply after shift, 3 bits.
    TRACTION_CMD_SET = 0b010,
    TRACTION_SET_ID = 0x08080100,
    TRACTION_SET_MASK = 0x0FF80380,
    TRACTION_SET_TRAIN_SHIFT = 10,
    TRACTION_SET_TRAIN_MASK = 0x3F,  // apply after shift.
    TRACTION_SET_MOTOR_FN =
        1,  // used as data[0] for traction get/set commands.
  };

  const struct can_frame& frame() { return message()->data()->frame(); }

  Action entry() OVERRIDE {
    uint32_t can_id = message()->data()->id();
    if ((can_id & TRACTION_SET_MASK) == TRACTION_SET_ID) {
      unsigned train_id =
          (can_id >> TRACTION_SET_TRAIN_SHIFT) & TRACTION_SET_TRAIN_MASK;
      if (!service()->train_db()->is_train_id_known(train_id)) {
        return release_and_exit();
      }
      // check if we have something to do.
      if ((frame().can_dlc == 3 &&  // set any parameter
           frame().data[1] == 0) ||
          (frame().can_dlc == 2 &&  // get parameter
           frame().data[1] == 0 &&
           // parameter number too high for mobile station
           (frame().data[0] >= 0xb ||
            // train number too high for mobile station
            train_id >= 10))) {
        return allocate_and_call(
            service()->nmranet_if()->addressed_message_write_flow(),
            STATE(send_write_query));
      }
    }
    return release_and_exit();
  }

  Action send_write_query() {
    auto* b = get_allocation_result(
        service()->nmranet_if()->addressed_message_write_flow());

    uint32_t can_id = message()->data()->id();
    unsigned train_id =
        (can_id >> TRACTION_SET_TRAIN_SHIFT) & TRACTION_SET_TRAIN_MASK;

    b->data()->src.id = service()->node()->node_id();
    b->data()->src.alias = 0;
    b->data()->dst.id = service()->train_db()->get_traction_node(train_id);
    b->data()->dst.alias = 0;
    b->data()->mti = nmranet::Defs::MTI_TRACTION_CONTROL_COMMAND;

    unsigned fn_address = 0;
    if (frame().data[0] != TRACTION_SET_MOTOR_FN) {
      fn_address = service()->train_db()->get_function_address(train_id,
                                                               frame().data[0]);
      if (fn_address == TrainDb::UNKNOWN_FUNCTION) {
        // we don't know, sorry.
        b->unref();
        return release_and_exit();
      }
    }

    if (frame().can_dlc == 3 && frame().data[0] == TRACTION_SET_MOTOR_FN) {
      // We are doing a set speed.
      nmranet::Velocity v;
      v.set_mph(frame().data[2] & 0x7F);
      if (frame().data[2] & 0x80) {
        v.reverse();
      }
      b->data()->payload = nmranet::TractionDefs::speed_set_payload(v);
      service()->nmranet_if()->addressed_message_write_flow()->send(b);
      return release_and_exit();
    } else if (frame().can_dlc == 3) {
      // We are doing a set function.
      unsigned fn_address = service()->train_db()->get_function_address(
          train_id, frame().data[0]);
      b->data()->payload =
          nmranet::TractionDefs::fn_set_payload(fn_address, frame().data[2]);
      service()->nmranet_if()->addressed_message_write_flow()->send(b);
      return release_and_exit();
    } else if (frame().can_dlc == 2 &&
               frame().data[0] == TRACTION_SET_MOTOR_FN) {
      // We are doing a get speed.
      b->data()->payload = nmranet::TractionDefs::speed_get_payload();
      service()->nmranet_if()->addressed_message_write_flow()->send(b);
      /// @TODO(balazs.racz) implement a handler that will wait for a response.
      return release_and_exit();
    } else if (frame().can_dlc == 2) {
      // We are doing a get fn.
      b->data()->payload = nmranet::TractionDefs::fn_get_payload(fn_address);
      service()->nmranet_if()->addressed_message_write_flow()->send(b);
      /// @TODO(balazs.racz) implement a handler that will wait for a response.
      return release_and_exit();
    } else {
      DIE("not sure what to do with this request.");
    }
  }

};

MobileStationTraction::MobileStationTraction(CanIf* mosta_if,
                                             nmranet::If* nmranet_if,
                                             TrainDb* train_db,
                                             nmranet::Node* query_node)
    : Service(nmranet_if->dispatcher()->service()->executor()),
      nmranetIf_(nmranet_if),
      mostaIf_(mosta_if),
      trainDb_(train_db),
      node_(query_node) {
  handler_ = new TractionImpl(this);
}

MobileStationTraction::~MobileStationTraction() { delete handler_; }

}  // namespace mobilestation
