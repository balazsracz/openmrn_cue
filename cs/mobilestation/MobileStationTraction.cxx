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

#define LOGLEVEL WARNING

#include "mobilestation/MobileStationTraction.hxx"

#include <inttypes.h>

#include "mobilestation/TrainDb.hxx"
#include "nmranet/Defs.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/If.hxx"
#include "nmranet/Node.hxx"
#include "nmranet/TractionClient.hxx"
#include "nmranet/TractionDefs.hxx"
#include "nmranet/Velocity.hxx"
#include "nmranet/WriteHelper.hxx"
#include "utils/constants.hxx"

#include "utils/CanIf.hxx"

namespace mobilestation {

DECLARE_CONST(mobile_station_train_count);

class TractionImpl : public IncomingFrameFlow {
 public:
  TractionImpl(MobileStationTraction* s)
      : IncomingFrameFlow(s),
        tractionClient_(service()->nmranet_if(), service()->node()),
        timer_(this) {
    service()->mosta_if()->frame_dispatcher()->register_handler(
        this, TRACTION_SET_ID, TRACTION_SET_MASK);
    service()->mosta_if()->frame_dispatcher()->register_handler(
        this, TRACTION_ESTOP_ID, TRACTION_ESTOP_MASK);
  }
  ~TractionImpl() {
    service()->mosta_if()->frame_dispatcher()->unregister_handler(
        this, TRACTION_SET_ID, TRACTION_SET_MASK);
    service()->mosta_if()->frame_dispatcher()->unregister_handler(
        this, TRACTION_ESTOP_ID, TRACTION_ESTOP_MASK);
  }

  MobileStationTraction* service() {
    return static_cast<MobileStationTraction*>(IncomingFrameFlow::service());
  }

 public:
  enum {
    TRACTION_CMD_SHIFT = 7,
    TRACTION_CMD_MASK = 7,  // apply after shift, 3 bits.
    TRACTION_CMD_SET = 0b010,
    TRACTION_SET_ID = 0x08080100,
    TRACTION_SET_MASK = 0x0FF80380,
    TRACTION_ESTOP_ID =   0x08000901,
    TRACTION_ESTOP_MASK = 0x0FFFFF80,
    TRACTION_SET_TRAIN_SHIFT = 10,
    TRACTION_SET_TRAIN_MASK = 0x3F,  // apply after shift.
    TRACTION_SET_MOTOR_FN =
        1,  // used as data[0] for traction get/set commands.
    TRACTION_TIMEOUT_MSEC = 500,
  };

 private:
  static const uint64_t EVENT_POWER_ON = 0x0501010114FF0004ULL;
  static const uint64_t EVENT_POWER_OFF = 0x0501010114FF0005ULL;

  const struct can_frame& frame() { return message()->data()->frame(); }

  Action entry() OVERRIDE {
    uint32_t can_id = message()->data()->id();

    if ((can_id & TRACTION_ESTOP_MASK) == (TRACTION_ESTOP_ID & TRACTION_ESTOP_MASK)) {
      if (frame().can_dlc == 3 && frame().data[0] == 1 &&
          frame().data[1] == 0) {
        //&& lastEstopState_ != frame().data[2]) {
        service()->set_estop_state(MobileStationTraction::ESTOP_FROM_MOSTA,
                                   frame().data[2]);
        return release_and_exit();
      }
    } else if ((can_id & TRACTION_SET_MASK) == (TRACTION_SET_ID & TRACTION_SET_MASK)) {
      if (!service()->train_db()->is_train_id_known(train_id())) {
        return release_and_exit();
      }
      // check if we have something to do.
      if ((frame().can_dlc == 3 &&  // set any parameter
           frame().data[1] == 0) ||
          need_response()) {
        return allocate_and_call(
            service()->nmranet_if()->global_message_write_flow(),
            STATE(send_write_query));
      }
      LOG(VERBOSE, "nothing to do. dlc %u, data[0] %u, data[1] %u",
          frame().can_dlc, frame().data[0], frame().data[1]);
    } else {
      LOG_ERROR("unexpected command 0x%08" PRIx32 " masked 0x%08" PRIx32 " compare 0x%08x", can_id, (can_id & TRACTION_ESTOP_MASK), TRACTION_ESTOP_ID);
    }
    return release_and_exit();
  }

  Action send_write_query() {
    auto* b = get_allocation_result(
        service()->nmranet_if()->addressed_message_write_flow());

    unsigned train_id = this->train_id();

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
        LOG(VERBOSE, "unknown fn");
        // we don't know, sorry.
        b->unref();
        return release_and_exit();
      }
    }
    LOG(VERBOSE, "in send query");

