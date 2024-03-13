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

    auto mainDispatcher = dispatch::CreateAndEnterDispatcher(
        "main",
        dispatch::bind(
            &RainbowTable::InitAndRunBuild,
            &rainbow
        )
    );

    // Restart the dispatcher
    mainDispatcher->Start();

    RainbowTable rainbow2;
    rainbow2.SetPath("test.bin");
    rainbow2.SetThreads(1);
    rainbow2.SetBlocksize(32);
    rainbow2.SetCount(256);
    dispatch::PostTaskToDispatcher(
        "main",
        dispatch::bind(
            &RainbowTable::InitAndRunBuild,
            &rainbow2
        )
    );
}