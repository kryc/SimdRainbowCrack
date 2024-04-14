//
//  WordGenerator.hpp
//  SimdCrack
//
//  Created by Kryc on 20/01/2024.
//  Copyright © 2020 Kryc. All rights reserved.
//

#ifndef WordGenerator_hpp
#define WordGenerator_hpp

#include <cstdint>
#include <string>
#include <vector>

#include <gmpxx.h>

// All values are sorted
// Take care when editing!
static const std::string LOWER = "abcdefghijklmnopqrstuvwxyz";
static const std::string UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const std::string ALPHA = UPPER + LOWER;
static const std::string NUMERIC = "0123456789";
static const std::string ASCII_SPECIAL_LO = " !\"#$%&'()*+,-./";
static const std::string ASCII_SPECIAL_ML = ":;<=>?@";
static const std::string ASCII_SPECIAL_MH = "[\\]^_`";
static const std::string ASCII_SPECIAL_HI = "{|}~";
static const std::string ASCII_SPECIAL = ASCII_SPECIAL_LO + ASCII_SPECIAL_ML + ASCII_SPECIAL_MH + ASCII_SPECIAL_HI;
static const std::string ALPHANUMERIC = NUMERIC + UPPER + LOWER;
static const std::string ASCII = ASCII_SPECIAL_LO + NUMERIC + ASCII_SPECIAL_ML + UPPER + ASCII_SPECIAL_MH + LOWER + ASCII_SPECIAL_HI;
// Based on an analysis of cracked passwords
static const std::string COMMON = "a1e20ion9r3sl85746tumdychbkgfpvjwzxqAE._SRMNILTODCBKPHG-UF!YJVWZ@QX*$#?& :+/";
static const std::string COMMON_SHORT = "a1e20ion9r3sl85746tumdychbkgfpvjwzxqAE._SRMNILTODCBKPHG-UF!YJVWZ@QX";

class WordGenerator
{
public:
    WordGenerator(void) : m_Charset(ALPHANUMERIC) {};
    WordGenerator(const std::string& Charset) : m_Charset(Charset) {};
    WordGenerator(const std::string& Charset, std::string Prefix, std::string Postfix)
        : m_Charset(Charset),m_Prefix(Prefix),m_Postfix(Postfix) {};
    static std::string  GenerateWord(const size_t Value, const std::string& Charset);
    static std::string  GenerateWordReversed(const size_t Value, const std::string& Charset);
    static std::string  GenerateWord(const mpz_class& Value, const std::string& Charset);
    static std::string  GenerateWordReversed(const mpz_class& Value, const std::string& Charset);
    static const size_t GenerateWord(char * Destination, const size_t DestSize, const mpz_class& Value, const std::string& Charset);
    static const size_t GenerateWordReversed(char * Destination, const size_t DestSize, const mpz_class& Value, const std::string& Charset);
    const std::string   Generate(const size_t Value);
    const std::string   GenerateReversed(const size_t Value);
    const std::string   Generate(const mpz_class& Value);
    const std::string   GenerateReversed(const mpz_class& Value);
    const size_t        Generate(char * Destination, const size_t DestSize, const mpz_class& Value);
    const size_t        GenerateReversed(char * Destination, const size_t DestSize, const mpz_class& Value);
    static const std::vector<uint8_t> GenerateParsingLookupTable(const std::string& Charset);
    const std::vector<uint8_t> GenerateParsingLookupTable(void) const { return GenerateParsingLookupTable(m_Charset); };
    static const mpz_class Parse(const std::string& Word, const std::string& Charset);
    static const mpz_class Parse(const std::string& Word, const std::vector<uint8_t>& LookupTable);
    const mpz_class Parse(const std::string& Word) const { return Parse(Word, m_Charset); };
    void SetPrefix(std::string& Prefix) { m_Prefix = Prefix; };
    void SetPostfix(std::string& Postfix) { m_Postfix = Postfix; };
    const std::string GetCharset(void) { return m_Charset; };
    static const mpz_class WordLengthIndex(const size_t WordLength, const std::string& Charset);
private:
    const std::string m_Charset;
    const std::vector<uint8_t> m_LookupTable;
    std::string m_Prefix;
    std::string m_Postfix;
};

const std::string&
ParseCharset(
    const std::string& Name
);

#endif /* WordGenerator_hpp */
