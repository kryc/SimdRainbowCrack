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
main(
    int argc,
    char* argv[]
)
{
    RainbowTable rainbow;
    std::string action;

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " action [-option] table" << std::endl;
        return 0;
    }

    action = argv[1];

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
        else
        {
            rainbow.SetPath(argv[i]);
        }
    }

    rainbow.SetCharset(ASCII);

    if (action == "build")
    {
        if (!rainbow.ValidateConfig())
        {
            std::cerr << "Invalid configuration. Exiting" << std::endl;
            return 1;
        }

        //
        // Create the main dispatcher
        //
        auto mainDispatcher = dispatch::CreateAndEnterDispatcher(
            "main",
            dispatch::bind(
                &RainbowTable::InitAndRun,
                &rainbow
            )
        );
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

        std::cout << "Type:      " << rainbow.GetType() << std::endl;
        std::cout << "Algorithm: " << rainbow.GetAlgorithmString() << std::endl;
        std::cout << "Min:       " << rainbow.GetMin() << std::endl;
        std::cout << "Max:       " << rainbow.GetMax() << std::endl;
        std::cout << "Length:    " << rainbow.GetLength() << std::endl;
        std::cout << "Count:     " << rainbow.GetCount() << std::endl;
        std::cout << "Charset:   \"" << rainbow.GetCharset() << "\"" << std::endl;
    }

    return 0;
}