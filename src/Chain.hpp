//
//  Chain.hpp
//  SimdCrack
//
//  Created by Kryc on 04/02/2024.
//  Copyright © 2020 Kryc. All rights reserved.
//

#include <string>
#include <gmpxx.h>

#ifndef Chain_hpp
#define Chain_hpp

class Chain
{
public:
    Chain(mpz_class Index, std::string Start, std::string End, size_t Length):
        m_Index(Index), m_Start(Start), m_End(End), m_Length(Length){};
    Chain(mpz_class Index, std::string Start, size_t Length):
        m_Index(Index), m_Start(Start), m_Length(Length){};
    void SetEnd(std::string End) { m_End = End; };
    const mpz_class Index(void) const { return m_Index; };
    const std::string Start(void) const { return m_Start; };
    const std::string End(void) const { return m_End; };
    const size_t Length(void) const { return m_Length; };
private:
    const mpz_class m_Index;
    const std::string m_Start;
    std::string m_End;
    const size_t m_Length;
};

#endif /* Chain_hpp */
