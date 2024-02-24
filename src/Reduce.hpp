//
//  Reduce.hpp
//  SimdCrack
//
//  Created by Kryc on 15/02/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#ifndef Reduce_hpp
#define Reduce_hpp

#include <assert.h>
#include <cinttypes>
#include <cstddef>
#include <iostream>
#include <gmpxx.h>
#include <math.h>
#include <string>

#include "WordGenerator.hpp"

static uint32_t rotl(
    const uint32_t Value,
    const uint8_t Distance
)
{
    return (Value << Distance) | (Value >> (32 - Distance));
}

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
        const size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    ) = 0;
    virtual ~Reducer() {};
protected:
    // A basic entropy extension function
    // It replaces the data in a destination buffer
    // Based on SHA1 extension function
    void ExtendEntropy(
        uint32_t* Buffer,
        const size_t LengthWords
    )
    {
        uint32_t temp[LengthWords * 2];
        // Copy existing state
        memcpy(temp, Buffer, LengthWords * sizeof(uint32_t));
        // Extend the buffer
        for (size_t i = LengthWords; i < sizeof(temp) / sizeof(*temp); i++)
        {
            temp[i] = rotl(temp[i - LengthWords] ^ temp[i - 2], 1);
        }
        // Copy the extended buffer back
        memcpy(Buffer, &temp[LengthWords], LengthWords * sizeof(uint32_t));
    }
    const size_t m_Min;
    const size_t m_Max;
    const size_t m_HashLength;
    const size_t m_HashLengthWords;
    const std::string m_Charset;
};

class BasicModuloReducer : public Reducer
{
public:
    BasicModuloReducer(
        const size_t Min,
        const size_t Max,
        const size_t HashLength,
        const std::string& Charset
    ) : Reducer(Min, Max, HashLength, Charset)
    {
        m_MinIndex = WordGenerator::WordLengthIndex(Min, Charset);
        m_MaxIndex = WordGenerator::WordLengthIndex(Max + 1, Charset);
        m_IndexRange = m_MaxIndex - m_MinIndex;
    };

    size_t Reduce(
        char* Destination,
        const size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    ) override
    {
        mpz_class reduction;
        // Parse the hash as a single bigint
        mpz_import(reduction.get_mpz_t(), m_HashLength, 1, sizeof(uint8_t), 1, 0, Hash);
        return PerformReduction(
            Destination,
            DestLength,
            reduction,
            Iteration
        );
    }
    
protected:

    inline size_t PerformReduction(
        char* Destination,
        const size_t DestLength,
        mpz_class& Value,
        const size_t Iteration
    )
    {
        // XOR the current rainbow collumn number
        Value ^= Iteration;
        // Constrain it within the index range
        Value %= m_IndexRange;
        // Add the minimum index to ensure it is >min and <max
        Value += m_MinIndex;
        // Generate and return the word for the index
        return WordGenerator::GenerateWord(
            Destination,
            DestLength,
            Value,
            m_Charset
        );
    }

    mpz_class m_MinIndex;
    mpz_class m_MaxIndex;
    mpz_class m_IndexRange;
};

class ModuloReducer final : public BasicModuloReducer
{
public:
    ModuloReducer(
        const size_t Min,
        const size_t Max,
        const size_t HashLength,
        const std::string& Charset
    ) : BasicModuloReducer(Min, Max, HashLength, Charset)
    {
        // Figure out the smallest number of bits of
        // input hash data required to generate a
        // word in the password space
        m_BitsRequired = 0;
        mpz_class mask = 0;
        while (mask < m_IndexRange)
        {
            mask <<= 1;
            mask |= 0x1;
            m_BitsRequired++;
        }
        // Calculate the number of whole bytes required
        m_WordsRequired = m_BitsRequired / 32;
        // The number of bits to mask off the most significant
        // byte to constrain the value
        size_t bitsoverflow = m_BitsRequired % 32;
        // If we overflow an 8-bit boundary then we
        // need to use an extra byte
        if (bitsoverflow != 0)
        {
            m_WordsRequired++;
        }
        // We need to mask off the top additional
        // bits though
        size_t bitstomask = 32 - bitsoverflow;
        m_MsbMask = 0xffffffff >> bitstomask;
        // std::cerr << m_BitsRequired << ' ' << m_WordsRequired << ' ' << bitsoverflow << ' ' << std::hex << (int)m_MsbMask << std::endl;
        assert(m_WordsRequired * sizeof(uint32_t) <= m_HashLength);
    };

    size_t Reduce(
        char* Destination,
        const size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    ) override
    {
        uint32_t hashBuffer[m_HashLengthWords];
        memcpy(hashBuffer, Hash, m_HashLength);
        // Repeatedly try to load the hash integer until it is in range
        // we do this to avoid a modulo bias favouring reduction at the bottom
        // end of the password space
        mpz_class reduction = m_IndexRange + 1;
        size_t offset = 0;
        while (reduction > m_IndexRange)
        {
            if (offset + m_WordsRequired == m_HashLengthWords)
            {
                ExtendEntropy((uint32_t*)hashBuffer, m_HashLengthWords);
                offset = 0;
            }
            // Mask off the most significant bit
            uint32_t savedWord = hashBuffer[0];
            hashBuffer[offset] = savedWord & m_MsbMask;
            // Parse the hash as a single bigint
            mpz_import(reduction.get_mpz_t(), m_WordsRequired, 1, sizeof(uint32_t), 0, 0, &hashBuffer[offset]);
            // Replace the original entropy in case we need to extend
            hashBuffer[offset] = savedWord;
            // Move the offset along
            offset++;
        }
        // Now return the password as normal
        return PerformReduction(
            Destination,
            DestLength,
            reduction,
            Iteration
        );
    }
private:
    size_t m_BitsRequired;
    size_t m_WordsRequired;
    uint32_t m_MsbMask;
};

class BytewiseReducer final : public Reducer
{
public:
    BytewiseReducer(
        const size_t Min,
        const size_t Max,
        const size_t HashLength,
        const std::string& Charset
    ) : Reducer(Min, Max, HashLength, Charset)
    {
        assert(Min == Max);
        // Doing a simple modulo on each byte will introduce
        // a modulo bias. We need to calculate the maximum
        // value that a multiple of the number of characters
        // will fit into a single uint8_t. We call this modmax.
        size_t maxval = m_Charset.size() - 1;
        m_ModMax = floor(pow(2, 8) / (maxval + 1)) * (maxval + 1);
    }
    
    size_t Reduce(
        char* Destination,
        const size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    ) override
    {
        uint8_t buffer[m_HashLength];
        // Copy hash to buffer
        memcpy(buffer, Hash, m_HashLength);
        // Loop and get all characters
        size_t bufferOffset = 0;
        size_t count = 0;
        size_t charsetSize = m_Charset.size();
        
        while (count < m_Max /* m_Min == m_Max */)
        {
            if (bufferOffset == m_HashLength)
            {
                ExtendEntropy((uint32_t*)buffer, m_HashLengthWords);
                bufferOffset = 0;
            }

            uint8_t next = buffer[bufferOffset++];
            if (next < m_ModMax)
            {
                Destination[count++] = m_Charset[next % charsetSize];
            }
        }
        return count;
    };
private:
    size_t m_ModMax;
};

#endif /* Reduce_hpp */
