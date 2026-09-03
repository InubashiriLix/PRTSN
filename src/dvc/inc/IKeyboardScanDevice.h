#pragma once
#include "src/fw/inc/Result.h"
#include "src/fw/inc/std_err.h"
#include "src/prt/HidProtocol.h"

class IKeyboardScanDevice
{
public:
    virtual ~IKeyboardScanDevice() = default;

    virtual Result<void, StdErrors> setup() = 0;
    virtual Result<void, StdErrors> end()   = 0;
    virtual Result<void, StdErrors> reset() = 0;

    virtual Result<prt_hid::KeyboardReport, StdErrors> scan()     = 0;
    virtual Result<prt_hid::KeyboardReport, StdErrors> snapshot() = 0;
};
