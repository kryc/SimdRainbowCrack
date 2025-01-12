//
//  main.cpp
//  RainbowCrack-
//
//  Created by Kryc on 15/02/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#include <filesystem>
#include <iostream>
#include <string>

#include "simdhash.h"

#include "RainbowTable.hpp"

#define ARGCHECK() \
    if (argc <= i) \
    { \
        std::cerr << "No value specified for " << arg << std::endl; \
        return 1; \
    }

int
VerifyAndLoad(
    RainbowTable& Table
)
{
    if (!Table.ValidTable())
    {
        std::cerr << "Provided table not found or invalid" << std::endl;
        return 1;
    }

    if (!Table.LoadTable())
    {
        std::cerr << "Error loading table file" << std::endl;
        return 1;
    }

    return 0;
}

int
main(
    int argc,
    char* argv[]
)
{
    RainbowTable rainbow;
    std::string action, target, destination;

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " action [-option] table" << std::endl;
        return 0;
    }

    size_t avx = SimdLanes() * 32;
    std::cout << "SimdRainbowCrack (AVX-" << avx << ")" << std::endl;

    action = argv[1];

    // Set the default charset
    rainbow.SetCharset("ascii");

    for (int i = 2; i < argc; i++)
	{
		std::string arg = argv[i];
        if (arg == "--min")
        {
            ARGCHECK();
            rainbow.SetMin(std::atoi(argv[++i]));
        }
        else if (arg == "--max")
        {
            ARGCHECK();
            rainbow.SetMax(std::atoi(argv[++i]));
        }
        else if (arg == "--chars")
        {
            ARGCHECK();
            rainbow.SetMin(std::atoi(argv[++i]));
            rainbow.SetMax(std::atoi(argv[i]));
        }
        else if (arg == "--charset")
        {
            ARGCHECK();
            rainbow.SetCharset(argv[++i]);
        }
        else if (arg == "--length")
        {
            ARGCHECK();
            rainbow.SetLength(std::atoi(argv[++i]));
        }
        else if (arg == "--blocksize")
        {
            ARGCHECK();
            rainbow.SetBlocksize(std::atoi(argv[++i]));
        }
        else if (arg == "--count")
        {
            ARGCHECK();
            rainbow.SetCount(std::atoi(argv[++i]));
        }
        else if (arg == "--threads")
        {
            ARGCHECK();
            rainbow.SetThreads(std::atoi(argv[++i]));
        }
        else if (arg == "--algorithm")
        {
            ARGCHECK();
            rainbow.SetAlgorithm(argv[++i]);
        }
        else if (arg == "--md4")
        {
            rainbow.SetAlgorithm("md4");
        }
        else if (arg == "--md5")
        {
            rainbow.SetAlgorithm("md5");
        }
        else if (arg == "--sha1")
        {
            rainbow.SetAlgorithm("sha1");
        }
        else if (arg == "--sha256")
        {
            rainbow.SetAlgorithm("sha256");
        }
        else if (arg == "--ntlm")
        {
            rainbow.SetAlgorithm("ntlm");
        }
        else if (arg == "--noindex")
        {
            rainbow.DisableIndex();
        }
        else if (rainbow.GetPath().empty())
        {
            rainbow.SetPath(argv[i]);
        }
        else if (action == "crack" || action == "test")
        {
            target = argv[i];
        }
        else if (action == "decompress" || action == "compress")
        {
            destination = argv[i];
        }
    }

    if (action == "build" || action == "resume")
    {
        if (!rainbow.ValidateConfig())
        {
            std::cerr << "Invalid configuration. Exiting" << std::endl;
            return 1;
        }

        rainbow.InitAndRunBuild();
    }
    else if (action == "crack")
    {
        int check = VerifyAndLoad(rainbow);
        if (check != 0)
        {
            return check;
        }

        rainbow.Crack(target);
    }
    else if (action == "decompress" || action == "compress")
    {
        if (!rainbow.ValidTable())
        {
            std::cerr << "Provided table not found or invalid" << std::endl;
            return 1;
        }

        if (!rainbow.LoadTable())
        {
            std::cerr << "Error loading table file" << std::endl;
            return 1;
        }

        if (action == "decompress")
        {
            if (destination.empty())
            {
                auto tablepath = rainbow.GetPath();
                auto extension = tablepath.extension();
                destination = tablepath.replace_extension(".utbl");
            }
            rainbow.Decompress(destination);
        }
        else
        {
            rainbow.Compress(destination);
        }
    }
    else if (action == "info")
    {
        if (!rainbow.TableExists())
        {
            std::cerr << "Rainbow table not found" << std::endl;
            return 1;
        }

        if (!rainbow.IsTableFile())
        {
            std::cerr << "Invalid rainbow table file" << std::endl;
            return 1;
        }

        if (!rainbow.LoadTable())
        {
            std::cerr << "Error loading table file" << std::endl;
            return 1;
        }

        std::cout << "Type:        " << rainbow.GetType() << std::endl;
        std::cout << "Algorithm:   " << rainbow.GetAlgorithmString() << std::endl;
        std::cout << "Min:         " << rainbow.GetMin() << std::endl;
        std::cout << "Max:         " << rainbow.GetMax() << std::endl;
        std::cout << "Length:      " << rainbow.GetLength() << std::endl;
        std::cout << "Count:       " << rainbow.GetCount() << std::endl;
        std::cout << "Charset:     \"" << rainbow.GetCharset() << "\"" << std::endl;
        std::cout << "Charset Len: " << rainbow.GetCharset().size() << std::endl;
        std::cout << "KS Coverage: " << rainbow.GetCoverage() << std::endl;
    }
    else if (action == "test")
    {
        int check = VerifyAndLoad(rainbow);
        if (check != 0)
        {
            return check;
        }

        auto hash = rainbow.DoHashHex((uint8_t*)&target[0], target.size());
        std::cout << "Testing for password \"" << target << "\": " << hash << std::endl;

        rainbow.Crack(hash);
    }

    return 0;
}