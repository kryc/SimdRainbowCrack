//
//  Util.hpp
//  CrackList
//
//  Created by Kryc on 11/08/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#ifndef Util_hpp
#define Util_hpp

#include <vector>
#include <string>
#include <cstdint>
#include <gmpxx.h>

namespace Util
{

std::vector<uint8_t>
ParseHex(
    const std::string& HexString
);

std::string
ToHex(
    const uint8_t* Bytes,
    const size_t Length
);

bool
IsHex(
    const std::string& String
);

std::string
ToLower(
    const std::string& String
);

const std::string
Hexlify(
    const std::string& Value
);

double
NumFactor(
    const double Value,
    std::string& HumanFactor
);

mpz_class
NumFactor(
    const mpz_class Value,
    std::string& HumanFactor
);

const double
SizeFactor(
    const double SizeBytes,
    std::string& HumanFactor
);

}

#endif /* Util_hpp */