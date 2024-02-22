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
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " [table file]" << std::endl;
        return 0;
    }

    RainbowTable rainbow;
    rainbow.SetPath(argv[1]);

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

    srand(time(nullptr));
    size_t chain = rand() % rainbow.GetCount();
    size_t offset = rand() % rainbow.GetLength();
    std::cout << "Using offset " << offset << " in chain " << chain << std::endl;

    mpz_class counter = WordGenerator::WordLengthIndex(rainbow.GetMin(), rainbow.GetCharset());
    counter += chain;
    auto start = WordGenerator::GenerateWord(counter, rainbow.GetCharset(), false);
    std::cout << "Start: '" << start << "'" << std::endl;

    FILE* fh = fopen(argv[1], "r");
    if (fh == nullptr)
    {
        std::cerr << "Unable to open file for reading" << std::endl;
        return 1;
    }
    fseek(fh, sizeof(TableHeader) + chain * rainbow.GetMax(), SEEK_SET);
    std::string end;
    end.resize(rainbow.GetMax());
    fread(&end[0], sizeof(char), rainbow.GetMax(), fh);
    fclose(fh);
    std::cout << "End: '" << end << "'" << std::endl;

    size_t hashsize = GetHashWidth(rainbow.GetAlgorithm());
    std::vector<uint8_t> hash(hashsize);
    std::vector<char> reduced(rainbow.GetMax());
    
    BasicModuloReducer reducer(rainbow.GetMin(), rainbow.GetMax(), hashsize, rainbow.GetCharset());

    size_t length = start.size();
    memcpy(&reduced[0], start.c_str(), length);

    for (size_t i = 0; i < rainbow.GetLength(); i++)
    {
        SHA1((uint8_t*)&reduced[0], length, &hash[0]);
        length = reducer.Reduce(&reduced[0], rainbow.GetMax(), &hash[0], i);
        if (i == offset)
        {
            std::cout << "Output: '" << std::string(&reduced[0], &reduced[length]) << "'" << std::endl;
            SHA1((const uint8_t*)&reduced[0], length, &hash[0]);
                
            std::cout << "Hash: ";
            for (size_t i = 0; i < hashsize; i++)
            {
                printf("%0X", hash[i]);
            }
            std::cout << std::endl;
        }
    }

    if (memcmp(&reduced[0], &end[0], rainbow.GetMax()) != 0)
    {
        std::cerr << "Non-matching endpoints!" << std::endl;
        // std::cerr << reduced << " != " << start << std::endl
    }

}