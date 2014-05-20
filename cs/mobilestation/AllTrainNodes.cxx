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
 * \file AllTrainNodes.hxx
 *
 * A class that instantiates every train node from the TrainDb.
 *
 * @author Balazs Racz
 * @date 20 May 2014
 */

#include "mobilestation/AllTrainNodes.hxx"

#include "dcc/Loco.hxx"
#include "mobilestation/TrainDb.hxx"
#include "nmranet/TractionTrain.hxx"

namespace mobilestation {

AllTrainNodes::AllTrainNodes(TrainDb* db,
                             NMRAnet::TrainService* traction_service) {
  trainImpl_.resize(const_lokdb_size);
  trainNode_.resize(const_lokdb_size);
  for (unsigned train_id = 0; train_id < const_lokdb_size; ++train_id) {
    if (!db->is_train_id_known(train_id)) continue;
    unsigned address = db->get_traction_node(train_id) & 0xffffffff;
    unsigned mode = db->get_drive_mode(train_id);
    switch (mode & 7) {
      case MARKLIN_OLD:
      case MARKLIN_NEW:
      case MFX: {
        trainImpl_[train_id] = new dcc::MMOldTrain(dcc::MMAddress(address));
        break;
      }
      case DCC_28:
      case DCC_128: {
        trainImpl_[train_id] =
            new dcc::Dcc28Train(dcc::DccShortAddress(address));
        break;
      }
      default:
        DIE("Unhandled train drive mode.");
    }
    if (trainImpl_[train_id]) {
      trainNode_[train_id] =
          new NMRAnet::TrainNode(traction_service, trainImpl_[train_id]);
    }
  }
}

AllTrainNodes::~AllTrainNodes() {
  for (auto* n : trainNode_) {
    delete n;
  }
  for (auto* t : trainImpl_) {
    delete t;
  }
}

}  // namespace mobilestation
