/** \copyright
 * Copyright (c) 2013, Balazs Racz
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
 * \file hw_init.cxx
 * low-level hardware initialization for the LPC2387 pandaII board.
 *
 * @author Balazs Racz
 * @date 13 April 2013
 */

#include "utils/logging.h"
#include "src/cs_config.h"

#include "LPC23xx.h"
#include "mbed.h"
#include "utils/pipe.hxx"
#include "can.h"
#include "FreeRTOSConfig.h"
#include "src/mbed_i2c_update.hxx"
#include "src/base.h"
#include "src/automata_control.h"
#include "src/host_packet.h"

#include "src/usb_proto.h"
#include "src/updater.hxx"

extern const unsigned long long NODE_ADDRESS;
#ifdef SECOND
const unsigned long long NODE_ADDRESS = 0x050101011436ULL;
#else
const unsigned long long NODE_ADDRESS = 0x050101011431ULL;
#endif


DigitalIn startpin(P1_4);

DEFINE_PIPE(can_pipe0, sizeof(struct can_frame));
DEFINE_PIPE(can_pipe1, sizeof(struct can_frame));
VIRTUAL_DEVTAB_ENTRY(canp0v0, can_pipe0, "/dev/canp0v0", 16);
VIRTUAL_DEVTAB_ENTRY(canp0v1, can_pipe0, "/dev/canp0v1", 16);

VIRTUAL_DEVTAB_ENTRY(canp1v0, can_pipe1, "/dev/canp1v0", 16);
VIRTUAL_DEVTAB_ENTRY(canp1v1, can_pipe1, "/dev/canp1v1", 16);

//I2C i2c(P0_10, P0_11); for panda CS
I2C i2c(P0_27, P0_28);

I2COutUpdater extender0(&i2c, 0x21, {}, get_state_byte(1, OFS_IOA), 2);
I2COutUpdater extender1(&i2c, 0x22, {}, get_state_byte(2, OFS_IOA), 2);
I2COutUpdater extender2(&i2c, 0x23, {}, get_state_byte(3, OFS_IOA), 2);

I2CInUpdater in_extender0(&i2c, 0x21, {}, get_state_byte(1, OFS_IOB), 1, 2);
I2CInUpdater in_extender1(&i2c, 0x22, {}, get_state_byte(2, OFS_IOB), 1, 2);
I2CInUpdater in_extender2(&i2c, 0x23, {}, get_state_byte(3, OFS_IOB), 1, 2);

SynchronousUpdater i2c_updater({&extender0, &extender1, &extender2, &in_extender0, &in_extender1, &in_extender2});

extern "C" {

extern uint32_t blinker_pattern;

void enable_fiq(void);
void enable_fiq_only(void);
void diewith(unsigned long);

/** Initializes the timer responsible for the blinking hardware and sets
 *   initial pattern.
 *
 *  \param pattern is a blinking pattern, each bit will be shifted to the
 *  output LED every 125 ms.
 */
void setblink(uint32_t pattern)
{
    blinker_pattern = pattern;
    // clock = raw clock
    PCLKSEL0 = (PCLKSEL0 & (~(0x3<<4))) | (0x01 << 4);
    LPC_TIM1->TCR = 2;  // stop & reset timer
    LPC_TIM1->CTCR = 0; // timer mode
    // prescale to 1 ms per tick
    LPC_TIM1->PR = configCPU_CLOCK_HZ / 1000;
    LPC_TIM1->MR0 = 125;
    LPC_TIM1->MCR = 3;  // reset and interrupt on match 0

    LPC_VIC->IntSelect |= (1<<5);   // FIQ
    LPC_VIC->IntEnable = (1<<5);   // FIQ

    enable_fiq();

    LPC_TIM1->TCR = 1;  // Timer go.
}

/** Updates the blinking pattern.
 *
 *  \param pattern is a blinking pattern, each bit will be shifted to the
 *  output LED every 125 ms.
 */
void resetblink(uint32_t pattern)
{
    blinker_pattern = pattern;
    // Makes a timer event trigger immediately.
    LPC_TIM1->TC = LPC_TIM1->MR0 - 2;
}

/** Aborts the program execution and sets a particular blinking pattern.
 *
 *  \param pattern is a blinking pattern, each bit will be shifted to the
 *  output LED every 125 ms.
 * 
 *  Never returns.
 */
void diewith(unsigned long pattern)
{
    enable_fiq_only();
    setblink(pattern);
    for (;;)
    {
    }
}

/** Initializes the processor hardware.
 */
void hw_init(void)
{
    LPC_GPIO3->FIODIR=(1<<26);
    LPC_GPIO3->FIOCLR=(1<<26);
    setblink(0x8000000AUL);
#ifndef SECOND
    // Waits for the start button to be pressed.
    while(!startpin)
    {
    }
#endif
    resetblink(1);
}


void log_output(char* buf, int size) {
    if (size <= 0) return;
    PacketBase pkt(size + 1);
    pkt[0] = CMD_VCOM1;
    memcpy(pkt.buf() + 1, buf, size);
    PacketQueue::instance()->TransmitPacket(pkt);
}

}  // extern C
