#include "pipe/pipe.hxx"
#include "nmranet_can.h"

extern const unsigned long long NODE_ADDRESS;
const unsigned long long NODE_ADDRESS = 0x050101011430ULL;

DEFINE_PIPE(can_pipe0, sizeof(struct can_frame));
