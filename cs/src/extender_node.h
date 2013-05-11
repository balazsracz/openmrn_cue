#ifndef _BRACZ_EXTENDER_NODE_H
#define _BRACZ_EXTENDER_NODE_H


class ExtenderNode {
 public:
    //! Sends a command to an extender node. The response is not waited for.
    virtual void SendCommand(const PacketBase& pkt) = 0;
};


extern ExtenderNode* aux_power_node;

#endif //  _BRACZ_EXTENDER_NODE_H
