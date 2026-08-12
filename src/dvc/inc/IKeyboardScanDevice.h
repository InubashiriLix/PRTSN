#pragma once
#include "src/fw/inc/Result.h"
#include "src/fw/inc/std_err.h"

#include <cstddef>
#include <cstdint>

struct KeyboardScanFrame
{
    const uint8_t* pressedBitmap;
    size_t         pressedBitmapSize;
    uint8_t        rows;
    uint8_t        cols;
    uint16_t       slotCount;
    uint32_t       timestemp;
    bool           changed;
};

class IKeyboardScanDevice
{
public:
    virtual ~IKeyboardScanDevice() = default;

    virtual Result<void, StdErrors> setup() = 0;
    virtual Result<void, StdErrors> end()   = 0;
    virtual Result<void, StdErrors> reset() = 0;

    virtual Result<KeyboardScanFrame, StdErrors> scan()     = 0;
    virtual Result<KeyboardScanFrame, StdErrors> snapshot() = 0;

    virtual uint8_t  getRowNum() const = 0;
    virtual uint8_t  getColNum() const = 0;
    virtual uint16_t slotCount() const = 0;
};
