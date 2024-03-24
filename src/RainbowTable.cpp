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
#include <openssl/md5.h>
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
        m_ChainWidth = GetChainWidth();
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
    if (blockStartId >= m_Count)
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

    auto reducer = GetReducer();
    std::vector<Chain> block;
    block.reserve(m_Blocksize);

    SimdHashBuffer words(m_Max);
    SimdHashBuffer hashes(m_HashWidth);

    // Calculate lower bound
    mpz_class lowerbound = WordGenerator::WordLengthIndex(m_Min, m_Charset);
    // Add the index for this chain
    mpz_class counter = lowerbound + blockStartId;

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
            chains[i].SetIndex(counter - lowerbound);
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
                auto length = reducer->Reduce((char*)words[h], m_Max, hash, i);
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
    size_t buffersize = m_ChainWidth * m_Blocksize;
    buffer.resize(buffersize);
    uint8_t* bufferptr = &buffer[0];
    uint64_t index;
    for (auto& chain : Block)
    {
        if (m_TableType == TypeUncompressed)
        {
            index = chain.Index().get_ui();
            *((uint64_t*)bufferptr) = index;
            bufferptr += sizeof(uint64_t);
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

bool
RainbowTable::SetType(
    const std::string Type
)
{
    if (Type == "compressed")
    {
        SetType(TypeCompressed);
    }
    else if (Type == "uncompressed")
    {
        SetType(TypeUncompressed);
    }
    else
    {
        SetType(TypeInvalid);
        return false;
    }
    return true;
}

void
RainbowTable::StoreTableHeader(
    void
) const
{
    TableHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
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

/* static */ bool
RainbowTable::GetTableHeader(
    const std::filesystem::path& Path,
    TableHeader* Header
)
{
    if (std::filesystem::file_size(Path) < sizeof(TableHeader))
    {
        return false;
    }
    
    std::ifstream fs(Path, std::ios::binary);
    if (!fs.is_open())
    {
        return false;
    }

    fs.read((char*)Header, sizeof(TableHeader));
    fs.close();

    if (Header->magic != kMagic)
    {
        return false;
    }
    return true;
}

/* static */ bool
RainbowTable::IsTableFile(
    const std::filesystem::path& Path
)
{
    TableHeader hdr;
    return GetTableHeader(Path, &hdr);
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
    m_ChainWidth = GetChainWidth();
    m_Chains = (std::filesystem::file_size(m_Path) - sizeof(TableHeader)) / m_ChainWidth;

    size_t dataSize = fileSize - sizeof(TableHeader);
    if (dataSize % m_ChainWidth != 0)
    {
        std::cerr << "Invalid or currupt table file. Data not a multiple of chain width" << std::endl;
        return false;
    }

    return true;
}

const size_t
RainbowTable::GetCount(void) const
{
    return (std::filesystem::file_size(m_Path) - sizeof(TableHeader)) / GetChainWidth();
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

bool
RainbowTable::MapTable(
    const bool ReadOnly
)
{
    m_MappedTableSize = std::filesystem::file_size(m_Path) - sizeof(TableHeader);
    m_MappedTableFd = fopen(m_Path.c_str(), "r");
    if (m_MappedTableFd == nullptr)
    {
        std::cerr << "Unable to open a handle to the table file" << std::endl;
        return false;
    }
    // std::cerr << m_MappedTableSize << std::endl;
    int flags = 0;
    int prot = PROT_READ;
    if (ReadOnly)
    {
        flags = MAP_PRIVATE;
    }
    else
    {
        prot |= PROT_WRITE;
    }
    
    m_MappedTable = (uint8_t*)mmap(nullptr, m_MappedTableSize, prot, flags, fileno(m_MappedTableFd), 0);
    if (m_MappedTable == MAP_FAILED)
    {
        std::cerr << "Unable to map table into memory: " << strerror(errno) << std::endl;
        return false;
    }
    auto ret = madvise(m_MappedTable, m_MappedTableSize, MADV_RANDOM | MADV_WILLNEED);
    if (ret != 0)
    {
        std::cerr << "Madvise not happy" << std::endl;
    }
    return true;
}

void
RainbowTable::DoHash(
    const uint8_t* Data,
    const size_t Length,
    uint8_t* Digest
)
{
    switch (m_Algorithm)
    {
    case HashMd5:
        MD5(Data, Length, Digest);
        break;
    case HashSha1:
        SHA1(Data, Length, Digest);
        break;
    case HashSha256:
        SHA256(Data, Length, Digest);
        break;
    default:
        break;
    }
}

const size_t
RainbowTable::FindEndpoint(
    const char* Endpoint,
    const size_t Length
)
{
    std::vector<char> comparitor;
    comparitor.resize(m_Max);
    memcpy(&comparitor[0], Endpoint, Length);
    size_t skiplen = GetChainWidth();
    if (m_TableType == TypeCompressed)
    {
        // Linear search
        uint8_t* endpoint = m_MappedTable + sizeof(TableHeader);
        for (
            size_t c = 0;
            c < m_Chains;
            c++, endpoint += skiplen)
        {
            if (memcmp(endpoint, &comparitor[0], m_Max) == 0)
            {
                return c;
            }
        }
    }
    else
    {
        // Binary search
        size_t low = 0;
        size_t high = m_Chains - 1;
        uint8_t* startendpoint = m_MappedTable + sizeof(TableHeader) + sizeof(uint64_t);
        while (low <= high)
        {
            size_t mid = (low + high) / 2;
            if (mid > m_Chains)
                break;
            uint8_t* endpoint = startendpoint + (skiplen * mid);
            int cmp = memcmp(endpoint, &comparitor[0], m_Max);
            if (cmp == 0)
            {
                uint64_t* index = (uint64_t*)endpoint - 1;
                return (size_t)*index;
            }
            else if (cmp < 0)
            {
                low = mid + 1;
            }
            else
            {
                high = mid - 1;
            }
        }
    }
    return (size_t)-1;
}

std::unique_ptr<Reducer>
RainbowTable::GetReducer(void)
const
{
    // For tables where the min and max are the same
    // (we are reducing to a constant length), we can
    // use the significantly faster BytewiseReducer
    if (m_Min == m_Max)
    {
        return std::make_unique<BytewiseReducer>(m_Min, m_Max, m_HashWidth, m_Charset);
    }
    // Otherwise we need to fall back to the much slower
    // modulo reducer which requires big integer division
    return std::make_unique<ModuloReducer>(m_Min, m_Max, m_HashWidth, m_Charset);
}

void
RainbowTable::Crack(
    std::string& Hash
)
{
    auto target = Util::ParseHex(Hash);

    std::vector<uint8_t> hash(m_HashWidth);
    std::vector<char> reduced(m_Max);
    auto reducer = GetReducer();
    size_t length;

    // Mmap the table
    if (!MapTable(true))
    {
        return;
    }

    // Perform check
    for (ssize_t i = m_Length - 1; i >= 0; i--)
    {
        memcpy(&hash[0], &target[0], m_HashWidth);

        for (size_t j = i; j < m_Length - 1; j++)
        {
            length = reducer->Reduce(&reduced[0], m_Max, &hash[0], j);
            DoHash((uint8_t*)&reduced[0], length, &hash[0]);
        }

        // Final reduction
        length = reducer->Reduce(&reduced[0], m_Max, &hash[0], m_Length - 1);
    
        // Check end, if it matches, we can perform one full chain to see if we find it
        size_t index = FindEndpoint(&reduced[0], length);
        if (index != (size_t)-1)
        {
            auto match = ValidateChain(index, &target[0]);
            if (match.has_value())
            {
                auto hashstr = Util::ToHex(&target[0], m_HashWidth);
                std::cout << hashstr << ' ' << match.value() << std::endl;
                return;
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
    auto reducer = GetReducer();
    mpz_class counter = WordGenerator::WordLengthIndex(m_Min, m_Charset);
    counter += ChainIndex;

    auto start = WordGenerator::GenerateWord(counter,m_Charset);
    length = start.size();
    memcpy(&reduced[0], start.c_str(), length);
    
    for (size_t i = 0; i < m_Length; i++)
    {
        DoHash((uint8_t*)&reduced[0], length, &hash[0]);
        if (memcmp(Target, &hash[0], m_HashWidth) == 0)
        {
            return std::string(&reduced[0], &reduced[length]);
        }
        length = reducer->Reduce(&reduced[0], m_Max, &hash[0], i);
    }
    return {};
}

RainbowTable::~RainbowTable(
    void
)
{
    Reset();
}

void
RainbowTable::Reset(
    void
)
{
    if (m_MappedTable != nullptr)
    {
        munmap(m_MappedTable, m_MappedTableSize);
    }

    if (m_MappedTableFd != nullptr)
    {
        fclose(m_MappedTableFd);
    }

    m_Path.clear();
    m_PathLoaded = false;
    m_Algorithm = HashUnknown;
    m_Min = 0;
    m_Max = 0;
    m_Length = 0;
    m_Blocksize = 1024;
    m_Count = 0;
    m_Threads = 0;
    m_Charset.clear();
    m_HashWidth = 0;
    m_ChainWidth = 0;
    m_Chains = 0;
    m_TableType = TypeCompressed;
    // For building
    m_StartingChains = 0;
    m_WriteHandle = nullptr;
    m_NextWriteBlock = 0;
    m_WriteCache.clear();
    if (m_DispatchPool != nullptr)
    {
        m_DispatchPool->Stop();
        m_DispatchPool->Wait();
        m_DispatchPool = nullptr;
    }
    m_ThreadsCompleted = 0;
}

int SortCompare(
    const void* Comp1,
    const void* Comp2,
    void* Arguments
)
{
    return memcmp((uint8_t*)Comp1 + sizeof(uint64_t), (uint8_t*)Comp2 + sizeof(uint64_t), (size_t)Arguments);
}

void
RainbowTable::SortTable(
    void
)
{
    if (!MapTable(true))
    {
        std::cerr << "Error mapping table for sort"  << std::endl;
        return;
    }

    uint8_t* start = m_MappedTable + sizeof(TableHeader);
    qsort_r(start, m_Count, GetChainWidth(), SortCompare, (void*)m_Max);
}

void
RainbowTable::Decompress(
    const std::filesystem::path& Destination
)
{
    FILE* fhw;
    TableHeader hdr;

    if (!MapTable(true))
    {
        std::cerr << "Error mapping table"  << std::endl;
        return;
    }

    hdr = *((TableHeader*)m_MappedTable);
    hdr.type = TypeUncompressed;

    fhw = fopen(Destination.c_str(), "w");
    if (fhw == nullptr)
    {
        std::cerr << "Error opening desination table for write" << std::endl;
        return;
    }

    fwrite(&hdr, sizeof(hdr), 1, fhw);
    uint8_t* next = m_MappedTable + sizeof(TableHeader);
    for (size_t index = 0; index < m_Chains; index++, next += m_Max)
    {
        fwrite(&index, sizeof(index), 1, fhw);
        fwrite(next, m_Max, sizeof(uint8_t), fhw);
    }
    fclose(fhw);

    RainbowTable newtable;
    newtable.SetPath(Destination);
    newtable.SortTable();
}

/* static */ const Chain
RainbowTable::GetChain(
    const std::filesystem::path& Path,
    const size_t Index
)
{
    TableHeader hdr;
    GetTableHeader(Path, &hdr);

    std::string charset(&hdr.charset[0], &hdr.charset[hdr.charsetlen]);

    Chain chain;
    chain.SetIndex(Index);
    chain.SetLength(hdr.length);

    FILE* fh = fopen(Path.c_str(), "r");
    fseek(fh, sizeof(TableHeader), SEEK_SET);
    fseek(fh, ChainWidthForType((TableType)hdr.type, hdr.max) * Index, SEEK_CUR);

    uint64_t start = Index;
    if (hdr.type == (uint8_t)TypeUncompressed)
    {
        fread(&start, sizeof(uint64_t), 1, fh);
        
    }
    mpz_class lowerbound = WordGenerator::WordLengthIndex(hdr.min, charset);
    auto word = WordGenerator::GenerateWord(lowerbound + start, charset);
    chain.SetStart(word);

    std::string endpoint;
    endpoint.resize(hdr.max);
    fread(&endpoint[0], sizeof(char), 1, fh);
    // Trim nulls
    endpoint.resize(strlen(endpoint.c_str()));
    chain.SetEnd(endpoint);

    return chain;
}