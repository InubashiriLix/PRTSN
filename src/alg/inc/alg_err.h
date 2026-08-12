#pragma once

#include "src/fw/inc/Result.h"

enum class MatrixErrorCode
{
    INDEX_OUT_OF_BOUNDS = 1,
};

using MatrixError = ErrorSet<MatrixErrorCode::INDEX_OUT_OF_BOUNDS>;

enum class MapMatrixErrorCode
{
    INDEX_OUT_OF_BOUNDS = 1,
    INVALID_DIMENSIONS  = 2,
};

using MapMatrixError = ErrorSet<
    MapMatrixErrorCode::INDEX_OUT_OF_BOUNDS,
    MapMatrixErrorCode::INVALID_DIMENSIONS>;
