/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "QREncoder.h"

#include "BitArray.h"
#include "GenericGF.h"
#include "QREncodeResult.h"
#include "QRErrorCorrectionLevel.h"
#include "QRMaskUtil.h"
#include "QRMatrixUtil.h"
#include "ReedSolomonEncoder.h"
#include "ZXTestSupport.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ZXing::QRCode {

/**
* Append mode info. On success, store the result in "bits".
*/
ZXING_EXPORT_TEST_ONLY
void AppendModeInfo(CodecMode mode, BitArray& bits)
{
	bits.appendBits(static_cast<int>(mode), 4);
}


/**
* Append length info. On success, store the result in "bits".
*/
ZXING_EXPORT_TEST_ONLY
void AppendLengthInfo(int numLetters, const Version& version, CodecMode mode, BitArray& bits)
{
	int numBits = CharacterCountBits(mode, version.versionNumber());
	if (numLetters >= (1 << numBits)) {
		throw std::invalid_argument(std::to_string(numLetters) + " is bigger than " + std::to_string((1 << numBits) - 1));
	}
	bits.appendBits(numLetters, numBits);
}

/**
* @return true if the number of input bits will fit in a code with the specified version and
* error correction level.
*/
static bool WillFit(int numInputBits, const Version& version, ErrorCorrectionLevel ecLevel) {
	// In the following comments, we use numbers of Version 7-H.
	// numBytes = 196
	int numBytes = version.totalCodewords();
	// getNumECBytes = 130
	auto& ecBlocks = version.ecBlocksForLevel(ecLevel);
	int numEcBytes = ecBlocks.totalCodewords();
	// getNumDataBytes = 196 - 130 = 66
	int numDataBytes = numBytes - numEcBytes;
	int totalInputBytes = (numInputBits + 7) / 8;
	return numDataBytes >= totalInputBytes;
}

/**
* Terminate bits as described in 8.4.8 and 8.4.9 of JISX0510:2004 (p.24).
*/
ZXING_EXPORT_TEST_ONLY
void TerminateBits(int numDataBytes, BitArray& bits)
{
	int capacity = numDataBytes * 8;
	if (bits.size() > capacity) {
		throw std::invalid_argument("data bits cannot fit in the QR Code" + std::to_string(bits.size()) + " > "
									+ std::to_string(capacity));
	}
	for (int i = 0; i < 4 && bits.size() < capacity; ++i) {
		bits.appendBit(false);
	}
	// Append termination bits. See 8.4.8 of JISX0510:2004 (p.24) for details.
	// If the last byte isn't 8-bit aligned, we'll add padding bits.
	int numBitsInLastByte = bits.size() & 0x07;
	if (numBitsInLastByte > 0) {
		for (int i = numBitsInLastByte; i < 8; i++) {
			bits.appendBit(false);
		}
	}
	// If we have more space, we'll fill the space with padding patterns defined in 8.4.9 (p.24).
	int numPaddingBytes = numDataBytes - bits.sizeInBytes();
	for (int i = 0; i < numPaddingBytes; ++i) {
		bits.appendBits((i & 0x01) == 0 ? 0xEC : 0x11, 8);
	}
	if (bits.size() != capacity) {
		throw std::invalid_argument("Bits size does not equal capacity");
	}
}

struct BlockPair
{
	ByteArray dataBytes;
	ByteArray ecBytes;
};


