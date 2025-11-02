/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BitArray.h"

namespace ZXing::QRCode {

enum class ErrorCorrectionLevel;
class EncodeResult;

EncodeResult Encode(const BitArray& dataBits, ErrorCorrectionLevel ecLevel, int versionNumber, int maskPattern = -1);

} // namespace ZXing::QRCode
