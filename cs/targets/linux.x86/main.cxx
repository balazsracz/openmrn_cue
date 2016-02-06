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
#include "nmranet/SimpleNodeInfoMockUserFile.hxx"
#include "nmranet/SimpleStack.hxx"
#include "nmranet/TractionTestTrain.hxx"
#include "nmranet/TractionTrain.hxx"
#include "custom/HostProtocol.hxx"

static const nmranet::NodeID NODE_ID = 0x050101011440ULL;
nmranet::SimpleCanStack stack(NODE_ID);
OVERRIDE_CONST(num_memory_spaces, 4);
OVERRIDE_CONST(local_nodes_count, 6);
OVERRIDE_CONST(local_alias_cache_size, 6);
OVERRIDE_CONST(remote_alias_cache_size, 30);
// Forces all trains to be our responsibility.
OVERRIDE_CONST(mobile_station_train_count, 0);

nmranet::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char* const nmranet::SNIP_DYNAMIC_FILENAME =
    nmranet::MockSNIPUserFile::snip_user_file_path;

nmranet::TrainService traction_service(stack.iface());

/*
nmranet::LoggingTrain train_a(43);
nmranet::LoggingTrain train_b(22);
nmranet::LoggingTrain train_c(465);
nmranet::TrainNode node_a(&traction_service, &train_a);
nmranet::TrainNode node_b(&traction_service, &train_b);
nmranet::TrainNode node_c(&traction_service, &train_c);
*/

CanHubFlow can_hub1(stack.service());
CanIf can_if1(stack.service(), &can_hub1);

commandstation::TrainDb train_db;
mobilestation::MobileStationTraction mosta_traction(&can_if1, stack.iface(),
                                                    &train_db, stack.node());

commandstation::AllTrainNodes all_train_nodes(&train_db, &traction_service, stack.info_flow(), stack.memory_config_handler());

namespace commandstation {
const struct const_loco_db_t const_lokdb[] = {
    // 0
  { 43, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, FNT11, ABV,  0xff, },
    "Am 843 093-6", FAKE_DRIVE },
  { 22, { 0, 3, 4,  0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "RE 460 TSR", FAKE_DRIVE }, // todo: there is no beamer here // LD-32 decoder
  { 465, { 0, 1, 0xff, }, { LIGHT, SPEECH,  0xff, },
    "Jim's steam", FAKE_DRIVE | PUSHPULL },
  {0, {0, }, {0, }, "", 0}, };

extern const size_t const_lokdb_size =
    sizeof(const_lokdb) / sizeof(const_lokdb[0]);
}

bracz_custom::HostClient host_client(stack.dg_service(), stack.node(), &can_hub1);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[]) {
  stack.connect_tcp_gridconnect_hub("localhost", 12021);

  stack.loop_executor();
  return 0;
}