/**
* Get number of data bytes and number of error correction bytes for block id "blockID". Store
* the result in "numDataBytesInBlock", and "numECBytesInBlock". See table 12 in 8.5.1 of
* JISX0510:2004 (p.30)
*/
ZXING_EXPORT_TEST_ONLY
void GetNumDataBytesAndNumECBytesForBlockID(int numTotalBytes, int numDataBytes, int numRSBlocks, int blockID,
											int& numDataBytesInBlock, int& numECBytesInBlock)
{
	if (blockID >= numRSBlocks) {
		throw std::invalid_argument("Block ID too large");
	}
	// numRsBlocksInGroup2 = 196 % 5 = 1
	int numRsBlocksInGroup2 = numTotalBytes % numRSBlocks;
	// numRsBlocksInGroup1 = 5 - 1 = 4
	int numRsBlocksInGroup1 = numRSBlocks - numRsBlocksInGroup2;
	// numTotalBytesInGroup1 = 196 / 5 = 39
	int numTotalBytesInGroup1 = numTotalBytes / numRSBlocks;
	// numTotalBytesInGroup2 = 39 + 1 = 40
	int numTotalBytesInGroup2 = numTotalBytesInGroup1 + 1;
	// numDataBytesInGroup1 = 66 / 5 = 13
	int numDataBytesInGroup1 = numDataBytes / numRSBlocks;
	// numDataBytesInGroup2 = 13 + 1 = 14
	int numDataBytesInGroup2 = numDataBytesInGroup1 + 1;
	// numEcBytesInGroup1 = 39 - 13 = 26
	int numEcBytesInGroup1 = numTotalBytesInGroup1 - numDataBytesInGroup1;
	// numEcBytesInGroup2 = 40 - 14 = 26
	int numEcBytesInGroup2 = numTotalBytesInGroup2 - numDataBytesInGroup2;
	// Sanity checks.
	// 26 = 26
	if (numEcBytesInGroup1 != numEcBytesInGroup2) {
		throw std::invalid_argument("EC bytes mismatch");
	}
	// 5 = 4 + 1.
	if (numRSBlocks != numRsBlocksInGroup1 + numRsBlocksInGroup2) {
		throw std::invalid_argument("RS blocks mismatch");
	}
	// 196 = (13 + 26) * 4 + (14 + 26) * 1
	if (numTotalBytes
		!= ((numDataBytesInGroup1 + numEcBytesInGroup1) * numRsBlocksInGroup1)
			   + ((numDataBytesInGroup2 + numEcBytesInGroup2) * numRsBlocksInGroup2)) {
		throw std::invalid_argument("Total bytes mismatch");
	}

	if (blockID < numRsBlocksInGroup1) {
		numDataBytesInBlock = numDataBytesInGroup1;
		numECBytesInBlock = numEcBytesInGroup1;
	}
	else {
		numDataBytesInBlock = numDataBytesInGroup2;
		numECBytesInBlock = numEcBytesInGroup2;
	}
}

ZXING_EXPORT_TEST_ONLY
void GenerateECBytes(const ByteArray& dataBytes, int numEcBytes, ByteArray& ecBytes)
{
	std::vector<int> message(dataBytes.size() + numEcBytes, 0);
	std::copy(dataBytes.begin(), dataBytes.end(), message.begin());
	ReedSolomonEncode(GenericGF::QRCodeField256(), message, numEcBytes);

	ecBytes.resize(numEcBytes);
	std::transform(message.end() - numEcBytes, message.end(), ecBytes.begin(), [](auto c) { return narrow_cast<uint8_t>(c); });
}


/**
* Interleave "bits" with corresponding error correction bytes. On success, store the result in
* "result". The interleave rule is complicated. See 8.6 of JISX0510:2004 (p.37) for details.
*/
ZXING_EXPORT_TEST_ONLY
BitArray InterleaveWithECBytes(const BitArray& bits, int numTotalBytes, int numDataBytes, int numRSBlocks)
{
	// "bits" must have "getNumDataBytes" bytes of data.
	if (bits.sizeInBytes() != numDataBytes) {
		throw std::invalid_argument("Number of bits and data bytes does not match");
	}

	// Step 1.  Divide data bytes into blocks and generate error correction bytes for them. We'll
	// store the divided data bytes blocks and error correction bytes blocks into "blocks".
	int dataBytesOffset = 0;
	int maxNumDataBytes = 0;
	int maxNumEcBytes = 0;

	// Since, we know the number of reedsolmon blocks, we can initialize the vector with the number.
	std::vector<BlockPair> blocks(numRSBlocks);

	for (int i = 0; i < numRSBlocks; ++i) {
		int numDataBytesInBlock = 0;
		int numEcBytesInBlock = 0;
		GetNumDataBytesAndNumECBytesForBlockID(numTotalBytes, numDataBytes, numRSBlocks, i, numDataBytesInBlock, numEcBytesInBlock);

		blocks[i].dataBytes = bits.toBytes(8 * dataBytesOffset, numDataBytesInBlock);
		GenerateECBytes(blocks[i].dataBytes, numEcBytesInBlock, blocks[i].ecBytes);

		maxNumDataBytes = std::max(maxNumDataBytes, numDataBytesInBlock);
		maxNumEcBytes = std::max(maxNumEcBytes, Size(blocks[i].ecBytes));
		dataBytesOffset += numDataBytesInBlock;
	}
	if (numDataBytes != dataBytesOffset) {
		throw std::invalid_argument("Data bytes does not match offset");
	}

	BitArray output;
	// First, place data blocks.
	for (int i = 0; i < maxNumDataBytes; ++i) {
		for (auto& block : blocks) {
			if (i < Size(block.dataBytes)) {
				output.appendBits(block.dataBytes[i], 8);
			}
		}
	}
	// Then, place error correction blocks.
	for (int i = 0; i < maxNumEcBytes; ++i) {
		for (auto& block : blocks) {
			if (i < Size(block.ecBytes)) {
				output.appendBits(block.ecBytes[i], 8);
			}
		}
	}
	if (numTotalBytes != output.sizeInBytes()) {  // Should be same.
		throw std::invalid_argument("Interleaving error: " + std::to_string(numTotalBytes) + " and " + std::to_string(output.sizeInBytes())
									+ " differ.");
	}
	return output;
}


