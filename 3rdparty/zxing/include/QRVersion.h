/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2023 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Point.h"
#include "QRECB.h"
#include "QRErrorCorrectionLevel.h"
#include "ZXAlgorithms.h"

#include <array>
#include <initializer_list>
#include <vector>

namespace ZXing {

class BitMatrix;

namespace QRCode {

/**
* See ISO 18004:2006 Annex D
*/
class Version
{
public:
	Type type() const { return _type; }
	bool isMicro() const { return type() == Type::Micro; }
	bool isRMQR() const { return type() == Type::rMQR; }
	bool isModel1() const { return type() == Type::Model1; }
	bool isModel2() const { return type() == Type::Model2; }

	int versionNumber() const { return _versionNumber; }

	const std::vector<int>& alignmentPatternCenters() const { return _alignmentPatternCenters; }

	int totalCodewords() const { return _totalCodewords; }

	int dimension() const { return SymbolSize(versionNumber(), isMicro() ? Type::Micro : Type::Model2).x; }

	const ECBlocks& ecBlocksForLevel(ErrorCorrectionLevel ecLevel) const { return _ecBlocks[(int)ecLevel]; }

	BitMatrix buildFunctionPattern() const;

	static constexpr PointI SymbolSize(int version, Type type)
	{
		auto square = [](int s) { return PointI(s, s); };
		auto valid = [](int v, int max) { return v >= 1 && v <= max; };

		if (type == Type::Model2) return valid(version, 40) ? square(17 + 4 * version) : PointI{};
		else return {}; // silence warning
	}

	static constexpr bool IsValidSize(PointI size, Type type)
	{
		if (type == Type::Model2) return size.x == size.y && size.x >= 21 && size.x <= 177 && (size.x % 4 == 1);
		else return false;
	}
	static bool HasValidSize(const BitMatrix& bitMatrix, Type type);

	static constexpr int Number(PointI size)
	{
		if (IsValidSize(size, Type::Model2)) return (size.x - 17) / 4;
		else return 0;
	}

	static int Number(const BitMatrix& bitMatrix);

	static const Version* DecodeVersionInformation(int versionBitsA, int versionBitsB = 0);

	static const Version* Model2(int number);


private:
	int _versionNumber;
	std::vector<int> _alignmentPatternCenters;
	std::array<ECBlocks, 4> _ecBlocks;
	int _totalCodewords;
	Type _type;

	Version(int versionNumber, std::initializer_list<int> alignmentPatternCenters, const std::array<ECBlocks, 4> &ecBlocks);
	Version(int versionNumber, const std::array<ECBlocks, 4>& ecBlocks);
};

} // QRCode
} // ZXing
