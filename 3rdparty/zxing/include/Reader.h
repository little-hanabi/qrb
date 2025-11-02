/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ReaderOptions.h"
#include "Quadrilateral.h"

#include <utility>

namespace ZXing {

class BinaryBitmap;
class ReaderOptions;

class Reader
{
protected:
	const ReaderOptions& _opts;

public:
	const bool supportsInversion;

	explicit Reader(const ReaderOptions& opts, bool supportsInversion = false) : _opts(opts), supportsInversion(supportsInversion) {}
	explicit Reader(ReaderOptions&& opts) = delete;
	virtual ~Reader() = default;

	virtual std::pair<std::vector<std::vector<uint8_t>>, std::vector<QuadrilateralI>> decode(const BinaryBitmap& image, bool single) const = 0;
};

} // ZXing
