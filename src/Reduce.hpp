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
#include <iostream>
#include <gmpxx.h>
#include <string>

#include "WordGenerator.hpp"

class Reducer
{
public:
    Reducer(
        const size_t Min,
        const size_t Max,
        const size_t HashLength,
        const std::string& Charset
    ) : m_Min(Min),
        m_Max(Max),
        m_HashLength(HashLength),
        m_HashLengthWords(HashLength/sizeof(uint32_t)),
        m_Charset(Charset) {};
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
    const size_t m_HashLengthWords;
    const std::string m_Charset;
};

class BigIntReducer : public Reducer
{
public:
    BigIntReducer(
        const size_t Min,
        const size_t Max,
        const size_t HashLength,
        const std::string& Charset
    ) : Reducer(Min, Max, HashLength, Charset)
    {
        m_MaxIndex = WordGenerator::WordLengthIndex(Max + 1, Charset);
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
            m_Charset
        );
    }
private:
    mpz_class m_MaxIndex;
};

class ModuloReducer : public Reducer
{
public:
    using Reducer::Reducer;
    static uint32_t rotl(uint32_t Value, uint8_t Distance)
    {
        return (Value << Distance) | (Value >> (32 - Distance));
    };
    static uint8_t SelectValueInRange(
        const uint8_t* Buffer,
        const size_t Length,
        size_t* Offset,
        const uint8_t Max,
        const size_t Range
    )
    {
        // Loop forwards to find matching value range
        while (*Offset < Length)
        {
            size_t i = (*Offset)++;
            if (Buffer[i] >= Range)
            {
                continue;
            }
            return Buffer[i] % (Max + 1);
        }
        return -1;
    }
    static inline size_t CalculateModMax(
        const size_t Max
    )
    {
        return floor(pow(2, 8) / (Max + 1)) * (Max + 1);
    }
    static inline uint8_t SelectValue(
        const uint8_t* Buffer,
        const size_t Length,
        size_t*      Offset,
        const uint8_t Min,
        const uint8_t Max
    )
    {
        // Calculate maximum value we can modulo
        uint8_t diff = Max - Min;
        size_t modmax = CalculateModMax(diff);
        return Min + SelectValueInRange(Buffer, Length, Offset, diff, modmax);
    }
    void ExtendEntropy(
        void
    )
    {
        constexpr size_t kBufferCount = sizeof(m_Buffer)/sizeof(uint32_t);
        uint32_t temp[kBufferCount * 2];
        // Copy existing state
        memcpy(temp, m_Buffer, sizeof(m_Buffer));
        // Extend the buffer
        for (size_t i = kBufferCount; i < sizeof(temp) / sizeof(*temp); i++)
        {
            temp[i] = rotl(temp[i - kBufferCount] ^ temp[i - 2], 1);
        }
        // Copy the extended buffer back
        memcpy(m_Buffer, &temp[kBufferCount], sizeof(m_Buffer));
    }
    size_t Reduce(
        char* Destination,
        size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    )
    {
        // Copy hash to buffer
        memcpy(m_Buffer, Hash, m_HashLength);
        // Perform initial entropy extension
        for (size_t i = m_HashLengthWords; i < sizeof(m_Buffer)/sizeof(uint32_t); i++)
        {
            m_Buffer[i] = rotl(m_Buffer[i - m_HashLengthWords] ^ m_Buffer[i - 2], 1);
        }
        // Get number of characters
        size_t characterCount;
        size_t bufferOffset = 0;
        for (;;)
        {
            characterCount = SelectValue(
                (uint8_t*)m_Buffer,
                sizeof(m_Buffer),
                &bufferOffset,
                m_Min,
                m_Max
            );
            if (bufferOffset != sizeof(m_Buffer))
            {
                break;
            }
            else
            {
                ExtendEntropy();
                bufferOffset = 0;
            }
        }
        // Loop and get all characters
        size_t count = 0;
        size_t max = m_Charset.size() - 1;
        size_t modmax = CalculateModMax(max);
        // std::cout << max << ' ' << modmax << std::endl;
        while (count < characterCount)
        {
            uint8_t index = SelectValueInRange(
                (uint8_t*)m_Buffer,
                sizeof(m_Buffer),
                &bufferOffset,
                max,
                modmax
            );
            if (bufferOffset != sizeof(m_Buffer))
            {
                Destination[count++] = m_Charset[index];
            }
            else
            {
                // std::cerr << "Extend" << std::endl;
                ExtendEntropy();
                bufferOffset = 0;
            }
        }
        return count;
    };
private:
    uint32_t m_Buffer[6];
};

#endif /* Reduce_hpp */
