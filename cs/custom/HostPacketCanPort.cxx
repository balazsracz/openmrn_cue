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
 * \file HostPacketCanPort.cxx
 *
 * Provides a bridge between host_packet sending / receiving CMD_CAN_PACKET
 * messages and a CanHubFlow.
 *
 * @author Balazs Racz
 * @date 15 May 2014
 */

#include "custom/HostPacketCanPort.hxx"

#include <memory>

#include "utils/Hub.hxx"
#include "custom/MCPCanFrameFormat.hxx"
#include "src/pic_can.h"
#include "utils/singleton.hxx"
#include "src/host_packet.h"
#include "src/usb_proto.h"


namespace bracz_custom {

class HostPacketBridge;

class HostPacketBridge : private CanHubPortInterface {
 public:
  HostPacketBridge(CanHubFlow* device)
      : device_(device) {
    device_->register_port(this);
  }

  ~HostPacketBridge() {
    device_->unregister_port(this);
  }

  void from_host(const uint8_t* buf, unsigned size) {
    auto* b = device_->alloc();
    b->data()->skipMember_ = this;
    struct can_frame* f = b->data()->mutable_frame();
    memset(f, 0, sizeof(*f));
    mcp_to_frame(buf, f);
    device_->send(b);
  }

  // Packet from the device to the host.
  void send(Buffer<CanHubData>*b, unsigned) OVERRIDE {
    if (!IS_CAN_FRAME_EFF(b->data()->frame()) ||
        (GET_CAN_FRAME_ID_EFF(b->data()->frame()) & ~1) == 0x0c000380) {
      // Rudimentary filter to remove noise from keepalive and dcc packets.
      b->unref();
      return;
    }

    uint8_t buf[15];
    frame_to_mcp(*b->data(), buf + CAN_START);
    b->unref();
    buf[0] = buf[CAN_LEN] + 5 + 1;
    buf[1] = CMD_CAN_PKT;
    PacketQueue::instance()->TransmitConstPacket(buf);
  }

 private:
  CanHubFlow* device_;
};

static std::unique_ptr<HostPacketBridge> bridge;

void init_host_packet_can_bridge(CanHubFlow* device) {
  bridge.reset(new HostPacketBridge(device));
}

void handle_can_packet_from_host(const uint8_t* buf, unsigned size) {
  HASSERT(bridge.get());
  bridge->from_host(buf, size);
}

}  // namespace bracz_custom
