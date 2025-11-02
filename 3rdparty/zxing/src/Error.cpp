/*
 * Copyright 2022 Axel Waggershauser
 */
// SPDX-License-Identifier: Apache-2.0

#include "Error.h"

namespace ZXing {

std::string ToString(const Error& e)
{
	const char* name[] = {"", "FormatError", "ChecksumError", "Unsupported"};
	std::string ret = name[static_cast<int>(e.type())];
	if (!e.msg().empty())
		ret += " (" + e.msg() + ")";
	return ret;
}

}
