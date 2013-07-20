
#include "mbed.h"
#include "pipe.hxx"
#include "can.h"
#include "FreeRTOSConfig.h"
#include "src/mbed_i2c_update.hxx"
#include "src/base.h"
#include "src/automata_control.h"



/*
DEFINE_PIPE(can_pipe0, sizeof(struct can_frame));
VIRTUAL_DEVTAB_ENTRY(canp0v0, can_pipe0, "/dev/canp0v0", 16);
VIRTUAL_DEVTAB_ENTRY(canp0v1, can_pipe0, "/dev/canp0v1", 16);
*/

I2C i2c(P0_5, P0_4);

I2COutUpdater extender0(&i2c, 0x25, {}, get_state_byte(1, OFS_IOA), 2);
I2COutUpdater extender1(&i2c, 0x24, {}, get_state_byte(2, OFS_IOA), 2);

I2CInUpdater in_extender0(&i2c, 0x25, {}, get_state_byte(1, OFS_IOB), 1);
I2CInUpdater in_extender1(&i2c, 0x24, {}, get_state_byte(2, OFS_IOB), 1);

SynchronousUpdater i2c_updater({&extender0, &extender1, &in_extender0, &in_extender1});

extern "C" {
void modules_init() {}
}
