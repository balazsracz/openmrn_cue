/** \copyright
 * Copyright (c) 2016, Balazs Racz
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
 * \file FindTrainNode.hxx
 *
 * A class that helps finding a train on the network.
 *
 * @author Balazs Racz
 * @date 7 Feb 2016
 */

#ifndef _BRACZ_COMMANDSTATION_FINDTRAINNODE_HXX_
#define _BRACZ_COMMANDSTATION_FINDTRAINNODE_HXX_

#include "commandstation/TrainDb.hxx"
#include "executor/StateFlow.hxx"
#include "nmranet/If.hxx"
#include "nmranet/Node.hxx"

namespace commandstation {

struct FindTrainNodeRequest {
    
    enum TrainType {
        UNKNOWN = 0,
        DCC_SHORT,
        DCC_LONG,
        MARKLIN_OLD,
        MARKLIN_NEW,
    };

    void reset(TrainType type, int address) {
        this->address = address;
        this->type = type;
    }
    
    TrainType type;
    int address;

    BarrierNotifiable done;

    int resultCode;
    nmranet::NodeID nodeId;
};

class FindTrainNode : public StateFlow<Buffer<FindTrainNodeRequest>, QList<1>> {
public:
    /// @param node is a local node from which queries can be sent out to the
    /// network. @param train_db is the allocated local train store.
    FindTrainNode(nmranet::Node* node, TrainDb* train_db)
        : StateFlow<Buffer<FindTrainNodeRequest>, QList<1>>(node->iface()),
        trainDb_(train_db),
        node_(node) {}

private:
    Action entry() {
        return return_with_error(nmranet::Defs::ERROR_UNIMPLEMENTED);
    }

    Action return_ok(nmranet::NodeID nodeId)
    {
        input()->nodeId = nodeId;
        return return_with_error(0);
    }

    Action return_with_error(int error)
    {
        if (error) {
            input()->nodeId = 0;
        }
        input()->resultCode = error;
        return_buffer();
        return exit();
    }

    FindTrainNodeRequest *input()
    {
        return message()->data();
    }

    nmranet::If *iface()
    {
        // We know that the service pointer is the node's interface from the
        // constructor.
        return static_cast<nmranet::If *>(service());
    }

    TrainDb* trainDb_;
    nmranet::Node* node_;
};

}  // namespace commandstation

#endif // _BRACZ_COMMANDSTATION_FINDTRAINNODE_HXX_