    if (frame().can_dlc == 3 && frame().data[0] == TRACTION_SET_MOTOR_FN) {
      // We are doing a set speed.
      nmranet::Velocity v;
      v.set_mph(frame().data[2] & 0x7F);
      if (frame().data[2] & 0x80) {
        v.reverse();
      }
      b->data()->payload = nmranet::TractionDefs::speed_set_payload(v);
      service()->nmranet_if()->addressed_message_write_flow()->send(b);
      if (need_response()) {
        responseByte_ = frame().data[2];
        return allocate_and_call(service()->mosta_if()->frame_write_flow(),
                                 STATE(send_response));
      } else {
        return release_and_exit();
      }
    } else if (frame().can_dlc == 3) {
      // We are doing a set function.
      unsigned fn_address = service()->train_db()->get_function_address(
          train_id, frame().data[0]);
      b->data()->payload =
          nmranet::TractionDefs::fn_set_payload(fn_address, frame().data[2]);
      service()->nmranet_if()->addressed_message_write_flow()->send(b);
      if (need_response()) {
        responseByte_ = frame().data[2];
        return allocate_and_call(service()->mosta_if()->frame_write_flow(),
                                 STATE(send_response));
      } else {
        return release_and_exit();
      }
    } else if (frame().can_dlc == 2 &&
               frame().data[0] == TRACTION_SET_MOTOR_FN) {
      // We are doing a get speed.
      b->data()->payload = nmranet::TractionDefs::speed_get_payload();
      Action a = sleep_and_call(&timer_, MSEC_TO_NSEC(TRACTION_TIMEOUT_MSEC),
                                STATE(handle_get_speed_response));
      tractionClient_.wait_for_response(b->data()->dst, b->data()->payload[0],
                                        &timer_);
      service()->nmranet_if()->addressed_message_write_flow()->send(b);
      return a;
    } else if (frame().can_dlc == 2) {
      // We are doing a get fn.
      LOG(VERBOSE, "sending fn get");
      b->data()->payload = nmranet::TractionDefs::fn_get_payload(fn_address);
      Action a = sleep_and_call(&timer_, MSEC_TO_NSEC(TRACTION_TIMEOUT_MSEC),
                                STATE(handle_get_fn_response));
      tractionClient_.wait_for_response(b->data()->dst, b->data()->payload[0],
                                        &timer_);
      service()->nmranet_if()->addressed_message_write_flow()->send(b);
      return a;
    } else {
      DIE("not sure what to do with this request.");
    }
  }

  Action handle_get_speed_response() {
    tractionClient_.wait_timeout();
    auto* rb = tractionClient_.response();
    if (!rb) {
      return release_and_exit();
    }
    nmranet::Velocity v_sp;
    if (!nmranet::TractionDefs::speed_get_parse_last(rb->data()->payload,
                                                     &v_sp)) {
      rb->unref();
      return release_and_exit();
    }
    uint8_t speed = v_sp.mph();
    if (speed > 127) speed = 127;
    if (v_sp.direction() == nmranet::Velocity::REVERSE) {
      speed |= 0x80;
    }
    responseByte_ = speed;
    rb->unref();

    return allocate_and_call(service()->mosta_if()->frame_write_flow(),
                             STATE(send_response));
  }

  Action handle_get_fn_response() {
    tractionClient_.wait_timeout();
    auto* rb = tractionClient_.response();
    if (!rb) {
      return release_and_exit();
    }
    uint16_t fn_value;
    if (!nmranet::TractionDefs::fn_get_parse(rb->data()->payload, &fn_value)) {
      rb->unref();
      return release_and_exit();
    }
    responseByte_ = fn_value ? 1 : 0;
    rb->unref();
    return allocate_and_call(service()->mosta_if()->frame_write_flow(),
                             STATE(send_response));
  }

  Action send_response() {
    auto* b = get_allocation_result(service()->mosta_if()->frame_write_flow());

    auto* f = b->data()->mutable_frame();
    SET_CAN_FRAME_ID_EFF(
        *f, TRACTION_SET_ID | (train_id() << TRACTION_SET_TRAIN_SHIFT));
    f->can_dlc = 3;
    f->data[0] = message()->data()->frame().data[0];
    f->data[1] = 0;
    f->data[2] = responseByte_;

    service()->mosta_if()->frame_write_flow()->send(b);
    return release_and_exit();
  }

  unsigned train_id() {
    return (message()->data()->id() >> TRACTION_SET_TRAIN_SHIFT) &
           TRACTION_SET_TRAIN_MASK;
  }

  /** Returns true if we must respond to this message. */
  bool need_response() {
    return ((frame().can_dlc == 2 || frame().can_dlc == 3) &&  // get or set
            frame().data[1] == 0 &&
            // parameter number too high for mobile station
            (frame().data[0] >= 0xb ||
             // train number too high for mobile station
             train_id() >= ((unsigned)config_mobile_station_train_count())));
  }

  nmranet::TractionResponseHandler tractionClient_;
  StateFlowTimer timer_;
  // The third byte of the Mosta response.
  uint8_t responseByte_;
};


