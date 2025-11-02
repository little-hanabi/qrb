/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Reader.h"
#include "Quadrilateral.h"

#include <utility>
#include <optional>

namespace ZXing::QRCode {

inline std::optional<int> qr_ecc;
inline std::optional<int> qr_version;

class Reader : public ZXing::Reader
{
public:
	using ZXing::Reader::Reader;

	std::pair<std::vector<std::vector<uint8_t>>, std::vector<QuadrilateralI>> decode(const BinaryBitmap& image, bool single) const override;
};

} // namespace ZXing::QRCode
