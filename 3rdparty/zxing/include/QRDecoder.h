/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>
#include <cstdint>

namespace ZXing {

class BitMatrix;

namespace QRCode {

std::vector<uint8_t> Decode(const BitMatrix& bits);

} // QRCode
} // ZXing
