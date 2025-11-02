/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2023 gitlost
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRBitMatrixParser.h"

#include "BitArray.h"
#include "BitMatrix.h"
#include "ByteArray.h"
#include "QRDataMask.h"
#include "QRFormatInformation.h"
#include "QRVersion.h"

#include <utility>

namespace ZXing::QRCode {

static bool getBit(const BitMatrix& bitMatrix, int x, int y, bool mirrored = false)
{
	return mirrored ? bitMatrix.get(y, x) : bitMatrix.get(x, y);
}

const Version* ReadVersion(const BitMatrix& bitMatrix, Type type)
{
	if (type == Type::Model2 && Version::HasValidSize(bitMatrix, type))
		return Version::Model2(Version::Number(bitMatrix));
	else return nullptr;
}

FormatInformation ReadFormatInformation(const BitMatrix& bitMatrix)
{
	// Read top-left format info bits
	int formatInfoBits1 = 0;
	for (int x = 0; x < 6; x++)
		AppendBit(formatInfoBits1, getBit(bitMatrix, x, 8));
	// .. and skip a bit in the timing pattern ...
	AppendBit(formatInfoBits1, getBit(bitMatrix, 7, 8));
	AppendBit(formatInfoBits1, getBit(bitMatrix, 8, 8));
	AppendBit(formatInfoBits1, getBit(bitMatrix, 8, 7));
	// .. and skip a bit in the timing pattern ...
	for (int y = 5; y >= 0; y--)
		AppendBit(formatInfoBits1, getBit(bitMatrix, 8, y));

	// Read the top-right/bottom-left pattern including the 'Dark Module' from the bottom-left
	// part that has to be considered separately when looking for mirrored symbols.
	// See also FormatInformation::DecodeQR
	int dimension = bitMatrix.height();
	int formatInfoBits2 = 0;
	for (int y = dimension - 1; y >= dimension - 8; y--)
		AppendBit(formatInfoBits2, getBit(bitMatrix, 8, y));
	for (int x = dimension - 8; x < dimension; x++)
		AppendBit(formatInfoBits2, getBit(bitMatrix, x, 8));

	return FormatInformation::DecodeQR(formatInfoBits1, formatInfoBits2);
}

static ByteArray ReadQRCodewords(const BitMatrix& bitMatrix, const Version& version, const FormatInformation& formatInfo)
{
	BitMatrix functionPattern = version.buildFunctionPattern();

	ByteArray result;
	result.reserve(version.totalCodewords());
	uint8_t currentByte = 0;
	bool readingUp = true;
	int bitsRead = 0;
	int dimension = bitMatrix.height();
	// Read columns in pairs, from right to left
	for (int x = dimension - 1; x > 0; x -= 2) {
		// Skip whole column with vertical timing pattern.
		if (x == 6)
			x--;
		// Read alternatingly from bottom to top then top to bottom
		for (int row = 0; row < dimension; row++) {
			int y = readingUp ? dimension - 1 - row : row;
			for (int col = 0; col < 2; col++) {
				int xx = x - col;
				// Ignore bits covered by the function pattern
				if (!functionPattern.get(xx, y)) {
					// Read a bit
					AppendBit(currentByte,
							  GetDataMaskBit(formatInfo.dataMask, xx, y) != getBit(bitMatrix, xx, y, formatInfo.isMirrored));
					// If we've made a whole byte, save it off
					if (++bitsRead % 8 == 0)
						result.push_back(std::exchange(currentByte, 0));
				}
			}
		}
		readingUp = !readingUp; // switch directions
	}
	if (Size(result) != version.totalCodewords())
		return {};

	return result;
}

ByteArray ReadCodewords(const BitMatrix& bitMatrix, const Version& version, const FormatInformation& formatInfo)
{
	if (version.type() == Type::Model2) return ReadQRCodewords(bitMatrix, version, formatInfo);
	else return {};
}

} // namespace ZXing::QRCode
