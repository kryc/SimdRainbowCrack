#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <gmpxx.h>

#include "WordGenerator.hpp"

#define D(val) (std::cout << "DBG: " << val << std::endl)

//
// Static
//
std::string
WordGenerator::GenerateWord(
    const size_t Value,
    const std::string& Charset,
    const bool Reverse
)
{
    std::string out;
    int64_t i;
    
    i = (int64_t)Value;
    
    while (i > 0)
    {
        i--;
        out += Charset[i % Charset.length()];
        i /= Charset.length();
    }

    if (Reverse)
    {
        std::reverse(std::begin(out), std::end(out));
    }
     
    return out;
}

const std::string
WordGenerator::Generate(
    const size_t Value,
    const bool Reverse
)
{
    return m_Prefix + GenerateWord(Value, m_Charset, Reverse) + m_Postfix;
}

//
// Static
//
std::string 
WordGenerator::GenerateWord(
    const mpz_class& Value,
    const std::string& Charset,
    const bool Reverse
)
{
    std::string out;

    mpz_class i(Value);
    mpz_class r;
    
    while (i > 0)
    {
        i--;
        r = i % Charset.length();
        out += Charset[r.get_ui()];
        i /= Charset.length();
    }

    if (Reverse)
    {
        std::reverse(std::begin(out), std::end(out));
    }
     
    return out;
}

//
// Static
//
size_t
WordGenerator::GenerateWord(
    char* Destination,
    const size_t DestSize,
    const mpz_class& Value,
    const std::string& Charset,
    const bool Reverse
)
{
    mpz_class i(Value);
    mpz_class r;
    size_t length;

    length = 0;

    while (i > 0)
    {
        r = (i - 1) % Charset.length();
        if (length == DestSize)
            return (size_t)-1;
        Destination[length++] = Charset[r.get_ui()];
        i = (i - 1) / Charset.length();
    }

    if (Reverse)
    {
        std::reverse(Destination, Destination + length);
    }

    return length;
}

const std::string
WordGenerator::Generate(
    const mpz_class& Value,
    const bool Reverse
)
{
    return m_Prefix + GenerateWord(Value, m_Charset, Reverse) + m_Postfix;
}

//
// static
//
const mpz_class
WordGenerator::Parse(
    const std::string& Word,
    const std::string& Charset
)
{
    mpz_class num;
    for (const char& c : Word)
    {
        num = num * Charset.size() + Charset.find_first_of(c);
    }
    return ++num;
}

//
// static
//
const mpz_class
WordGenerator::Parse(
    const std::string& Word,
    const std::vector<uint8_t>& LookupTable
)
{
    mpz_class num;
    for (const char& c : Word)
    {
        num = num * LookupTable[256] + LookupTable[c];
    }
    return ++num;
}

//
// static
//
const std::vector<uint8_t>
WordGenerator::GenerateParsingLookupTable(
    const std::string& Charset
)
{
    std::vector<uint8_t> table;

    table.resize(256);
    memset(&table[0], 0, table.size());
    table[256] = Charset.size();

    for (const char& c : Charset)
    {
        table[c] = Charset.find_first_of(c);
    }

    return table;
}

//
// Static
const mpz_class
WordGenerator::WordLengthIndex(
    const size_t WordLength,
    const std::string& Charset
)
{
    mpz_class counter = 0;
    mpz_class product;
    for (size_t i = 0; i < WordLength; i++)
    {
        mpz_ui_pow_ui(product.get_mpz_t(), ASCII.size(), i);
        counter += product;
    }
    return counter;
}