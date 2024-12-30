//
//  reductionperf.cpp
//  RainbowCrack-
//
//  Created by Kryc on 21/02/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#include <iostream>
#include <gmpxx.h>
#include <chrono>
#include <map>

#include "Reduce.hpp"
#include "Util.hpp"
#include "WordGenerator.hpp"

#include "simdhash.h"


void TestReducer(
    FILE* Random,
    Reducer* Reducer
)
{
    uint8_t hash[SHA1_SIZE];
    char word[MAX_LENGTH + 1];
    std::map<size_t, size_t> distributions;
    size_t iteration;
    
#ifdef BIGINT
    std::cout << "Exhausting keyspace: " << Reducer->GetKeyspace().get_str() << std::endl;
#else
    std::cout << "Exhausting keyspace: " << Reducer->GetKeyspace() << std::endl;
#endif

    for (mpz_class i = 0; i < Reducer->GetKeyspace(); i++)
    {
        // Get a random iteration
        fread((void*)&iteration, 1, sizeof(iteration), Random);
        iteration %= 2500;
        // Read a random "hash"
        fread(hash, sizeof(hash), sizeof(*hash), Random);
        auto length = Reducer->Reduce(word, MAX_LENGTH, hash, 1);
        if (length > Reducer->GetMax() || length < Reducer->GetMin())
        {
            std::cerr << "Warning: invalid length: " << length << std::endl;
        }
        distributions[length]++;
        // word[length] = '\0';
        // std::cout << word << " (" << length << ")" << std::endl;
    }

    for (size_t i = Reducer->GetMin(); i <= Reducer->GetMax(); i++)
    {
        std::cout << i << ' ' << distributions[i] << std::endl;
    }
}

int main(
    int argc,
    char* argv[]
)
{
    // Open a handle to /dev/urandom
    FILE* fh = fopen("/dev/urandom", "r");
    if (fh == nullptr)
    {
        std::cerr << "Unable to open handle to random" << std::endl;
        return 1;
    }

    // std::cout << "ModuloReducer" << std::endl;
    // ModuloReducer mr(1, atoi(argv[1]), SHA1_SIZE, ASCII);
    // TestReducer(
    //     fh,
    //     &mr
    // );

    std::cout << "HybridReducer" << std::endl;
    HybridReducer hr(1, atoi(argv[1]), SHA1_SIZE, ASCII);
    TestReducer(
        fh,
        &hr
    );

    fclose(fh);
}