/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRWriter.h"

#include "BitMatrix.h"
#include "QREncodeResult.h"
#include "QREncoder.h"
#include "QRErrorCorrectionLevel.h"

#include <stdexcept>
#include <utility>

namespace ZXing::QRCode {

static const int QUIET_ZONE_SIZE = 4;

Writer::Writer()
	: _margin(QUIET_ZONE_SIZE),
	  _ecLevel(ErrorCorrectionLevel::Low),
	  _version(0),
	  _maskPattern(-1)
{}

BitMatrix Writer::encode(const BitArray& contents, int width, int height) const
{
	if (contents.size() == 0) {
		throw std::invalid_argument("Found empty contents");
	}

	if (width < 0 || height < 0) {
		throw std::invalid_argument("Requested dimensions are invalid");
	}

	EncodeResult code = Encode(contents, _ecLevel, _version, _maskPattern);
	return Inflate(std::move(code.matrix), width, height, _margin);
}

} // namespace ZXing::QRCode
