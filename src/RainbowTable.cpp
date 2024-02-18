//
//  RainbowTable.cpp
//  RainbowCrack-
//
//  Created by Kryc on 15/02/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#include <cinttypes>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <openssl/sha.h>

#include "SimdHashBuffer.hpp"

#include "Chain.hpp"
#include "RainbowTable.hpp"
#include "Util.hpp"

void
RainbowTable::InitAndRunBuild(
    void
)
{
    if (m_Threads == 0)
    {
        m_Threads = std::thread::hardware_concurrency();
    }

    // Validate config and load header if exists
    if (!ValidateConfig())
    {
        std::cerr << "Configuration error" << std::endl;
        return;
    }

    // Write the table header if we didn't load from disk
    if (!m_PathLoaded)
    {
        StoreTableHeader();
        m_HashWidth = GetHashWidth(m_Algorithm);
        m_ChainWidth = m_TableType == TypeCompressed ? m_Max : m_Max * 2;
        m_Chains = (std::filesystem::file_size(m_Path) - sizeof(TableHeader)) / m_ChainWidth;
    }

    m_StartingChains = m_Chains;

    m_WriteHandle = fopen(m_Path.c_str(), "a");
    if (m_WriteHandle == nullptr)
    {
        std::cerr << "Unable to open table for writing" << std::endl;
        return;
    }

    m_DispatchPool = dispatch::CreateDispatchPool("pool", m_Threads);

    for (size_t i = 0; i < m_Threads; i++)
    {
        m_DispatchPool->PostTask(
            dispatch::bind(
                &RainbowTable::GenerateBlock,
                this,
                i,
                i
            )
        );
    }
}

void
RainbowTable::GenerateBlock(
    const size_t ThreadId,
    const size_t BlockId
)
{
    size_t blockStartId = m_StartingChains + (m_Blocksize * BlockId);

    // Check if we should end
    if (blockStartId > m_Count)
    {
        dispatch::PostTaskToDispatcher(
            "main",
            dispatch::bind(
                &RainbowTable::ThreadCompleted,
                this,
                ThreadId
            )
        );
        return;
    }

    BigIntReducer reducer(m_Min, m_Max, m_HashWidth, m_Charset);
    std::vector<Chain> block;
    block.reserve(m_Blocksize);

    SimdHashBuffer words(m_Max);
    SimdHashBuffer hashes(m_HashWidth);

    // Calculate lower bound
    mpz_class counter = WordGenerator::WordLengthIndex(m_Min, m_Charset);
    // Add the index for this chain
    counter += blockStartId;

    size_t iterations = m_Blocksize / SimdLanes();
    for (size_t iteration = 0; iteration < iterations; iteration++)
    {
        std::vector<Chain> chains;
        chains.resize(SimdLanes());

        // Set the chain start point
        for (size_t i = 0; i < SimdLanes(); i++)
        {
            auto length = WordGenerator::GenerateWord((char*)words[i], m_Max, counter, m_Charset);
            words.SetLength(i, length);
            chains[i].SetStart(words[i], length);
            chains[i].SetIndex(counter);
            counter++;
        }

        // Perform the hash/reduce cycle
        for (size_t i = 0; i < m_Length; i++)
        {
            // Perform hash
            SimdHashContext ctx;
            SimdHashInit(&ctx, m_Algorithm);
            SimdHashUpdate(&ctx, words.Lengths(), words.ConstBuffers());
            SimdHashFinalize(&ctx);
            SimdHashGetHashes(&ctx, hashes.Buffer());

            // Perform reduce
            for (size_t h = 0; h < SimdLanes(); h++)
            {
                const uint8_t* hash = hashes[h];
                auto length = reducer.Reduce((char*)words[h], m_Max, hash, i);
                words.SetLength(h, length);
            }
        }

        // Save the chain information
        
        for (size_t h = 0; h < SimdLanes(); h++)
        {
            chains[h].SetEnd(words[h], words.GetLength(h));
            block.push_back(std::move(chains[h]));
        }
    }

    //
    // Post a task to the main thread
    // to save this block
    //
    dispatch::PostTaskToDispatcher(
        "main",
        dispatch::bind(
            &RainbowTable::SaveBlock,
            this,
            BlockId,
            std::move(block)
        )
    );

    //
    // Post the next task
    //
    size_t nextblock = BlockId + m_Threads;
    dispatch::PostTaskFast(
        dispatch::bind(
            &RainbowTable::GenerateBlock,
            this,
            ThreadId,
            nextblock
        )
    );
}

