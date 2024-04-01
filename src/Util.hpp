//
//  Util.hpp
//  SimdCrack
//
//  Created by Kryc on 14/09/2020.
//  Copyright © 2020 Kryc. All rights reserved.
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
    std::string& HexString
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


}

#endif /* Util_hpp */