class TrackPowerOnOffBit : public nmranet::BitEventInterface {
 public:
  TrackPowerOnOffBit(MobileStationTraction* s)
      : BitEventInterface(nmranet::TractionDefs::EMERGENCY_STOP_EVENT,
                          nmranet::TractionDefs::CLEAR_EMERGENCY_STOP_EVENT),
        service_(s) {}

  virtual nmranet::EventState GetCurrentState() {
    return service_->get_estop_state(MobileStationTraction::ESTOP_FROM_OPENLCB) ? nmranet::EventState::VALID : nmranet::EventState::INVALID;
  }

  virtual void SetState(bool new_value) {
    service_->set_estop_state(MobileStationTraction::ESTOP_FROM_OPENLCB,
                              new_value);
  }

  virtual nmranet::Node* node() { return service_->node(); }

 private:
  MobileStationTraction* service_;
};

struct MobileStationTraction::Impl {
  Impl(MobileStationTraction* parent)
      : handler_(parent)
      , lcb_power_bit_(parent)
      , lcb_power_bit_consumer_(&lcb_power_bit_) {}
  ~Impl() {}
  TractionImpl handler_;   //< The implementation flow.
  TrackPowerOnOffBit lcb_power_bit_;
  nmranet::BitEventConsumer lcb_power_bit_consumer_;
};

void MobileStationTraction::set_estop_state(EstopSource source,
                                            bool is_stopped) {
  // We set the stored state for that source.
  update_estop_bit(source, is_stopped);
  EstopSource bit;
  bool last_state;
  bit = ESTOP_FROM_MOSTA;
  last_state = estopState_ & bit;
  if (last_state != is_stopped) {
    // Sends update to MoSta.
    LOG_ERROR("estop command %d to MoSta", is_stopped);
    auto* b = mosta_if()->frame_write_flow()->alloc();
    struct can_frame* f = b->data()->mutable_frame();
    SET_CAN_FRAME_ID_EFF(*f, TractionImpl::TRACTION_ESTOP_ID);
    SET_CAN_FRAME_EFF(*f);
    f->can_dlc = 3;
    f->data[0] = 1;
    f->data[1] = 0;
    f->data[2] = is_stopped ? 1 : 0;
    mosta_if()->frame_write_flow()->send(b);

    update_estop_bit(bit, is_stopped);
  }
  bit = ESTOP_FROM_OPENLCB;
  last_state = estopState_ & bit;
  if (last_state != is_stopped) {
    // Sends update to OpenLCB.
    LOG_ERROR("estop command %d to OpenLCB", is_stopped);
    auto* b = nmranet_if()->global_message_write_flow()->alloc();
    b->data()->reset(
        nmranet::Defs::MTI_EVENT_REPORT, node()->node_id(),
        nmranet::eventid_to_buffer(
            is_stopped
                ? nmranet::TractionDefs::EMERGENCY_STOP_EVENT
                : nmranet::TractionDefs::CLEAR_EMERGENCY_STOP_EVENT));
    nmranet_if()->global_message_write_flow()->send(b);

    update_estop_bit(bit, is_stopped);
  }
}


MobileStationTraction::MobileStationTraction(CanIf* mosta_if,
                                             nmranet::If* nmranet_if,
                                             TrainDb* train_db,
                                             nmranet::Node* query_node)
    : Service(nmranet_if->dispatcher()->service()->executor()),
      estopState_(0),
      nmranetIf_(nmranet_if),
      mostaIf_(mosta_if),
      trainDb_(train_db),
      node_(query_node) {
  impl_ = new Impl(this);
  //impl_->handler_ = new TractionImpl(this);
}

MobileStationTraction::~MobileStationTraction() { delete impl_; }

}  // namespace mobilestation
