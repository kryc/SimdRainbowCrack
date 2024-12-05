//
//  Util.cpp
//  CrackList
//
//  Created by Kryc on 11/08/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#include <vector>
#include <string>
#include <cstdint>
#include "Util.hpp"

namespace Util
{

std::vector<uint8_t>
ParseHex(
	const std::string& HexString
)
{
	std::vector<uint8_t> vec;
	bool doingUpper = true;
	uint8_t next = 0;
	
	for (size_t i = 0; i < HexString.size(); i ++)
	{
		if (HexString[i] >= 0x30 && HexString[i] <= 0x39)
		{
			next |= HexString[i] - 0x30;
		}
		else if (HexString[i] >= 0x41 && HexString[i] <= 0x46)
		{
			next |= HexString[i] - 0x41 + 10;
		}
		else if (HexString[i] >= 0x61 && HexString[i] <= 0x66)
		{
			next |= HexString[i] - 0x61 + 10;
		}
		
		if ((HexString.size() % 2 == 1 && i == 0) ||
			doingUpper == false)
		{
			vec.push_back(next);
			next = 0;
			doingUpper = true;
		}
		else if (doingUpper)
		{
			next <<= 4;
			doingUpper = false;
		}
	}
	
	return vec;
}

std::string
ToHex(
	const uint8_t* Bytes,
	const size_t Length
)
{
	char buffer[3];
	std::string ret;

	buffer[2] = '\0';

	for (size_t i = 0; i < Length; i++)
	{
		snprintf(buffer, 3, "%02x", Bytes[i]);
		ret.push_back(buffer[0]);
		ret.push_back(buffer[1]);
	}

	return ret;
}

bool
IsHex(
	const std::string& String
)
{
	// Detect an odd length string
	if (String.size() & 1)
	{
		return false;
	}

	for (char c : String)
	{
		if (!isxdigit(c))
		{
			return false;
		}
	}
	
	return true;
}

std::string
ToLower(
    const std::string& String
)
{
	std::string result;

	for (char c : String)
	{
		if (c >= 'A' && c <= 'Z')
		{
			result.push_back(c + ('a' - 'A'));
		}
		else
		{
			result.push_back(c);
		}
	}

	return result;
}

}