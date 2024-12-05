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

}

#endif /* Util_hpp */