static int ChooseMaskPattern(const BitArray& bits, ErrorCorrectionLevel ecLevel, const Version& version, TritMatrix& matrix)
{
	int minPenalty = std::numeric_limits<int>::max();  // Lower penalty is better.
	int bestMaskPattern = -1;
	// We try all mask patterns to choose the best one.
	for (int maskPattern = 0; maskPattern < NUM_MASK_PATTERNS; maskPattern++) {
		BuildMatrix(bits, ecLevel, version, maskPattern, matrix);
		int penalty = MaskUtil::CalculateMaskPenalty(matrix);
		if (penalty < minPenalty) {
			minPenalty = penalty;
			bestMaskPattern = maskPattern;
		}
	}
	return bestMaskPattern;
}

static int CalculateBitsNeeded(CodecMode mode, const BitArray& headerBits, const BitArray& dataBits, const Version& version)
{
	return headerBits.size() + CharacterCountBits(mode, version.versionNumber()) + dataBits.size();
}

EncodeResult Encode(const BitArray& dataBits, ErrorCorrectionLevel ecLevel, int versionNumber, int maskPattern)
{
	// Pick an encoding mode appropriate for the content. Note that this will not attempt to use
	// multiple modes / segments even if that were more efficient. Twould be nice.
	constexpr auto mode = CodecMode::BYTE;

	// This will store the header information, like mode and
	// length, as well as "header" segments like an ECI segment.
	BitArray headerBits;

	// (With ECI in place,) Write the mode marker
	AppendModeInfo(mode, headerBits);

	const Version* version;
	if (versionNumber > 0) {
		version = Version::Model2(versionNumber);
		if (version != nullptr) {
			int bitsNeeded = CalculateBitsNeeded(mode, headerBits, dataBits, *version);
			if (!WillFit(bitsNeeded, *version, ecLevel)) {
				throw std::invalid_argument("Data too big for requested version");
			}
		}
		else {
			throw std::invalid_argument("Invalid version number");
		}
	}
	else {
		throw std::invalid_argument("Invalid version number");
	}

	BitArray headerAndDataBits;
	headerAndDataBits.appendBitArray(headerBits);
	// Find "length" of main segment and write it
	int numLetters = dataBits.sizeInBytes();
	AppendLengthInfo(numLetters, *version, mode, headerAndDataBits);
	// Put data together into the overall payload
	headerAndDataBits.appendBitArray(dataBits);

	auto& ecBlocks = version->ecBlocksForLevel(ecLevel);
	int numDataBytes = version->totalCodewords() - ecBlocks.totalCodewords();

	// Terminate the bits properly.
	TerminateBits(numDataBytes, headerAndDataBits);

	// Interleave data bits with error correction code.
	BitArray finalBits =
		InterleaveWithECBytes(headerAndDataBits, version->totalCodewords(), numDataBytes, ecBlocks.numBlocks());

	EncodeResult output;
	output.ecLevel = ecLevel;
	output.mode = mode;
	output.version = version;

	//  Choose the mask pattern and set to "qrCode".
	int dimension = version->dimension();
	TritMatrix matrix(dimension, dimension);
	output.maskPattern = maskPattern != -1 ? maskPattern : ChooseMaskPattern(finalBits, ecLevel, *version, matrix);

	// Build the matrix and set it to "qrCode".
	BuildMatrix(finalBits, ecLevel, *version, output.maskPattern, matrix);

	output.matrix = ToBitMatrix(matrix);

	return output;
}

} // namespace ZXing::QRCode
