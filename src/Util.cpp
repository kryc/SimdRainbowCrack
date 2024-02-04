//
//  Util.cpp
//  SimdCrack
//
//  Created by Kryc on 14/09/2020.
//  Copyright © 2020 Kryc. All rights reserved.
//

#include <vector>
#include <string>
#include <cstdint>
#include "Util.hpp"

namespace Util
{

std::vector<uint8_t>
ParseHex(std::string HexString)
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
		snprintf(buffer, 2, "%02X", Bytes[i]);
		ret.push_back(buffer[0]);
		ret.push_back(buffer[1]);
	}

	return ret;
}


}
