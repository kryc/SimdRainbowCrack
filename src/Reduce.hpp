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

#include "Common.hpp"
#include "WordGenerator.hpp"

#ifdef BIGINT
typedef mpz_class index_t;
#else
typedef uint64_t index_t;
#endif

static inline uint32_t
rotl(
    const uint32_t Value,
    const uint8_t Distance
)
{
    return (Value << Distance) | (Value >> (32 - Distance));
}

static inline uint32_t
rotr(
    const uint32_t Value,
    const uint8_t Distance
)
{
    return (Value >> Distance) | (Value << (32 - Distance));
}

static inline void
calculate_bytes_required(
    const index_t Value,
    size_t* BytesRequired,
    uint8_t* MSBMask
)
{
    // Figure out the smallest number of bits of
    // input hash data required to represent _Value_
    size_t bitsRequired = 0;
    index_t mask = 0;
    size_t bitsoverflow;

    while (mask < Value)
    {
        mask <<= 1;
        mask |= 0x1;
        bitsRequired++;
    }
    // Calculate the number of whole bytes required
    *BytesRequired = bitsRequired / 8;
    // The number of bits to mask off the most
    // significant byte
    bitsoverflow = bitsRequired % 8;
    // If we overflow an 8-bit boundary then we
    // need to use an extra byte
    if (bitsoverflow != 0)
    {
        (*BytesRequired)++;
        // We need to mask off the top additional
        // bits though
        size_t bitstomask = 8 - bitsoverflow;
        *MSBMask = 0xff >> bitstomask;
    }
    else
    {
        *MSBMask = 0xff;
    }
}

// Doing a simple modulo on each byte will introduce
// a modulo bias. We need to calculate the maximum
// value that a multiple of the number of characters
// will fit into a single uint8_t
static inline uint8_t
calculate_modulo_bias_mask(
    const size_t CharsetSize
)
{
    size_t maxval = CharsetSize - 1;
    return floor(pow(2, 8) / (maxval + 1)) * (maxval + 1);
}

static inline index_t
load_bytes_to_index(
    const uint8_t* const Buffer,
    const size_t Offset,
    const size_t Length
)
{
#ifdef BIGINT
    index_t reduction;
    mpz_import(reduction.get_mpz_t(), m_BytesRequired, 1, sizeof(uint8_t), 0, 0, &hashBuffer[offset]);
#else
    index_t reduction = 0;
    for (size_t i = 0; i < Length; i++)
    {
        reduction <<= 8;
        reduction |= Buffer[Offset + i];
    }
#endif
    return reduction;
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
        m_Charset(Charset),
#ifdef BIGINT
        m_MinIndex(WordGenerator::WordLengthIndex(Min, Charset)),
        m_MaxIndex(WordGenerator::WordLengthIndex(Max + 1, Charset))
#else
        m_MinIndex(WordGenerator::WordLengthIndex64(Min, Charset)),
        m_MaxIndex(WordGenerator::WordLengthIndex64(Max + 1, Charset))
#endif
        { };
    virtual size_t Reduce(
        char* Destination,
        const size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    ) const = 0;
    virtual ~Reducer() {};
    const size_t GetMin(void) const { return m_Min; }
    const size_t GetMax(void) const { return m_Max; }
    const size_t GetHashLength(void) const { return m_HashLength; }
    const std::string& GetCharset(void) const  { return m_Charset; }
    const index_t GetMinIndex(void) const { return m_MinIndex; }
    const index_t GetMaxIndex(void) const { return m_MaxIndex; }
    const index_t GetKeyspace(void) const { return m_MaxIndex - m_MinIndex; }
protected:
    // A basic entropy extension function based on SHA256 extension
    // It replaces the data in a destination buffer
    void ExtendEntropy(
        uint32_t* const Buffer,
        const size_t LengthWords
    ) const
    {
        uint32_t temp[LengthWords * 2];
        // Copy existing state
        memcpy(temp, Buffer, LengthWords * sizeof(uint32_t));
        // Extend the buffer
        for (size_t i = LengthWords; i < sizeof(temp) / sizeof(*temp); i++)
        {
#ifndef EXTEND_SIMPLE
            uint32_t s0 = rotr(temp[i - LengthWords], 7) ^ rotr(temp[i - LengthWords], 18) ^ (temp[i - LengthWords] >> 3);
            uint32_t s1 = rotr(temp[i - 2], 17) ^ rotr(temp[i - 2], 19) ^ (temp[i - 2] >> 10);
            temp[i] = s0 + s1;
#else
            temp[i] = rotl(temp[i - LengthWords] ^ temp[i - 2], 1);
#endif
        }
        // Copy the extended buffer back
        memcpy(Buffer, &temp[LengthWords], LengthWords * sizeof(uint32_t));
    }

    inline const size_t
    GetCharsUnbiased(
        char* const Destination,
        const uint8_t* Buffer,
        const size_t Offset,
        const size_t Length,
        const uint8_t ModMax
    ) const
    {
        // Now read bytes from the remaining input buffer
        size_t bytesWritten = 0;
        const size_t charsetSize = m_Charset.size();
        size_t offset = Offset;
        while (bytesWritten < Length)
        {
            if (offset >= m_HashLength)
            {
                ExtendEntropy((uint32_t*)Buffer, m_HashLengthWords);
                offset = 0;
            }

            uint8_t next = Buffer[offset++];
            if (next < ModMax)
            {
                Destination[bytesWritten++] = m_Charset[next % charsetSize];
            }
        }
        return bytesWritten;
    }

    const size_t m_Min;
    const size_t m_Max;
    const size_t m_HashLength;
    const size_t m_HashLengthWords;
    const std::string m_Charset;
    const index_t m_MinIndex;
    const index_t m_MaxIndex;
};

