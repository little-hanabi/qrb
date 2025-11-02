/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRReader.h"

#include "BinaryBitmap.h"
#include "Quadrilateral.h"
#include "QRDetector.h"
#include "QRDecoder.h"

#include <utility>

namespace ZXing::QRCode {

std::pair<std::vector<std::vector<uint8_t>>, std::vector<QuadrilateralI>> Reader::decode(const BinaryBitmap& image, const bool single) const
{
	auto binImg = image.getBitMatrix();
	if (binImg == nullptr) return {};

	std::pair<std::vector<std::vector<uint8_t>>, std::vector<QuadrilateralI>> result;

	auto FP = FindFinderPatterns(*binImg, true);
	for (const auto& pattern : GenerateFinderPatternSets(FP)) {
		const auto detectorResult = SampleQR(*binImg, pattern);
		const auto decoderResult = Decode(detectorResult.bits());

		if (detectorResult.isValid() && !decoderResult.empty()) {
			result.first.push_back(decoderResult);
			result.second.push_back(detectorResult.position());
			if (single) return result;
		}
	}
	
	return result;
}

} // namespace ZXing::QRCode
