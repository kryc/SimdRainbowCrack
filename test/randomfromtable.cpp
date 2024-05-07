#include <iostream>
#include <string>
#include <tuple>
#include <openssl/sha.h>

#include "simdhash.h"

#include "RainbowTable.hpp"
#include "WordGenerator.hpp"

std::tuple<std::string, std::string>
GetAtOffset(
    const RainbowTable& Table,
    const size_t Chain,
    const size_t Offset
)
{
    // std::cout << "Using offset " << Offset << " in chain " << Chain << std::endl;

    mpz_class counter = WordGenerator::WordLengthIndex(Table.GetMin(), Table.GetCharset());
    counter += Chain;
    auto start = WordGenerator::GenerateWord(counter, Table.GetCharset());
    // std::cout << "Start: '" << start << "'" << std::endl;

    FILE* fh = fopen(Table.GetPath().c_str(), "r");
    if (fh == nullptr)
    {
        std::cerr << "Unable to open file for reading" << std::endl;
        return {"", ""};
    }
    fseek(fh, sizeof(TableHeader) + Chain * Table.GetMax(), SEEK_SET);
    std::string end;
    end.resize(Table.GetMax());
    fread(&end[0], sizeof(char), Table.GetMax(), fh);
    fclose(fh);
    // std::cout << "End: '" << end << "'" << std::endl;

    size_t hashsize = GetHashWidth(Table.GetAlgorithm());
    std::vector<uint8_t> hash(hashsize);
    std::vector<char> reduced(Table.GetMax());
    
    auto reducer = RainbowTable::GetReducer(Table.GetMin(), Table.GetMax(), hashsize, Table.GetCharset());

    size_t length = start.size();
    memcpy(&reduced[0], start.c_str(), length);

    for (size_t i = 0; i < Table.GetLength(); i++)
    {
        RainbowTable::DoHash((uint8_t*)&reduced[0], length, &hash[0], Table.GetAlgorithm());
        length = reducer->Reduce(&reduced[0], Table.GetMax(), &hash[0], i);
        if (i == Offset)
        {
            std::string output(&reduced[0], &reduced[length]);
            RainbowTable::DoHash((const uint8_t*)&reduced[0], length, &hash[0], Table.GetAlgorithm());
            
            std::string hashString;
            for (size_t h = 0; h < hashsize; h++)
            {
                char buff[3];
                snprintf(buff, sizeof(buff), "%02X", hash[h]);
                buff[2] = '\0';
                hashString += buff;
            }
            return {output, hashString};
        }
    }

    if (memcmp(&reduced[0], &end[0], Table.GetMax()) != 0)
    {
        std::cerr << "Non-matching endpoints!" << std::endl;
    }

    return {"", ""};
}

int
main(
    int argc,
    char* argv[]
)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " [table file] [count]" << std::endl;
        return 0;
    }

    size_t count = 1;
    if (argc == 3)
    {
        count = atoi(argv[2]);
    }

    RainbowTable rainbow;
    rainbow.SetPath(argv[1]);

    if (!rainbow.TableExists())
    {
        std::cerr << "Rainbow table not found" << std::endl;
        return 1;
    }

    if (!rainbow.IsTableFile(argv[1]))
    {
        std::cerr << "Invalid rainbow table file" << std::endl;
        return 1;
    }

    if (!rainbow.LoadTable())
    {
        std::cerr << "Error loading table file" << std::endl;
        return 1;
    }

    std::cerr << "Type:      " << rainbow.GetType() << std::endl;
    std::cerr << "Algorithm: " << rainbow.GetAlgorithmString() << std::endl;
    std::cerr << "Min:       " << rainbow.GetMin() << std::endl;
    std::cerr << "Max:       " << rainbow.GetMax() << std::endl;
    std::cerr << "Length:    " << rainbow.GetLength() << std::endl;
    std::cerr << "Count:     " << rainbow.GetCount() << std::endl;
    std::cerr << "Charset:   \"" << rainbow.GetCharset() << "\"" << std::endl;

    srand(time(nullptr));

    for (size_t i = 0; i < count; i++)
    {
        size_t chain = rand() % rainbow.GetCount();
        size_t offset = rand() % rainbow.GetLength();
        auto result = GetAtOffset(rainbow, chain, offset);
        
        std::cout << std::get<1>(result) << " " << std::get<0>(result) << std::endl;
    }
}