//
//  Reduce.hpp
//  SimdCrack
//
//  Created by Kryc on 15/02/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#ifndef Reduce_hpp
#define Reduce_hpp

#include <cinttypes>
#include <cstddef>
#include <gmpxx.h>

#include "WordGenerator.hpp"

class Reducer
{
public:
    Reducer(
        size_t Min,
        size_t Max,
        size_t HashLength
    ) : m_Min(Min), m_Max(Max), m_HashLength(HashLength) {};
    virtual size_t Reduce(
        char* Destination,
        size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    ) = 0;
protected:
    const size_t m_Min;
    const size_t m_Max;
    const size_t m_HashLength;
};

class BigIntReducer : public Reducer
{
public:
    BigIntReducer(
        size_t Min,
        size_t Max,
        size_t HashLength
    ) : Reducer(Min, Max, HashLength)
    {
        m_MaxIndex = WordGenerator::WordLengthIndex(Max + 1, ASCII);
    };

    size_t Reduce(
        char* Destination,
        size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    )
    {
        mpz_class reduction;
        mpz_class iteration(Iteration);

        mpz_import(reduction.get_mpz_t(), m_HashLength, 1, sizeof(uint8_t), 1, 0, Hash);
        reduction ^= iteration;
        reduction %= m_MaxIndex;

        return WordGenerator::GenerateWord(
            Destination,
            DestLength,
            reduction,
            ASCII
        );
    }
private:
    mpz_class m_MaxIndex;
};

#endif /* Reduce_hpp */
