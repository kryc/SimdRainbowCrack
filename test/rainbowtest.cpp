#include <array>
#include <iostream>
#include <gmpxx.h>
#include <string>
#include <tuple>
#include <vector>
#include <openssl/sha.h>

#include "simdhash.h"
#include "Chain.hpp"
#include "Util.hpp"
#include "WordGenerator.hpp"

using ChainBlock = std::vector<Chain>;

size_t
Reduce(
    char* Destination,
    size_t DestLength,
    const uint8_t* Hash,
    const size_t Length,
    const mpz_class MaxIndex,
    const size_t Iteration
)
{
    mpz_class reduction;
    mpz_class iteration(Iteration);

    mpz_import(reduction.get_mpz_t(), Length, 1, sizeof(uint8_t), 1, 0, Hash);
    reduction ^= iteration;
    reduction %= MaxIndex;

    return WordGenerator::GenerateWord(
        Destination,
        DestLength,
        reduction,
        ASCII
    );
}

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
    std::vector<uint8_t> buffer;
    std::vector<uint8_t*> buffers;
    std::vector<size_t> lengths;
    std::vector<uint8_t> hashes;
    ChainBlock ret;
    
    mpz_class counter;
    mpz_class maxindex;

    // Calculate lower bound
    counter = WordGenerator::WordLengthIndex(Min, ASCII);
    // Calculate the upper bount
    maxindex = WordGenerator::WordLengthIndex(Max + 1, ASCII);
    // Add the index for this chain
    counter += Start;

    buffer.resize((Max + 1) * SimdLanes());
    buffers.resize((Max + 1) * SimdLanes());
    for (size_t i = 0; i < SimdLanes(); i++)
    {
        buffers[i] = &buffer[i * (Max + 1)];
    }

    lengths.resize(SimdLanes());
    hashes.resize(SimdLanes() * SHA1_SIZE);

    for (size_t i = 0; i < SimdLanes(); i++)
    {
        auto length = WordGenerator::GenerateWord((char*)buffers[i], Max, counter, ASCII);
        lengths[i] = length;
        buffers[i][length] = '\0';
        std::string start((char*)buffers[i]);
        ret.push_back(Chain(counter, std::move(start), Length));
        counter++;
    }

    for (size_t i = 0; i < Length; i++)
    {
        SimdHashContext ctx;
        SimdSha1Init(&ctx);
        SimdHashUpdate(&ctx, &lengths[0], (const uint8_t**)&buffers[0]);
        SimdSha1Finalize(&ctx);
        SimdHashGetHashes(&ctx, &hashes[0]);

        for (size_t h = 0; h < SimdLanes(); h++)
        {
            const uint8_t* hash = &hashes[h * SHA1_SIZE];
            auto length = Reduce((char*)buffers[h], Max, hash, SHA1_SIZE, maxindex, i);
            lengths[h] = length;
            buffers[h][length] = '\0';
        }

        if (i == Capture)
        {
            Midpoint->assign(std::string((char*)buffers[0], (char*)(buffers[0] + lengths[0])));
        }
    }

    for (size_t i = 0; i < SimdLanes(); i++)
    {
        ret[i].SetEnd((char*)buffers[i]);
    }

    return ret;
}

bool
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
    mpz_class maxindex;
    
    maxindex = WordGenerator::WordLengthIndex(Max + 1, ASCII);

    length = Chain.Start().size();
    memcpy(&reduced[0], Chain.Start().c_str(), length);
    
    for (size_t i = 0; i < Chain.Length(); i++)
    {
        SHA1((uint8_t*)&reduced[0], length, hash);
        if (memcmp(Hash, hash, SHA1_SIZE) == 0)
        {
            return true;
        }
        length = Reduce(&reduced[0], Max, hash, SHA1_SIZE, maxindex, i);
    }
    return false;
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
    mpz_class maxindex;

    // Get the start index
    start = Chain.Index();

    // Calculate lower bound
    counter = WordGenerator::WordLengthIndex(Min, ASCII);
    // Calculate the upper bount
    maxindex = WordGenerator::WordLengthIndex(Max + 1, ASCII);
    // Add the index for this chain
    counter += start;

    uint8_t hash[SHA1_SIZE];
    std::vector<char> reduced(Max + 1);

    size_t length;

    for (ssize_t i = Chain.Length() - 1; i >= 0; i--)
    {
        memcpy(hash, Hash, SHA1_SIZE);

        for (size_t j = i; j < Chain.Length() - 1; j++)
        {
            length = Reduce(&reduced[0], Max, hash, SHA1_SIZE, maxindex, j);
            SHA1((uint8_t*)&reduced[0], length, hash);
        }

        // Final reduction
        length = Reduce(&reduced[0], Max, hash, SHA1_SIZE, maxindex, Chain.Length() - 1);
    
        // Check end, if it matches, we can perform one full chain to see if we find it
        if (memcmp(Chain.End().c_str(), &reduced[0], length) == 0)
        {
            auto hashstr = Util::ToHex(Hash, SHA1_SIZE);
            std::string result(&reduced[0], &reduced[length]);
            std::cout << hashstr << ' ' << result << std::endl;
        }
    }
}

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
        5,
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