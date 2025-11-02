/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRReader.h"
#include "QRDecoder.h"

#include "BitMatrix.h"
#include "BitSource.h"
#include "GenericGF.h"
#include "QRBitMatrixParser.h"
#include "QRCodecMode.h"
#include "QRDataBlock.h"
#include "QRFormatInformation.h"
#include "QRVersion.h"
#include "ReedSolomonDecoder.h"
#include "StructuredAppend.h"
#include "ZXAlgorithms.h"
#include "ZXTestSupport.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace ZXing::QRCode {

/**
* <p>Given data and error-correction codewords received, possibly corrupted by errors, attempts to
* correct the errors in-place using Reed-Solomon error correction.</p>
*
* @param codewordBytes data and error correction codewords
* @param numDataCodewords number of codewords that are data bytes
* @return false if error correction fails
*/
static bool CorrectErrors(ByteArray& codewordBytes, int numDataCodewords)
{
	// First read into an array of ints
	std::vector<int> codewordsInts(codewordBytes.begin(), codewordBytes.end());

	int numECCodewords = Size(codewordBytes) - numDataCodewords;
	if (!ReedSolomonDecode(GenericGF::QRCodeField256(), codewordsInts, numECCodewords))
		return false;

	// Copy back into array of bytes -- only need to worry about the bytes that were data
	// We don't care about errors in the error-correction codewords
	std::copy_n(codewordsInts.begin(), numDataCodewords, codewordBytes.begin());
	return true;
}

/**
 * QR codes encode mode indicators and terminator codes into a constant bit length of 4.
 * Micro QR codes have terminator codes that vary in bit length but are always longer than
 * the mode indicators.
 * M1 - 0 length mode code, 3 bits terminator code
 * M2 - 1 bit mode code, 5 bits terminator code
 * M3 - 2 bit mode code, 7 bits terminator code
 * M4 - 3 bit mode code, 9 bits terminator code
 * IsTerminator peaks into the bit stream to see if the current position is at the start of
 * a terminator code.  If true, then the decoding can finish. If false, then the decoding
 * can read off the next mode code.
 *
 * See ISO 18004:2015, 7.4.1 Table 2
 *
 * @param bits the stream of bits that might have a terminator code
 * @param version the QR or micro QR code version
 */
bool IsEndOfStream(const BitSource& bits, const Version& version)
{
	const int bitsRequired = TerminatorBitsLength(version);
	const int bitsAvailable = std::min(bits.available(), bitsRequired);
	return bitsAvailable == 0 || bits.peakBits(bitsAvailable) == 0;
}

/**
* <p>QR Codes can encode text as bits in one of several modes, and can use multiple modes
* in one QR Code. This method decodes the bits back into text.</p>
*
* <p>See ISO 18004:2006, 6.4.3 - 6.4.7</p>
*/
ZXING_EXPORT_TEST_ONLY
std::vector<uint8_t> DecodeBitStream(ByteArray&& bytes, const Version& version)
{
	if (!version.isModel2()) return {};

	BitSource bits(bytes);
	std::vector<uint8_t> result;
	StructuredAppendInfo structuredAppend;
	const int modeBitLength = CodecModeBitsLength(version);

	try
	{
		while(!IsEndOfStream(bits, version)) {
			CodecMode mode;
			if (modeBitLength == 0)
				mode = CodecMode::NUMERIC; // MicroQRCode version 1 is always NUMERIC and modeBitLength is 0
			else
				mode = CodecModeForBits(bits.readBits(modeBitLength), version.type());

			const int count = bits.readBits(CharacterCountBits(mode, version.versionNumber()));
			if (mode == CodecMode::BYTE) for (int i = 0; i < count; i++) result.push_back(narrow_cast<uint8_t>(bits.readBits(8)));
			else return {};
		}
	} catch (...) { return {}; }

	return result;
}

std::vector<uint8_t> Decode(const BitMatrix& bits)
{
	if (!Version::HasValidSize(bits, Type::Model2)) return {};

	auto formatInfo = ReadFormatInformation(bits);
	if (!formatInfo.isValid()) return {};

	const Version* pversion = ReadVersion(bits, formatInfo.type());
	if (!pversion) return {};

	const Version& version = *pversion;
    if (!version.isModel2()) return {};

	// Read codewords
	ByteArray codewords = ReadCodewords(bits, version, formatInfo);
	if (codewords.empty()) return {};

	// Separate into data blocks
	std::vector<DataBlock> dataBlocks = DataBlock::GetDataBlocks(codewords, version, formatInfo.ecLevel);
	if (dataBlocks.empty()) return {};

	// Count total number of data bytes
	const auto op = [](auto totalBytes, const auto& dataBlock){ return totalBytes + dataBlock.numDataCodewords();};
	const auto totalBytes = Reduce(dataBlocks, int{}, op);
	ByteArray resultBytes(totalBytes);
	auto resultIterator = resultBytes.begin();

	// Error-correct and copy data blocks together into a stream of bytes
	for (auto& dataBlock : dataBlocks)
	{
		ByteArray& codewordBytes = dataBlock.codewords();
		int numDataCodewords = dataBlock.numDataCodewords();

		if (!CorrectErrors(codewordBytes, numDataCodewords)) return {};

		resultIterator = std::copy_n(codewordBytes.begin(), numDataCodewords, resultIterator);
	}

	// Decode the contents of that stream of bytes
	auto result = DecodeBitStream(std::move(resultBytes), version);

	if(!qr_ecc.has_value() && !qr_version.has_value() && !result.empty()) {
		qr_ecc = static_cast<int>(formatInfo.ecLevel);
		qr_version = version.versionNumber();
	}

    return result;
}

} // namespace ZXing::QRCode
