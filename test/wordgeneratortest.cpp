//
//  wordgeneratortest.cpp
//  RainbowCrack-
//
//  Created by Kryc on 22/03/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#include <iostream>
#include <string>

#include "gmpxx.h"

#include "WordGenerator.hpp"

int main(
    int argc,
    char* argv[]
)
{
    std::string word, word2;
    mpz_class bigint;
    char buffer[128];
    size_t length;

    for (size_t i = 0; i < 10000; i++)
    {
        bigint = i;

        word = WordGenerator::GenerateWord(i, ASCII);
        word2 = WordGenerator::GenerateWord(bigint, ASCII);

        if (word != word2)
        {
            std::cerr << "Bigint version for index " << i << " does not match" << std::endl;
            return -1;
        }

        length = WordGenerator::GenerateWord(buffer, sizeof(buffer), i, ASCII);

        if (word.size() != length)
        {
            std::cerr << "Length for index " << i << " incorrect" << std::endl;
            return -1;
        }

        if (memcmp(&word[0], buffer, length) != 0)
        {
            std::cerr << "Non-matching words for index " << i << " incorrect" << std::endl;
            return -1;
        }

        length = WordGenerator::GenerateWord(buffer, sizeof(buffer), bigint, ASCII);

        if (word.size() != length || memcmp(&word[0], buffer, length) != 0)
        {
            std::cerr << "Non-matching words for index " << i << " incorrect" << std::endl;
            return -1;
        }
    }

    bigint = 1;
    auto test = WordGenerator::GenerateWord(bigint, ASCII);

    if (test.size() != 1 and test[0] != ASCII[0])
    {
        std::cerr << "Invalid word at index 1" << std::endl;
    }

    bigint++;

    test = WordGenerator::GenerateWord(bigint, ASCII);

    if (test.size() != 1 and test[0] != ASCII[1])
    {
        std::cerr << "Invalid word at index 2" << std::endl;
    }
}