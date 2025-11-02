/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRErrorCorrectionLevel.h"

#include <cassert>

namespace ZXing::QRCode {

const char* ToString(ErrorCorrectionLevel l)
{
	assert(l != ErrorCorrectionLevel::Invalid);
	static const char* const LEVEL_STR[] = {"L", "M", "Q", "H", nullptr};
	return LEVEL_STR[static_cast<int>(l)];
}

ErrorCorrectionLevel ECLevelFromBits(int bits)
{
	constexpr ErrorCorrectionLevel LEVEL_FOR_BITS[] = {ErrorCorrectionLevel::Medium, ErrorCorrectionLevel::Low,
													   ErrorCorrectionLevel::High, ErrorCorrectionLevel::Quality};
	return LEVEL_FOR_BITS[bits & 0x3];
}

int BitsFromECLevel(ErrorCorrectionLevel l)
{
	assert(l != ErrorCorrectionLevel::Invalid);
	static const int BITS[] = {1, 0, 3, 2, -1};
	return BITS[(int)l];
}

} // namespace ZXing::QRCode
