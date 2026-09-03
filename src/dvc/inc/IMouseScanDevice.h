#pragma once

#include "src/fw/inc/Result.h"
#include "src/fw/inc/std_err.h"
#include "src/prt/HidProtocol.h"

class IMouseScanDevice
{
public:
    virtual ~IMouseScanDevice() = default;

    virtual Result<void, StdErrors> setup() = 0;
    virtual Result<void, StdErrors> end()   = 0;
    virtual Result<void, StdErrors> reset() = 0;

    // Mouse motion is relative: a successful scan consumes the returned x/y/wheel deltas.
    virtual Result<prt_hid::MouseReport, StdErrors> scan() = 0;
};
