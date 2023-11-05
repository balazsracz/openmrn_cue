#include "openlcb/If.hxx"
#include "address.h"

#ifndef NODEID_HIGH_BITS
#define NODEID_HIGH_BITS 0x14
#endif

extern const openlcb::NodeID NODE_ID;
/// WARNING: this is manually endian-swapped, because the underlying flash will
/// be used as a memory space.
const openlcb::NodeID NODE_ID =
    (uint64_t(NODEID_LOW_BITS) << 40) |
    (uint64_t(NODEID_HIGH_BITS) << 32) |
    0x01010105;
