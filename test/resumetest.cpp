#include <filesystem>
#include <iostream>
#include <openssl/sha.h>

#include "simdhash.h"

#include "RainbowTable.hpp"
#include "WordGenerator.hpp"

int main(
    int argc,
    char* argv[]
)
{
    std::filesystem::remove("test.bin");

    RainbowTable rainbow;
    rainbow.SetPath("test.bin");
    rainbow.SetCharset(ASCII);
    rainbow.SetMin(12);
    rainbow.SetMax(12);
    rainbow.SetLength(50);
    rainbow.SetAlgorithm("sha1");
    rainbow.SetThreads(1);
    rainbow.SetBlocksize(32);
    rainbow.SetCount(32);
    rainbow.SetType("uncompressed");

    auto mainDispatcher = dispatch::CreateDispatcher("main");

    dispatch::PostTaskToDispatcher(
        mainDispatcher,
        dispatch::bind(
            &RainbowTable::InitAndRunBuild,
            &rainbow
        )
    );

    mainDispatcher->Wait();

    rainbow.Reset();

    // Restart the dispatcher
    mainDispatcher->Start();

    rainbow.SetPath("test.bin");
    rainbow.SetThreads(1);
    rainbow.SetBlocksize(32);
    rainbow.SetCount(64);
    dispatch::PostTaskToDispatcher(
        mainDispatcher,
        dispatch::bind(
            &RainbowTable::InitAndRunBuild,
            &rainbow
        )
    );

    mainDispatcher->Wait();

    rainbow.Reset();

    std::cout << "Checking chains" << std::endl;
    bool error = false;
    mpz_class lowerbound = WordGenerator::WordLengthIndex(12, ASCII);

    for (size_t i = 0; i < 64; i++)
    {
        auto chain = RainbowTable::GetChain("test.bin", i);
        if (chain.Index() != i)
        {
            std::cerr << "Non-matching index: " << i << " != " << chain.Index() << std::endl;
            error = true;
        }
        
        mpz_class index = lowerbound + i;

        std::string targetstart = WordGenerator::GenerateWord(index, ASCII);
        if (chain.Start() != targetstart)
        {
            std::cerr << "Non-matching start word: " << targetstart << " != " << chain.Start() << std::endl;
            error = true;
        }

        // Check the chain
        auto computed = RainbowTable::ComputeChain(i, 12, 12, 50, HashSha1, ASCII);
        if (chain.Start() != computed.Start())
        {
            std::cerr << "Computed start does not match: " << chain.Start() << " != " << computed.Start() << std::endl;
            error = true;
        }
        if (chain.End() != computed.End())
        {
            std::cerr << "Computed end does not match: " << chain.End() << " != " << computed.End() << std::endl;
            error = true;
        }
    }

    if (error == false)
    {
        std::cout << "No issues found" << std::endl;
    }
}