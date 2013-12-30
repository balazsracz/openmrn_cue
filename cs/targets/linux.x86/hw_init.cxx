#include "utils/pipe.hxx"
#include "nmranet_can.h"

extern const unsigned long long NODE_ADDRESS;
const unsigned long long NODE_ADDRESS = 0x050101011430ULL;
extern Executor g_executor;

//DEFINE_PIPE(can_pipe0, &g_executor, sizeof(struct can_frame));