void
RainbowTable::WriteBlock(
    const size_t BlockId,
    const ChainBlock& Block
)
{
    // Create a byte buffer so we can do it on one shot
    std::vector<uint8_t> buffer;
    size_t buffersize = m_TableType == TypeCompressed ? m_Max * m_Blocksize : m_Max * 2 * m_Blocksize;
    buffer.resize(buffersize);
    uint8_t* bufferptr = &buffer[0];
    for (auto& chain : Block)
    {
        if (m_TableType == TypeUncompressed)
        {
            memcpy(bufferptr, chain.Start().c_str(), chain.Start().size());
            bufferptr += m_Max;
        }
        memcpy(bufferptr, chain.End().c_str(), chain.End().size());
        bufferptr += m_Max;
    }
    fwrite(&buffer[0], buffersize, sizeof(uint8_t), m_WriteHandle);
    fflush(m_WriteHandle);
}

void
RainbowTable::SaveBlock(
    const size_t BlockId,
    const std::vector<Chain> Block
)
{
    if (BlockId % m_Threads == 0)
    {
        std::cout << "'" << Block[0].Start() << "' -> '" << Block[0].End() << "'" << std::endl;
    }
    if (BlockId == m_NextWriteBlock)
    {
        WriteBlock(BlockId, Block);
        m_NextWriteBlock++;
        while (m_WriteCache.find(m_NextWriteBlock) != m_WriteCache.end())
        {
            WriteBlock(m_NextWriteBlock, m_WriteCache.at(m_NextWriteBlock));
            m_WriteCache.erase(m_NextWriteBlock);
            m_NextWriteBlock++;
        }
    }
    else
    {
        m_WriteCache.emplace(BlockId, std::move(Block));
    }
}

void
RainbowTable::SetType(
    const std::string Type
)
{
    if (Type == "compressed")
    {
        m_TableType = TypeCompressed;
    }
    else if (Type == "uncompressed")
    {
        m_TableType = TypeUncompressed;
    }
    else
    {
        m_TableType = TypeInvalid;
    }
}

void
RainbowTable::StoreTableHeader(
    void
) const
{
    TableHeader hdr;
    hdr.magic = kMagic;
    hdr.type = m_TableType;
    hdr.algorithm = m_Algorithm;
    hdr.min = m_Min;
    hdr.max = m_Max;
    hdr.length = m_Length;
    hdr.charsetlen = m_Charset.size();
    strncpy(hdr.charset, &m_Charset[0], sizeof(hdr.charset));

    std::ofstream fs(m_Path, std::ios::out | std::ios::binary);
    fs.write((const char*)&hdr, sizeof(hdr));
    fs.close();
}

bool
RainbowTable::IsTableFile(
    void
)
{
    TableHeader hdr;
    std::ifstream fs(m_Path, std::ios::binary);
    if (!fs.is_open())
    {
        return false;
    }

    fs.read((char*)&hdr, sizeof(hdr));
    fs.close();

    if (hdr.magic != kMagic)
    {
        return false;
    }
    return true;
}

bool
RainbowTable::LoadTable(
    void
)
{
    TableHeader hdr;

    size_t fileSize = std::filesystem::file_size(m_Path);

    if(fileSize < sizeof(TableHeader))
    {
        std::cerr << "Not enough data in file" << std::endl;
        return false;
    }

    std::ifstream fs(m_Path, std::ios::binary);
    if (!fs.is_open())
    {
        std::cerr << "Unable to open RainbowTable" << std::endl;
        return false;
    }

    fs.read((char*)&hdr, sizeof(hdr));
    fs.close();

    if (hdr.magic != kMagic)
    {
        std::cerr << "Invalid RainbowTable file" << std::endl;
        return false;
    }

    m_TableType = (TableType)hdr.type;
    m_Algorithm = (HashAlgorithm)hdr.algorithm;
    m_Min = hdr.min;
    m_Max = hdr.max;
    m_Length = hdr.length;
    m_Charset = std::string(&hdr.charset[0], &hdr.charset[hdr.charsetlen]);
    m_HashWidth = GetHashWidth(m_Algorithm);
    m_ChainWidth = m_TableType == TypeCompressed ? m_Max : m_Max * 2;
    m_Chains = (std::filesystem::file_size(m_Path) - sizeof(TableHeader)) / m_ChainWidth;

    return true;
}

const size_t
RainbowTable::GetCount(void) const
{
    size_t chainwidth = m_TableType == TypeCompressed ? m_Max : m_Max * 2;
    return (std::filesystem::file_size(m_Path) - sizeof(TableHeader)) / chainwidth;
}

bool
RainbowTable::ValidateConfig(
    void
)
{
    if (m_Path.empty())
    {
        std::cerr << "No rainbow table file specified" << std::endl;
        return false;
    }

    if (TableExists())
    {
        LoadTable();
        m_PathLoaded = true;
    }

    if (m_Max == 0)
    {
        std::cerr << "No max length specified" << std::endl;
        return false;
    }

    if (m_Length == 0)
    {
        std::cerr << "No chain length specified" << std::endl;
        return false;
    }

    if (m_Algorithm == HashUnknown)
    {
        std::cerr << "No algorithm specified" << std::endl;
        return false;
    }

    if (m_TableType == TypeInvalid)
    {
        std::cerr << "Invalid table type specified" << std::endl;
        return false;
    }

    if (m_Blocksize == 0)
    {
        std::cerr << "No block size specified" << std::endl;
        return false;
    }

    if (m_Blocksize % SimdLanes() != 0)
    {
        std::cerr << "Block size must be a multiple of Simd width (" << SimdLanes() << ")" << std::endl;
        return false;
    }

    if (m_Charset.empty())
    {
        std::cerr << "No or invalid charset specified" << std::endl;
        return false;
    }

    if (m_Count == 0)
    {
        std::cerr << "No count specified" << std::endl;
        return false;
    }

    return true;
}

