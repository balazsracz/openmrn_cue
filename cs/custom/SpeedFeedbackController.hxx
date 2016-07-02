/** \copyright
 * Copyright (c) 2014-2016, Balazs Racz
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
 * \file SpeedFeedbackController.hxx
 *
 * Regulator for train speed feedback.
 *
 * @author Balazs Racz
 * @date 1 Jul 2016
 */

#ifndef _BRACZ_CUSTOM_SPEEDFEEDBACKCONTROLLER_HXX_
#define _BRACZ_CUSTOM_SPEEDFEEDBACKCONTROLLER_HXX_

#include "nmranet/ConfigRepresentation.hxx"
#include "utils/ConfigUpdateListener.hxx"
#include "utils/Ewma.hxx"

CDI_GROUP(FeedbackParams, Name("Load control"),
          Description("Adjust parameters of the load compensation feedback."));
CDI_GROUP_ENTRY(integrate, nmranet::Uint8ConfigEntry,
                Name("Integration parameter"), Min(0), Max(255), Default(20),
                Description(
                    "0 turns off integrating, 255 makes integrating adjustment "
                    "very slow."));
CDI_GROUP_ENTRY(adjust_param, nmranet::Uint8ConfigEntry,
                Name("Rate of adjustment per cycle"), Default(16),
                Description(
                    "Amplifies or dampens the load compensation adjustment. "
                    "16=direct takeover; less than 16 will dampen, more than "
                    "16 will enlarge adjustment.."));
CDI_GROUP_ENTRY(
    max_adjust, nmranet::Uint8ConfigEntry, Name("Maximum adjustment"),
    Default(16),
    Description("What should be the maximum adjustment per cycle."));
CDI_GROUP_ENTRY(
    min_power, nmranet::Uint8ConfigEntry, Name("Minimum output power"),
    Default(0x30),
    Description("Minimum power value that the motor needs to turn."));
CDI_GROUP_END();

class SpeedFeedbackController : private DefaultConfigUpdateListener {
 public:
  SpeedFeedbackController(FeedbackParams par) : par_(par), flushState_(1) {}

  void reset_to_zero() { flushState_ = 1; }

  void set_desired_rate(uint16_t rate) { desiredRate_ = rate; }

  uint8_t measure_tick(uint16_t measurement) {
    if (flushState_) {
      flushState_ = 0;
      measEwma_.reset_state(measurement);
      lastOutputValue_ = 0;
    } else {
      measEwma_.add_value(measurement);
    }
    // Observed difference
    float diff = desiredRate_ - measEwma_.avg();
    diff *= adjustParam_;
    diff /= 16;
    // Action to take
    int delta;
    if (diff < -maxDiff_) {
      delta = -maxDiff_;
    } else if (diff > maxDiff_) {
      delta = maxDiff_;
    } else {
      delta = diff;
    }
    int newValue = lastOutputValue_;
    newValue += delta;
    if (newValue < 0 || !desiredRate_) newValue = 0;
    if (newValue > 255) newValue = 255;
    if (newValue < minPower_ && desiredRate_) newValue = minPower_;
    lastOutputValue_ = newValue;

    debugValue_ = (uint64_t(desiredRate_) << 40) |
                  (uint64_t(lastOutputValue_) << 32) |
                  (uint64_t(measEwma_.avg()) << 16) | (uint64_t(measurement));

    return newValue;
  }

  uint64_t debug_value() { return debugValue_; }

 private:
  UpdateAction apply_configuration(int fd, bool initial_load,
                                   BarrierNotifiable *done) override {
    AutoNotify an(done);
    float raw_alpha = 256 - par_.integrate().read(fd);
    raw_alpha /= 256;
    measEwma_.set_alpha(raw_alpha);
    maxDiff_ = par_.max_adjust().read(fd);
    adjustParam_ = par_.adjust_param().read(fd);
    minPower_ = par_.min_power().read(fd);
    return UPDATED;
  }

  /// Clears configuration file and resets the configuration settings to
  /// factory value.
  ///
  /// @param fd is the file descriptor for the EEPROM file. The current
  /// offset in this file is unspecified, callees must do lseek.
  void factory_reset(int fd) override {
    par_.integrate().write(fd, par_.integrate_options().defaultvalue());
    par_.adjust_param().write(fd, par_.adjust_param_options().defaultvalue());
    par_.max_adjust().write(fd, par_.max_adjust_options().defaultvalue());
    par_.min_power().write(fd, par_.min_power_options().defaultvalue());
  }

  /// Configuration parameters.
  FeedbackParams par_;
  /// Stores the EWMA of the measured values.
  AbsEwma measEwma_{0.8};
  // alpha value of the integrating component. alpha = 0 turns of integrating,
  // alpha = 0.99 will make very very slow integrating compensation.
  //float integralAlpha_{0.2};
  // moved to measurementEwma_

  /// What is the maximum value to adjust per iteration.
  uint8_t maxDiff_{16};
  /// Fixed-point representation of the amplification of the adjustment. The
  /// decimal dot is between the nibbles.
  uint8_t adjustParam_{16};
  /// Config: minimum output power.
  uint8_t minPower_{0};
  uint64_t debugValue_{0};
  /// What is the desired value of the measured values.
  uint16_t desiredRate_{0};
  /// What value did we output last time.
  uint8_t lastOutputValue_{0};
  /// Requests zeroing the internal state.
  uint8_t flushState_ : 1;
};

#endif  // _BRACZ_CUSTOM_SPEEDFEEDBACKCONTROLLER_HXX_
