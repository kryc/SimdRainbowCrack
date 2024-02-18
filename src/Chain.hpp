//
//  Chain.hpp
//  RainbowCrack-
//
//  Created by Kryc on 04/02/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#include <string>
#include <gmpxx.h>
#include <vector>

#ifndef Chain_hpp
#define Chain_hpp

class Chain
{
public:
    Chain(void) = default;
    Chain(mpz_class& Index, std::string& Start, std::string& End, size_t Length):
        m_Index(Index), m_Start(Start), m_End(End), m_Length(Length){};
    Chain(mpz_class& Index, std::string& Start, size_t Length):
        m_Index(Index), m_Start(Start), m_Length(Length){};
    void SetIndex(const mpz_class& Index) { m_Index = Index; };
    void SetStart(const std::string& Start) { m_Start = Start; };
    void SetStart(const char* Start, const size_t Length) { SetStart(std::string(Start, Start + Length)); };
    void SetStart(const uint8_t* Start, const size_t Length) { SetStart((char*)Start, Length); };
    void SetEnd(const std::string& End) { m_End = End; };
    void SetEnd(const char* End, const size_t Length) { SetEnd(std::string(End, End + Length)); };
    void SetEnd(const uint8_t* End, const size_t Length) { SetEnd((char*)End, Length); };
    void SetLength(const size_t Length) { m_Length = Length; };
    const mpz_class& Index(void) const { return m_Index; };
    const std::string& Start(void) const { return m_Start; };
    const std::string& End(void) const { return m_End; };
    const size_t Length(void) const { return m_Length; };
private:
    mpz_class m_Index;
    std::string m_Start;
    std::string m_End;
    size_t m_Length;
};

using ChainBlock = std::vector<Chain>;

#endif /* Chain_hpp */
