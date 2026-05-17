#pragma once

#include <cstdint>

enum NodeState : uint8_t
{
    BOOTING = 0,
    RUNNING = 1,
    ERROR   = 2,
};

struct NodeInfo
{
    const char* projectName;
    const char* fullName;
    const char* boardName;
    const char* version;
    const char* nodeName;
    const char* nodeId;
    NodeState   state;

    void      updateNodeState(NodeState newState);
    void      getNodeState(NodeState& outState) const;
    NodeState getNodeState() const;
};
