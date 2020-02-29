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
 * \file main.cxx
 *
 * A simple application to demonstrate asynchronous interfaces.
 *
 * @author Balazs Racz
 * @date 7 Dec 2013
 */

#include <stdio.h>
#include <unistd.h>

#include "commandstation/UpdateProcessor.hxx"
#include "mobilestation/MobileStationTraction.hxx"
#include "commandstation/TrainDb.hxx"
#include "commandstation/AllTrainNodes.hxx"
#include "openlcb/SimpleNodeInfoMockUserFile.hxx"
#include "openlcb/SimpleStack.hxx"
#include "openlcb/TractionTestTrain.hxx"
#include "openlcb/TractionTrain.hxx"
#include "dcc/FakeTrackIf.hxx"
#include "executor/PoolToQueueFlow.hxx"
#include "custom/LoggingBit.hxx"
#include "openlcb/TractionCvSpace.hxx"
#include "utils/JSTcpClient.hxx"


static const openlcb::NodeID NODE_ID = 0x050101011440ULL;
openlcb::SimpleCanStack stack(NODE_ID);
OVERRIDE_CONST(num_memory_spaces, 12);
OVERRIDE_CONST(local_nodes_count, 0);
OVERRIDE_CONST(local_alias_cache_size, 5000);
OVERRIDE_CONST(remote_alias_cache_size, 5000);
// Forces all trains to be our responsibility.
OVERRIDE_CONST(mobile_station_train_count, 0);

namespace commandstation {
extern const struct const_traindb_entry_t const_lokdb[];
const struct const_traindb_entry_t const_lokdb[] = {
  // 0
  //  { 51, { LIGHT, TELEX, FN_NONEXISTANT, SHUNT, ABV },
  //  "BR 260417", DCC_28 },  // ESU LokPilot 3.0
  // 1
  //{ 66, { LIGHT },
  //"Re 6/6 11665", DCC_128 },
  { 31, { LIGHT, FN_NONEXISTANT, TELEX, ABV ,}, "F7 31", DCCMODE_FAKE_DRIVE },
  { 123, { LIGHT, FN_NONEXISTANT, TELEX, ABV ,}, "Example 123", DCC_128 },

    {7000, {0xff}, "Node test 7000", DCC_28},
    {5000, {0xff}, "TT 5000", DCC_28},
    {5104, {0xff}, "TT 5104", DCC_28},
    {5100, {0xff}, "TT 5100", DCC_28},
  { 0, {0,}, "", 0},
  { 0, {0,}, "", 0},
};
extern const size_t const_lokdb_size;
const size_t const_lokdb_size = sizeof(const_lokdb) / sizeof(const_lokdb[0]);
}  // namespace commandstation

openlcb::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char* const openlcb::SNIP_DYNAMIC_FILENAME =
    openlcb::MockSNIPUserFile::snip_user_file_path;

dcc::FakeTrackIf track_if(stack.service(), 2);
commandstation::UpdateProcessor cs_loop(stack.service(), &track_if);
PoolToQueueFlow<Buffer<dcc::Packet>> pool_translator(stack.service(), track_if.pool(), &cs_loop);

LoggingBit track_on_off(openlcb::Defs::EMERGENCY_OFF_EVENT,
                        openlcb::Defs::CLEAR_EMERGENCY_OFF_EVENT,
                        "emergency_stop");
openlcb::BitEventConsumer powerbit(&track_on_off);

openlcb::TrainService traction_service(stack.iface());

commandstation::TrainDb train_db;
commandstation::AllTrainNodes all_train_nodes(&train_db, &traction_service, stack.info_flow(), stack.memory_config_handler());

dcc::RailcomHubFlow railcom_hub(stack.service());
openlcb::TractionCvSpace traction_cv(stack.memory_config_handler(), &track_if, &railcom_hub, openlcb::MemoryConfigDefs::SPACE_DCC_CV);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[]) {
  new JSTcpClient(stack.can_hub(), "localhost", 12021);

  stack.loop_executor();
  return 0;
}
