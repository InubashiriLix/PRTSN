#include "NodeInfo.h"

NodeState NodeInfo::getNodeState() const {
    return state;
}

void NodeInfo::getNodeState(NodeState& outState) const {
    outState = state;
}

void NodeInfo::updateNodeState(NodeState newState) {
    state = newState;
}