class BasicModuloReducer : public Reducer
{
public:
    BasicModuloReducer(
        const size_t Min,
        const size_t Max,
        const size_t HashLength,
        const std::string& Charset
    ) : Reducer(Min, Max, HashLength, Charset) {}

    size_t Reduce(
        char* Destination,
        const size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    ) const override
    {
        
        // Parse the hash as a single bigint
        index_t reduction = load_bytes_to_index(Hash, 0, m_HashLength);
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
        index_t& Value,
        const size_t Iteration
    ) const
    {
        // XOR the current rainbow collumn number
        Value ^= Iteration;
        // Constrain it within the index range
        Value %= GetKeyspace();
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
        calculate_bytes_required(
            GetKeyspace(),
            &m_BytesRequired,
            &m_MsbMask
        );

        assert(m_BytesRequired * sizeof(uint8_t) <= m_HashLength);
    };

    size_t Reduce(
        char* Destination,
        const size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    ) const override
    {
        uint8_t hashBuffer[m_HashLength];
        memcpy(hashBuffer, Hash, m_HashLength);
        // Repeatedly try to load the hash integer until it is in range
        // we do this to avoid a modulo bias favouring reduction at the bottom
        // end of the password space
        index_t reduction = GetKeyspace() + 1;
        size_t offset = 0;
        while (reduction > GetKeyspace())
        {
            if (offset + m_BytesRequired == m_HashLength)
            {
                ExtendEntropy((uint32_t*)hashBuffer, m_HashLengthWords);
                offset = 0;
            }
            // Mask off the most significant bit
            uint8_t mostSigByte = hashBuffer[offset];
            hashBuffer[offset] = mostSigByte & m_MsbMask;
            // Parse the hash as a single integer
            reduction = load_bytes_to_index(hashBuffer, offset, m_BytesRequired);
            // Replace the original entropy in case we need to extend
            hashBuffer[offset] = mostSigByte;
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
protected:
    size_t m_BytesRequired;
    uint8_t m_MsbMask;
};

class HybridReducer final : public Reducer
{
public:
    HybridReducer(
        const size_t Min,
        const size_t Max,
        const size_t HashLength,
        const std::string& Charset
    ) : Reducer(Min, Max, HashLength, Charset)
    {
        index_t total = 0;
        for (size_t i = Min; i <= Max; i++)
        {
#ifdef BIGINT
            const mpz_class lower = WordGenerator::WordLengthIndex(i, Charset);
            const mpz_class upper = WordGenerator::WordLengthIndex(i + 1, Charset);
#else
            const uint64_t lower = WordGenerator::WordLengthIndex64(i, Charset);
            const uint64_t upper = WordGenerator::WordLengthIndex64(i + 1, Charset);
#endif
            const index_t keyspace = upper - lower;
            total += keyspace;
            m_Limits[i] = total;
        }

        // We need to calculate the number of bytes required
        // to represent the largest range of the keyspace
        calculate_bytes_required(
            total,
            &m_BytesRequired,
            &m_MsbMask
        );

#ifndef BIGINT
        assert(m_BytesRequired <= sizeof(uint64_t));
#endif

        m_ModMax = calculate_modulo_bias_mask(m_Charset.size());
    }

    size_t Reduce(
        char* Destination,
        const size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    ) const override
    {
        uint8_t buffer[m_HashLength];
        uint32_t * const buffer32 = (uint32_t*) buffer;
        const uint32_t * const hash32 = (uint32_t*) Hash;
        size_t length, offset;
        // Copy and mix in the iteration
        for (size_t i = 0; i < m_HashLengthWords; i++)
        {
            buffer32[i] = hash32[i] ^ rotl(0x5a827999 * Iteration, i);
        }

        // Initialize the buffer read offset
        offset = 0;
        
        // If we are using variable lengths we use a bigint
        // within the keyspace range to get the length
        if (m_Min != m_Max)
        {
            // Repeatedly try to load the hash integer until it is in range
            // we do this to avoid a modulo bias favouring reduction at the bottom
            // end of the password space
            index_t reduction = m_Limits[m_Max] + 1;
            while (reduction >= m_Limits[m_Max])
            {
                if (offset + m_BytesRequired == m_HashLength)
                {
                    ExtendEntropy((uint32_t*)buffer, m_HashLengthWords);
                    offset = 0;
                }
                // Mask off the most significant bit
                // const uint8_t mostSigByte = buffer[offset];
                // buffer[offset] = mostSigByte & m_MsbMask;
                // Parse the hash as a single integer
                reduction = load_bytes_to_index(buffer, offset, m_BytesRequired);
                // Replace the original entropy in case we need to extend
                // buffer[offset] = mostSigByte;
                // Move the offset along
                offset++;
            }

            length = 0;
            for (size_t i = m_Min; i <= m_Max && length == 0; i++)
            {
                if (reduction < m_Limits[i])
                {
                    length = i;
                }
            }
        }
        else
        {
            length = m_Max;
        }

        // Now read bytes from the remaining input buffer
        // Update the used bytes to account for entropy used in length calculation
        offset += m_BytesRequired - 1;
        // Or... just reset it to zero to minimize entropy extension.. Is reusing
        // the entropy used in the length calculation a bad thing...?
        // offset = 0;

        return GetCharsUnbiased(
            Destination,
            buffer,
            offset,
            length,
            m_ModMax
        );
    }
private:
    size_t m_BytesRequired;
    uint8_t m_MsbMask;
    std::array<index_t, MAX_LENGTH> m_Limits{};
    size_t m_ModMax;
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
        m_ModMax = calculate_modulo_bias_mask(m_Charset.size());
    }
    
    size_t Reduce(
        char* Destination,
        const size_t DestLength,
        const uint8_t* Hash,
        const size_t Iteration
    ) const override
    {
        uint8_t buffer[m_HashLength];
        // Copy hash to buffer so we can update it
        memcpy(buffer, Hash, m_HashLength);

        return GetCharsUnbiased(
            Destination,
            buffer,
            0,
            m_Max,
            m_ModMax
        );
    };
private:
    size_t m_ModMax;
};

#endif /* Reduce_hpp */
