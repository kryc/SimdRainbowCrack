#include <array>
#include <iostream>
#include <gmpxx.h>
#include <optional>
#include <string>
#include <tuple>
#include <vector>
#include <openssl/sha.h>

#include "simdhash.h"
#include "Chain.hpp"
#include "Util.hpp"
#include "WordGenerator.hpp"
#include "SimdHashBuffer.hpp"
#include "Reduce.hpp"

using ChainBlock = std::vector<Chain>;


ChainBlock
ComputeChain(
    mpz_class Start,
    const size_t Length,
    const size_t Min,
    const size_t Max,
    const size_t Capture,
    std::string* Midpoint
)
{
    SimdHashBuffer words(Max + 1);
    SimdHashBuffer hashes(SHA1_SIZE);
    
    ChainBlock ret;
    
    mpz_class counter;

    BytewiseReducer reducer(Min, Max, SHA1_SIZE, ASCII);

    // Calculate lower bound
    counter = WordGenerator::WordLengthIndex(Min, ASCII);
    // Add the index for this chain
    counter += Start;

    for (size_t i = 0; i < SimdLanes(); i++)
    {
        auto length = WordGenerator::GenerateWord((char*)words[i], Max, counter, ASCII);
        words.SetLength(i, length);
        words[i][length] = '\0';
        std::string start((char*)words[i]);
        ret.push_back(Chain(counter, start, Length));
        counter++;
    }

    for (size_t i = 0; i < Length; i++)
    {
        SimdHash(
            HashAlgorithmSHA1,
            words.GetLengths(),
            words.ConstBuffers(),
            hashes.Buffer()
        );

        for (size_t h = 0; h < SimdLanes(); h++)
        {
            const uint8_t* hash = hashes[h];
            auto length = reducer.Reduce((char*)words[h], Max, hash, i);
            words.SetLength(h, length);
            words[h][length] = '\0';
        }

        if (i == Capture)
        {
            Midpoint->assign(std::string((char*)words[0]));
        }
    }

    for (size_t i = 0; i < SimdLanes(); i++)
    {
        ret[i].SetEnd((char*)words[i]);
    }

    return ret;
}

std::optional<std::string>
ValidateChain(
    const Chain Chain,
    const size_t Min,
    const size_t Max,
    const uint8_t* Hash
)
{
    size_t length;
    uint8_t hash[SHA1_SIZE];
    std::vector<char> reduced(Max + 1);
    BytewiseReducer reducer(Min, Max, SHA1_SIZE, ASCII);

    length = Chain.Start().size();
    memcpy(&reduced[0], Chain.Start().c_str(), length);
    
    for (size_t i = 0; i < Chain.Length(); i++)
    {
        SHA1((uint8_t*)&reduced[0], length, hash);
        if (memcmp(Hash, hash, SHA1_SIZE) == 0)
        {
            return std::string(&reduced[0], &reduced[length]);
        }
        length = reducer.Reduce(&reduced[0], Max, hash, i);
    }
    return {};
}

void
CheckChain(
    const Chain Chain,
    const size_t Min,
    const size_t Max,
    const uint8_t* Hash
)
{
    mpz_class start;
    mpz_class counter;

    // Get the start index
    start = Chain.Index();

    // Calculate lower bound
    counter = WordGenerator::WordLengthIndex(Min, ASCII);
    // Add the index for this chain
    counter += start;

    BasicModuloReducer reducer(Min, Max, SHA1_SIZE, ASCII);

    uint8_t hash[SHA1_SIZE];
    std::vector<char> reduced(Max + 1);

    size_t length;

    for (ssize_t i = Chain.Length() - 1; i >= 0; i--)
    {
        memcpy(hash, Hash, SHA1_SIZE);

        for (size_t j = i; j < Chain.Length() - 1; j++)
        {
            length = reducer.Reduce(&reduced[0], Max, hash, j);
            SHA1((uint8_t*)&reduced[0], length, hash);
        }

        // Final reduction
        length = reducer.Reduce(&reduced[0], Max, hash, Chain.Length() - 1);
    
        // Check end, if it matches, we can perform one full chain to see if we find it
        if (memcmp(Chain.End().c_str(), &reduced[0], length) == 0)
        {
            auto match = ValidateChain(Chain, Min, Max, Hash);
            if (match.has_value())
            {
                auto hashstr = Util::ToHex(Hash, SHA1_SIZE);
                std::cout << hashstr << ' ' << match.value() << std::endl;
            }
            
        }
    }
}

/*void
CheckChainSimd(
    const Chain Chain,
    const size_t Min,
    const size_t Max,
    const uint8_t* Hash
)
{
    mpz_class start;
    mpz_class counter;
    mpz_class maxindex;

    // Get the start index
    start = Chain.Index();

    // Calculate lower bound
    counter = WordGenerator::WordLengthIndex(Min, ASCII);
    // Calculate the upper bount
    maxindex = WordGenerator::WordLengthIndex(Max + 1, ASCII);
    // Add the index for this chain
    counter += start;

    SimdHashBuffer words(Max + 1);
    SimdHashBuffer hashes(SHA1_SIZE);
    std::vector<char> reducedTop;
    size_t topLength;
    reducedTop.resize(Max);

    size_t length;
    size_t lastIndex = words.Count() - 1;

    // Initialize hash buffers
    for (size_t i = 0; i < SimdLanes(); i++)
    {
        memcpy(hashes[i], Hash, SHA1_SIZE);
    }

    for (ssize_t i = Chain.Length() - 1; i >= 0; i--)
    {
        // Do reductions
        topLength = Reduce(&reducedTop[0], Max, hashes[0], SHA1_SIZE, maxindex, i);
        for (size_t j = 1; j < SimdLanes() - 1; j++)
        {
            length = Reduce((char*)words[j - 1], Max, hashes[j], SHA1_SIZE, maxindex, i - j);
            words.SetLength(j - 1, length);
        }
        // Reduce the original hash into the last bucket
        if (i > SimdLanes())
        {
            length = Reduce((char*)words[lastIndex], Max, Hash, SHA1_SIZE, maxindex, i - SimdLanes());
            words.SetLength(lastIndex, length);
        }

        // Check end, if it matches, we can perform one full chain to see if we find it
        if (memcmp(Chain.End().c_str(), &reducedTop[0], topLength) == 0)
        {
            std::cout << "Found End" << std::endl;
            break;
        }

        // Do a round of hashes
        SimdHashContext ctx;
        SimdSha1Init(&ctx);
        SimdHashUpdate(&ctx, words.Lengths(), words.ConstBuffers());
        SimdSha1Finalize(&ctx);
        SimdHashGetHashes(&ctx, hashes.Buffer());
    }
}*/

int
main(
    int argc,
    char* argv[]
)
{
    std::string midpoint;

    size_t min = 6;
    size_t max = 16;
    size_t len = 2000;

    auto chains = ComputeChain(
        0,
        len,
        min,
        max,
        6,
        &midpoint
    );

    std::cout << "Mid (" << midpoint.size() << "): " << midpoint << std::endl;
    uint8_t hash[SHA1_SIZE];
    SHA1((uint8_t*)midpoint.c_str(), midpoint.size(), hash);

    CheckChain(
        chains[0],
        min,
        max,
        &hash[0]
    );
    
    return 0;
}