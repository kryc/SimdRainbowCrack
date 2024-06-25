//
//  Common.hpp
//  RainbowCrack-
//
//  Created by Kryc on 23/06/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#ifndef Common_hpp
#define Common_hpp

// Defined on a uint32/uint64 boundary minus
// one so that we can store the length in a
// single byte
#define MAX_LENGTH (31)

// A small string class with no initializers
// and all public members to allow
// for faster initialization
class SmallString
{
public:
    void Set(const char* Value, const uint8_t Length) { memcpy(this->Value, Value, Length); this->Length = Length; }
    void Set(const uint8_t* Value, const uint8_t Length) { Set((const char*)Value, Length); };
    void SetLength(const uint8_t Length) { this->Length = Length; };
    std::string GetString(void) const { return std::string(&Value[0], &Value[this->Length]); };
    char Value[MAX_LENGTH];
    uint8_t Length;
} ;

#endif /* Common_hpp */