void
RainbowTable::ThreadCompleted(
    const size_t ThreadId
)
{
    m_ThreadsCompleted++;
    if (m_ThreadsCompleted == m_Threads)
    {
        std::cout << "Table Creation completed" << std::endl;
        // Stop the pool
        m_DispatchPool->Stop();
        m_DispatchPool->Wait();

        // Stop the current (main) dispatcher
        dispatch::CurrentDispatcher()->Stop();
    }
}

void
RainbowTable::Crack(
    std::string& Hash
)
{
    auto target = Util::ParseHex(Hash);

    // Mmap the table
    m_MappedTableSize = std::filesystem::file_size(m_Path) - sizeof(TableHeader);
    m_MappedTableFd = fopen(m_Path.c_str(), "r");
    if (m_MappedTableFd == nullptr)
    {
        std::cerr << "Unable to open a handle to the table file" << std::endl;
        return;
    }
    // std::cerr << m_MappedTableSize << std::endl;
    m_MappedTable = (uint8_t*)mmap(nullptr, m_MappedTableSize, PROT_READ, MAP_PRIVATE, fileno(m_MappedTableFd), 0);
    if (m_MappedTable == MAP_FAILED)
    {
        std::cerr << "Unable to map table into memory: " << strerror(errno) << std::endl;
        return;
    }
    auto ret = madvise(m_MappedTable, m_MappedTableSize, MADV_RANDOM | MADV_WILLNEED);
    if (ret != 0)
    {
        std::cerr << "Madvise not happy" << std::endl;
    }

    std::vector<uint8_t> hash(m_HashWidth);
    std::vector<char> reduced(m_Max);
    BigIntReducer reducer(m_Min, m_Max, m_HashWidth, m_Charset);
    size_t length;

    // Perform check
    for (ssize_t i = m_Length - 1; i >= 0; i--)
    {
        memcpy(&hash[0], &target[0], m_HashWidth);

        for (size_t j = i; j < m_Length - 1; j++)
        {
            length = reducer.Reduce(&reduced[0], m_Max, &hash[0], j);
            SHA1((uint8_t*)&reduced[0], length, &hash[0]);
        }

        // Final reduction
        length = reducer.Reduce(&reduced[0], m_Max, &hash[0], m_Length - 1);
    
        // Check end, if it matches, we can perform one full chain to see if we find it
        uint8_t* endpoint = m_TableType == TypeCompressed ? m_MappedTable + sizeof(TableHeader) : m_MappedTable + sizeof(TableHeader) + m_Max;
        size_t skiplen = m_TableType == TypeCompressed ? m_Max : m_Max * 2;
        for (
            size_t c = 0;
            c < m_Chains;
            c++, endpoint += skiplen)
        {
            if (memcmp(endpoint, &reduced[0], length) == 0)
            {
                auto match = ValidateChain(c, &target[0]);
                if (match.has_value())
                {
                    auto hashstr = Util::ToHex(&target[0], m_HashWidth);
                    std::cout << hashstr << ' ' << match.value() << std::endl;
                    return;
                }
            }
            else
            {
                m_FalsePositives++;
            }
        }
        
    }
}

std::optional<std::string>
RainbowTable::ValidateChain(
    const size_t ChainIndex,
    const uint8_t* Target
)
{
    size_t length;
    std::vector<uint8_t> hash(m_HashWidth);
    std::vector<char> reduced(m_Max);
    BigIntReducer reducer(m_Min, m_Max, m_HashWidth, m_Charset);
    mpz_class counter = WordGenerator::WordLengthIndex(m_Min, m_Charset);
    counter += ChainIndex;

    auto start = WordGenerator::GenerateWord(counter,m_Charset, false);
    length = start.size();
    memcpy(&reduced[0], start.c_str(), length);
    
    for (size_t i = 0; i < m_Length; i++)
    {
        SHA1((uint8_t*)&reduced[0], length, &hash[0]);
        if (memcmp(Target, &hash[0], m_HashWidth) == 0)
        {
            return std::string(&reduced[0], &reduced[length]);
        }
        length = reducer.Reduce(&reduced[0], m_Max, &hash[0], i);
    }
    return {};
}

RainbowTable::~RainbowTable(
    void
)
{
    if (m_MappedTable != nullptr)
    {
        munmap(m_MappedTable, m_MappedTableSize);
    }
}