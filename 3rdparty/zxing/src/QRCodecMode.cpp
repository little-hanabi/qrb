/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2023 gitlost
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRCodecMode.h"

#include "Error.h"
#include "QRVersion.h"
#include "ZXAlgorithms.h"

#include <array>

namespace ZXing::QRCode {

CodecMode CodecModeForBits(int bits, Type type)
{
	if (type == Type::Model2) {
		if ((bits >= 0x00 && bits <= 0x05) || (bits >= 0x07 && bits <= 0x09) || bits == 0x0d)
			return static_cast<CodecMode>(bits);
	}

	throw FormatError("Invalid codec mode");
}

int CharacterCountBits(CodecMode mode, const int versionNumber)
{
	int i;
	if (versionNumber <= 9)
		i = 0;
	else if (versionNumber <= 26)
		i = 1;
	else
		i = 2;

	if (mode == CodecMode::BYTE) return std::array{8, 16, 16}[i];
	else return 0;
}

int CodecModeBitsLength(const Version& version)
{
	return version.isMicro() ? version.versionNumber() - 1 : 4 - version.isRMQR();
}

int TerminatorBitsLength(const Version& version)
{
	return version.isMicro() ? version.versionNumber() * 2 + 1 : 4 - version.isRMQR();
}

} // namespace ZXing::QRCode
