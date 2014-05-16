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
 * \file MCPCanFrameFormat.cxx
 *
 * Conversion routines between struct can_frame and an MCP2515-compatible CAN
 * buffer.
 *
 * @author Balazs Racz
 * @date 15 May 2014
 */

#include "custom/MCPCanFrameFormat.hxx"

#include <string.h>
#include "nmranet_can.h"
#include "src/pic_can.h"

namespace bracz_custom {

void mcp_to_frame(const uint8_t* mcp_frame, struct can_frame* can_frame) {
    bool extended = (mcp_frame[1] & 0x08);

    CLR_CAN_FRAME_RTR(*can_frame);
    CLR_CAN_FRAME_ERR(*can_frame);
    if (extended) {
        uint32_t id = mcp_frame[0];
        id <<= 3;
        id |= mcp_frame[1] >> 5;
        id <<= 2;
        id |= mcp_frame[1] & 3;
        id <<= 8;
        id |= mcp_frame[2];
        id <<= 8;
        id |= mcp_frame[3];
        SET_CAN_FRAME_EFF(*can_frame);
        SET_CAN_FRAME_ID_EFF(*can_frame, id);
    } else {
        uint32_t id = mcp_frame[0];
        id <<= 3;
        id |= mcp_frame[1] >> 5;
        CLR_CAN_FRAME_EFF(*can_frame);
        SET_CAN_FRAME_ID(*can_frame, id);
    }
    int len = mcp_frame[4];
    can_frame->can_dlc = len;
    memcpy(can_frame->data, mcp_frame + 5, len);
}

void frame_to_mcp(const struct can_frame& frame, uint8_t* mcp_frame) {
  // the constants in pic_can.h are offset by this much. This compensates for
  // the offset.
  uint8_t* pkt = mcp_frame - CAN_START;
  pkt[CAN_LEN] = frame.can_dlc;
  memcpy(&pkt[CAN_D0], frame.data, pkt[CAN_LEN]);
  if (IS_CAN_FRAME_EFF(frame)) {
    uint32_t id = GET_CAN_FRAME_ID_EFF(frame);
    pkt[CAN_EIDL] = id & 0xff;
    id >>= 8;
    pkt[CAN_EIDH] = id & 0xff;
    id >>= 8;
    pkt[CAN_SIDL] = 0x08;
    pkt[CAN_SIDL] |= id & 3;
    id >>= 2;
    pkt[CAN_SIDL] |= (id & 7) << 5;
    id >>= 3;
    pkt[CAN_SIDH] = id & 0xff;
  } else {
    uint32_t id = GET_CAN_FRAME_ID(frame);
    pkt[CAN_SIDH] = id >> 3;
    pkt[CAN_SIDL] = id << 5;
  }
}

}  // namespace bracz_custom
