#include "openlcb/If.hxx"
#include "address.h"

extern const openlcb::NodeID NODE_ID;
const openlcb::NodeID NODE_ID = 0x050101011400ULL | NODEID_LOW_BITS;
extern const uint16_t DEFAULT_ALIAS;
const uint16_t DEFAULT_ALIAS = 0x400 | NODEID_LOW_BITS;

#include "utils/ReflashBootloader.hxx"

extern const SegmentTable table[];

#include "payload.hxxout"

const SegmentTable table[] = {
  {(uint8_t*)0x08000000, header_arr, ARRAYSIZE(header_arr)},
  {(uint8_t*)0x0801e000, data_arr, ARRAYSIZE(data_arr)},
  {0, 0, 0},
};

