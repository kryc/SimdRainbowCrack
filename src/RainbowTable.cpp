//
//  RainbowTable.cpp
//  RainbowCrack-
//
//  Created by Kryc on 15/02/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#include <cinttypes>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <unistd.h>

#include "SimdHashBuffer.hpp"

#include "Chain.hpp"
#include "Common.hpp"
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

    if (m_Threads > 1)
    {
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
    else
    {
        dispatch::PostTaskFast(
            dispatch::bind(
                &RainbowTable::GenerateBlock,
                this,
                0,
                0
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
    std::vector<SmallString> block(m_Blocksize);

    SimdHashBufferFixed<MAX_OPTIMIZED_BUFFER_SIZE> words;
    std::array<uint8_t, MAX_HASH_SIZE * MAX_LANES> hashes;

    // Calculate lower bound and add the current index
    mpz_class counter = CalculateLowerBound() + blockStartId;
    const size_t hashWidth = m_HashWidth;
    
    // Start measuring the block generation time
    const auto start = std::chrono::system_clock::now();

    const size_t iterations = m_Blocksize / SimdLanes();
    for (size_t iteration = 0; iteration < iterations; iteration++)
    {
        // Set the chain start point
        for (size_t i = 0; i < SimdLanes(); i++)
        {
            const auto length = WordGenerator::GenerateWord((char*)words[i], m_Max, counter, m_Charset);
            assert(length != -1);
            words.SetLength(i, length);
            counter++;
        }

        // Perform the hash/reduce cycle
        for (size_t i = 0; i < m_Length; i++)
        {
            // Perform hash
            SimdHashContext ctx;
            SimdHashInit(&ctx, m_Algorithm);
            SimdHashUpdate(&ctx, words.GetLengths(), words.ConstBuffers());
            SimdHashFinalize(&ctx);
            SimdHashGetHashes(&ctx, &hashes[0]);

            // Perform reduce
            for (size_t h = 0; h < SimdLanes(); h++)
            {
                const uint8_t* hash = &hashes[h * hashWidth];
                const auto length = reducer->Reduce((char*)words[h], m_Max, hash, i);
                words.SetLength(h, length);
            }
        }

        // Save the chain information
        
        for (size_t h = 0; h < SimdLanes(); h++)
        {
            block[iteration * SimdLanes() + h].Set(words[h], words.GetLength(h));
        }
    }

    const auto end = std::chrono::system_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    //
    // Post a task to the main thread
    // to save this block
    //
    dispatch::PostTaskToDispatcher(
        "main",
        dispatch::bind(
            &RainbowTable::SaveBlock,
            this,
            ThreadId,
            BlockId,
            std::move(block),
            elapsed_ms.count()
        )
    );

    //
    // Post the next task
    //
    const size_t nextblock = BlockId + m_Threads;
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
    const std::vector<SmallString>& Block
)
{
    // Create a byte buffer so we can do it on one shot
    size_t bufferSize = m_ChainWidth * m_Blocksize;
    std::vector<uint8_t> buffer(bufferSize);
    uint8_t* bufferptr = &buffer[0];
    size_t index = BlockId;
    // Loop through the chains and add them to the buffer
    for (auto& endpoint : Block)
    {
        if (m_TableType == TypeUncompressed)
        {
            *((rowindex_t*)bufferptr) = index++;
            bufferptr += sizeof(rowindex_t);
        }
        memcpy(bufferptr, endpoint.Value, endpoint.Length);
        bufferptr += m_Max;
    }
    // Perform the write in a single shot and flush
    fwrite(&buffer[0], bufferSize, sizeof(uint8_t), m_WriteHandle);
    fflush(m_WriteHandle);
    m_ChainsWritten += Block.size();
}

char
DoubleMultipleChar(
    double& Value
)
{
    if (Value > 1000000000.f)
    {
        Value /= 1000000000.f;
        return 'b';
    }
    else if (Value > 1000000.f)
    {
        Value /= 1000000.f;
        return 'm';
    }
    else if (Value > 1000.f)
    {
        Value /= 1000.f;
        return 'k';
    }
    return ' ';
}

void
RainbowTable::OutputStatus(
    const SmallString& LastEndpoint
) const
{
    assert(dispatch::CurrentDispatcher() == dispatch::GetDispatcher("main").get());

    uint64_t averageMs = 0;
    for (auto const& [thread, val] : m_ThreadTimers)
    {
        averageMs += val;
    }
    averageMs /= m_Threads;

    double chainsPerSec = 1000.f * m_Blocksize / averageMs;
    double hashesPerSec = chainsPerSec * m_Length;

    char cpsChar = DoubleMultipleChar(chainsPerSec);
    char hpsChar = DoubleMultipleChar(hashesPerSec);

    double chains = (double)(m_StartingChains + m_ChainsWritten);
    char chainsChar = DoubleMultipleChar(chains);

    double percent = ((double)(m_StartingChains + m_ChainsWritten) / (double)m_Count) * 100.f;

    char statusbuf[72];
    statusbuf[sizeof(statusbuf) - 1] = '\0';
    memset(statusbuf, '\b', sizeof(statusbuf) - 1);
    fprintf(stderr, "%s", statusbuf);
    memset(statusbuf, ' ', sizeof(statusbuf) - 1);
    int count = snprintf(
        statusbuf, sizeof(statusbuf),
        "C:%.1lf%c(%.1f%%) C/s: %.1lf%c H/s:%.1lf%c E:\"%s\"",
            chains,
            chainsChar,
            percent,
            chainsPerSec,
            cpsChar,
            hashesPerSec,
            hpsChar,
            (char*)LastEndpoint.Value
    );
    if (count < sizeof(statusbuf) - 1)
    {
        statusbuf[count] = ' ';
    }
    fprintf(stderr, "%s", statusbuf);
}

void
RainbowTable::SaveBlock(
    const size_t ThreadId,
    const size_t BlockId,
    const std::vector<SmallString> Block,
    const uint64_t Time
)
{
    m_ThreadTimers[ThreadId] = Time;

    OutputStatus(Block[0]);

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

float
RainbowTable::GetCoverage(
    void
)
{
    mpz_class lowerbound = WordGenerator::WordLengthIndex(m_Min, m_Charset);
    mpz_class upperbound = WordGenerator::WordLengthIndex(m_Max + 1, m_Charset);
    mpf_class delta = upperbound - lowerbound;

    mpf_class count = m_Chains * m_Length;
    mpf_class percentage = (count / delta) * 100;

    return percentage.get_d();
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

    if (!GetTableHeader(m_Path, &hdr))
    {
        std::cerr << "Error reading table header" << std::endl;
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
        std::cout << "Chains written: " << m_ChainsWritten << std::endl;
        // Stop the pool
        if (m_DispatchPool != nullptr)
        {
            m_DispatchPool->Stop();
            m_DispatchPool->Wait();
        }

        // Stop the current (main) dispatcher
        dispatch::CurrentDispatcher()->Stop();
    }
}

bool
RainbowTable::UnmapTable(
    void
)
{
    int result = 0;
    if (m_MappedTable != nullptr)
    {
        result = munmap(m_MappedTable, m_MappedTableSize);
        m_MappedTable = nullptr;
        m_MappedTableSize = 0;
        if (result != 0)
        {
            std::cerr << "Unable to unmap table" << std::endl;
        }
    }

    if (m_MappedTableFd != nullptr)
    {
        fclose(m_MappedTableFd);
        m_MappedTableFd = nullptr;
    }

    return result == 0;
}

bool
RainbowTable::MapTable(
    const bool ReadOnly
)
{
    // Check if it is already mapped
    if (TableMapped())
    {
        if (m_MappedReadOnly == ReadOnly)
        {
            return true;
        }
        // Unmap it to remap writable
        if (!UnmapTable())
        {
            std::cerr << "Unmapping table failed" << std::endl;
            return false;
        }
    }

    m_MappedFileSize = std::filesystem::file_size(m_Path);
    m_MappedTableSize = m_MappedFileSize - sizeof(TableHeader);
    if (ReadOnly)
    {
        m_MappedTableFd = fopen(m_Path.c_str(), "r");
    }
    else
    {
        m_MappedTableFd = fopen(m_Path.c_str(), "r+");
    }
    if (m_MappedTableFd == nullptr)
    {
        std::cerr << "Unable to open a handle to the table file" << std::endl;
        return false;
    }

    int flags = 0;
    int prot = PROT_READ;
    if (ReadOnly)
    {
        flags = MAP_PRIVATE;
    }
    else
    {
        flags = MAP_SHARED;
        prot |= PROT_WRITE;
    }
    
    m_MappedTable = (uint8_t*)mmap(nullptr, m_MappedFileSize, prot, flags, fileno(m_MappedTableFd), 0);
    if (m_MappedTable == MAP_FAILED)
    {
        std::cerr << "Unable to map table into memory: " << strerror(errno) << std::endl;
        return false;
    }
    auto ret = madvise(m_MappedTable, m_MappedFileSize, MADV_RANDOM | MADV_WILLNEED);
    if (ret != 0)
    {
        std::cerr << "Madvise not happy" << std::endl;
    }

    return true;
}

const uint8_t*
RainbowTable::GetEndpointAt(
    const size_t Index
) const
{
    // Get the pointer to the chain
    const uint8_t* entry = m_MappedTable + sizeof(TableHeader) + (Index * GetChainWidth());
    // Return the pointer to the endpoint
    return m_TableType == TypeUncompressed ? entry + sizeof(rowindex_t) : entry;
}

void
RainbowTable::IndexTable(
    void
)
{
    assert(TableMapped());
    assert(GetCount() > 0);

    // Zero the lengths
    memset(m_MappedTableLookupSize, 0, sizeof(m_MappedTableLookupSize));
    // Zero the pointers
    memset(m_MappedTableLookup, 0, sizeof(m_MappedTableLookup));

    const size_t chainWidth = GetChainWidth();
    const uint8_t* base = GetEndpointAt(0);
    const uint8_t* endpoint = base;
    
    // Save the first endpoint
    uint16_t last = *(uint16_t*)endpoint;
    m_MappedTableLookup[last] = endpoint;
    // size_t count = 0;

    constexpr size_t READAHEAD = 64;

    // First pass
    for (size_t i = 0; i < GetCount(); i+= READAHEAD)
    {
        const uint16_t index = *(uint16_t*)endpoint;
        if (index != last)
        {
            m_MappedTableLookup[index] = endpoint;
        }
        last = index;
        endpoint += (chainWidth * READAHEAD);
    }

    // Loop over each known endpoint
    for (size_t i = 0; i < LOOKUP_SIZE; i++)
    {
        const uint8_t* offset = m_MappedTableLookup[i];
        if (offset == nullptr)
        {
            continue;
        }

        // Check if it is the base
        if (offset == base)
        {
            continue;
        }

        // Walk backwards until we find the previous
        const uint16_t index = *(uint16_t*)offset;
        for (;;)
        {
            offset -= chainWidth;
            const uint16_t next = *(uint16_t*)offset;
            if (next != index)
            {
                break;
            }
            m_MappedTableLookup[i] -= chainWidth;
        }
    }

    // Calculate the counts
    // We walk through each item, look for the next offset
    // and calculate the distance between them
    const uint8_t* max = 0;
    uint16_t maxIndex = 0;
    for (size_t i = 0; i < LOOKUP_SIZE; i++)
    {
        const uint8_t* offset = m_MappedTableLookup[i];
        // Find the next closest offset
        const uint8_t* next = (const uint8_t*)-1;
        for (size_t j = 0; j < LOOKUP_SIZE; j++)
        {
            if (j == i)
            {
                continue;
            }

            const uint8_t* test = m_MappedTableLookup[j];
            if (test > offset && test < next)
            {
                next = test;
            }
        }
        // Calculate the distance
        assert(next > offset);
        
        if(next != (const uint8_t*)-1)
        {
            m_MappedTableLookupSize[i] = (next - offset) / GetMax();

            // Update the max so we can calculate the last entry
            if (offset > max)
            {
                max = offset;
                maxIndex = i;
            }
        }
    }

    // Calculate final size
    const uint8_t* end = m_MappedTable + m_MappedFileSize - GetMax(); //GetEndpointAt(GetCount() - 1);
    m_MappedTableLookupSize[maxIndex] = (end - max) / GetMax();

    // Linear version
#if 0
    for (size_t i = 0; i < GetCount(); i++)
    {
        const uint16_t index = *(uint16_t*)endpoint;
        // New id, we need to walk back
        if (index != last)
        {
            m_MappedTableLookupSize[last] = count;
            m_MappedTableLookup[index] = endpoint;
            count = 1;
        }
        else
        {
            count++;
        }
        last = index;
        endpoint += chainWidth;
    }
    m_MappedTableLookupSize[last] = count;
#endif
}

/* static */ void
RainbowTable::DoHash(
    const uint8_t* Data,
    const size_t Length,
    uint8_t* Digest,
    const HashAlgorithm Algorithm
)
{
    switch (Algorithm)
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

/* static */ std::string
RainbowTable::DoHashHex(
    const uint8_t* Data,
    const size_t Length,
    const HashAlgorithm Algorithm
)
{
    uint8_t buffer[MAX_BUFFER_SIZE];
    switch (Algorithm)
    {
    case HashMd5:
        MD5(Data, Length, buffer);
        return Util::ToHex(buffer, MD5_SIZE);
        break;
    case HashSha1:
        SHA1(Data, Length, buffer);
        return Util::ToHex(buffer, SHA1_SIZE);
        break;
    case HashSha256:
        SHA256(Data, Length, buffer);
        return Util::ToHex(buffer, SHA256_SIZE);
        break;
    default:
        return std::string();
        break;
    }
}

const size_t
RainbowTable::FindEndpoint(
    const char* Endpoint,
    const size_t Length
) const
{
    std::vector<char> comparitor;
    comparitor.resize(m_Max);
    memcpy(&comparitor[0], Endpoint, Length);
    size_t skiplen = GetChainWidth();
    // Uncompressed tables are just flat files
    // of endpoints, each of m_Max width. They are
    // unsorted so we need to do a Linear search
    if (m_TableType == TypeCompressed)
    {
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
    // Uncompressed files are flat binary files of an
    // unsigned integer index, followed by the text
    // endpoint of m_Max width. They are sortd by endpoint
    // so we can do a binary search
    else
    {
        // Lookup this endpoint offset and length
        uint16_t index = *(uint16_t*)Endpoint;
        const uint8_t* tableStart = m_MappedTableLookup[index];
        const size_t tablelength = m_MappedTableLookupSize[index];

        // Endpoint not found in lookup table
        if (tableStart == 0)
        {
            return (size_t)-1;
        }

        // Perform the search
        size_t low = 0;
        size_t high = tablelength - 1;
        // uint8_t* startendpoint = m_MappedTable + sizeof(TableHeader) + sizeof(rowindex_t);
        while (low <= high)
        {
            size_t mid = (low + high) / 2;
            if (mid > tablelength)
                break;
            const uint8_t* endpoint = tableStart + (skiplen * mid);
            int cmp = memcmp(endpoint, &comparitor[0], m_Max);
            if (cmp == 0)
            {
                rowindex_t* index = (rowindex_t*)endpoint - 1;
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

/* static */ std::unique_ptr<Reducer>
RainbowTable::GetReducer(
    const size_t Min,
    const size_t Max,
    const size_t HashWidth,
    const std::string& Charset
)
{
    // For tables where the min and max are the same
    // (we are reducing to a constant length), we can
    // use the significantly faster BytewiseReducer
    if (Min == Max)
    {
        return std::make_unique<BytewiseReducer>(Min, Max, HashWidth, Charset);
    }
    // Otherwise we need to fall back to the much slower
    // modulo reducer which requires big integer division
    return std::make_unique<ModuloReducer>(Min, Max, HashWidth, Charset);
}

std::optional<std::string>
RainbowTable::CrackOne(
    std::string& Hash
)
{
    if (Hash.size() != m_HashWidth * 2)
    {
        std::cerr << "Invalid length of provided hash: " << Hash.size() << " != " << m_HashWidth * 2 << std::endl;
        std::cerr << "Hash: '" << Hash << "'" << std::endl;
        return std::nullopt;
    }

    auto target = Util::ParseHex(Hash);

    std::vector<uint8_t> hash(m_HashWidth);
    std::vector<char> reduced(m_Max);
    auto reducer = GetReducer();
    size_t length;

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
                return match;
            }
            else
            {
                m_FalsePositives++;
            }
        }
    }
    return std::nullopt;
}

void
RainbowTable::ResultFound(
    const std::string Hash,
    const std::string Result
)
{
    std::cout << Hash << " " << Result << std::endl;
}

//std::vector<std::tuple<std::string, std::string>>
void
RainbowTable::CrackSimd(
    std::vector<std::string> Hashes
)
{
    size_t lanes = Hashes.size();
    std::vector<std::vector<uint8_t>> hashbytes;
    auto reducer = GetReducer();
    size_t cracked = 0;
    SimdHashBufferFixed<MAX_OPTIMIZED_BUFFER_SIZE> words;
    std::array<uint8_t, MAX_HASH_SIZE * MAX_LANES> hashes;
    const size_t hashWidth = m_HashWidth;

    // Parse the hex strings into byte arrays
    for (size_t i = 0; i < lanes; i++)
    {
        hashbytes.push_back(
            Util::ParseHex(Hashes[i])
        );
    }

    for (ssize_t i = m_Length - 1; i >= 0; i--)
    {
        // Copy the hashes into the Simd buffers
        for (size_t j = 0; j < lanes; j++)
        {
            memcpy(&hashes[j * hashWidth], hashbytes[j].data(), hashbytes[j].size());
        }

        // Perform the full chain from the next hop
        for (size_t j = i; j < m_Length - 1; j++)
        {
            // Perform the reductions
            for (size_t h = 0; h < lanes; h++)
            {
                const uint8_t* hash = &hashes[h * hashWidth];
                const size_t length = reducer->Reduce((char*)words[h], m_Max, hash, j);
                assert(length != (size_t)-1 && length >= m_Min && length <= m_Max);
                if (length < m_Max)
                {
                    std::cout << length << ' ' << std::string((char*)words[h], length);
                }
                words.SetLength(h, length);
            }
            
            // Perform hash
            SimdHashContext ctx;
            SimdHashInit(&ctx, m_Algorithm);
            SimdHashUpdate(&ctx, words.GetLengths(), words.ConstBuffers());
            SimdHashFinalize(&ctx);
            SimdHashGetHashes(&ctx, &hashes[0]);
        }

        // Perform the final reductions and check
        for (size_t h = 0; h < lanes; h++)
        {
            const uint8_t* hash = &hashes[h * hashWidth];
            const size_t length = reducer->Reduce((char*)words[h], m_Max, hash, m_Length - 1);
            assert(length != (size_t)-1 && length >= m_Min && length <= m_Max);
            // Check end, if it matches, we can perform one full chain to see if we find it
            size_t index = FindEndpoint((char*)words[h], length);
            if (index != (size_t)-1)
            {
                auto match = ValidateChain(index, hashbytes[h].data());
                if (match.has_value())
                {
                    dispatch::PostTaskToDispatcher(
                        "main",
                        dispatch::bind(
                            &RainbowTable::ResultFound,
                            this,
                            Hashes[h],
                            match.value()
                        )
                    );
                    // Track that we cracked one
                    // If we have cracked all lanes, we can quit early
                    if (++cracked == SimdLanes())
                    {
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
}

void
RainbowTable::CrackWorker(
    const size_t ThreadId
)
{
    bool exhausted = false;
    while (!exhausted)
    {
        // Read the next line from the input file
        std::vector<std::string> next;
        {
            std::lock_guard<std::mutex> lock(m_HashFileStreamLock);
            for (size_t i = 0; i < SimdLanes(); i++)
            {
                std::string line;
                if(!std::getline(m_HashFileStream, line))
                {
                    exhausted = true;
                    break;
                }
                next.push_back(std::move(line));
            }
        }

        if (exhausted)
        {
            break;
        }

        CrackSimd(
            std::move(next)
        );
    }

    // We dropped out, post that we are done
    dispatch::PostTaskToDispatcher(
        "main",
        dispatch::bind(
            &RainbowTable::ThreadCompleted,
            this,
            ThreadId
        )
    );
}

void
RainbowTable::CrackOneWorker(
    const size_t ThreadId
)
{
    bool exhausted = false;
    while (!exhausted)
    {
        // Read the next line from the input file
        std::string next;
        {
            std::lock_guard<std::mutex> lock(m_HashFileStreamLock);
            if(!std::getline(m_HashFileStream, next))
            {
                exhausted = true;
                break;
            }
        }

        auto result = CrackOne(
            next
        );

        if (result.has_value())
        {
            dispatch::PostTaskToDispatcher(
                "main",
                dispatch::bind(
                    &RainbowTable::ResultFound,
                    this,
                    next,
                    result.value()
                )
            );
        }
    }

    // We dropped out, post that we are done
    dispatch::PostTaskToDispatcher(
        "main",
        dispatch::bind(
            &RainbowTable::ThreadCompleted,
            this,
            ThreadId
        )
    );
}

void
RainbowTable::Crack(
    std::string& Target
)
{
    // Mmap the table
    if (!MapTable(true))
    {
        std::cerr << "Error mapping the table" << std::endl;
        return;
    }

    // Index the table
    std::cerr << "Indexing table..";
    IndexTable();
    std::cerr << " done." << std::endl;

    // Figure out if this is a single hash
    if (Util::IsHex(Target))
    {
        auto result = CrackOne(Target);
        if (result)
        {
            std::cout << Target << ' ' << result.value() << std::endl;
        }

        // Stop the main dispatcher
        dispatch::CurrentDispatcher()->Stop();
    }
    // Check if it is a file
    else if (std::filesystem::exists(Target))
    {
        // Open the input file handle
        m_HashFileStream = std::ifstream(Target);

        if (m_Threads == 0)
        {
            m_Threads = std::thread::hardware_concurrency();
        }

        // Create the dispatchers
        m_DispatchPool = dispatch::CreateDispatchPool("pool", m_Threads);
        
        // Loop through and start the cracking jobs
        for (size_t i = 0; i < m_Threads; i++)
        {
            m_DispatchPool->PostTask(
                dispatch::bind(
                    &RainbowTable::CrackWorker,
                    this,
                    i
                )
            );
        }
    }
    else
    {
        std::cerr << "Unrecognised target hash or file" << std::endl;
    }
}

std::optional<std::string>
RainbowTable::ValidateChain(
    const size_t ChainIndex,
    const uint8_t* Target
) const
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
    UnmapTable();

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

int
SortCompareEndpoints(
    const void* Comp1,
    const void* Comp2,
    void* Arguments
)
{
    return memcmp((uint8_t*)Comp1 + sizeof(rowindex_t), (uint8_t*)Comp2 + sizeof(rowindex_t), (size_t)Arguments);
}

int
SortCompareStartpoints(
    const void* Comp1,
    const void* Comp2
)
{
    return *((rowindex_t*)Comp1) - *((rowindex_t*)Comp2);
}

void
RainbowTable::SortStartpoints(
    void
)
{
    if (!MapTable(false))
    {
        std::cerr << "Error mapping table for sort"  << std::endl;
        return;
    }

    uint8_t* start = m_MappedTable + sizeof(TableHeader);
    if (m_TableType == TypeCompressed)
    {
        std::cerr << "Unable to sort compressed tables by start point" << std::endl;
        return;
    }

    qsort(start, GetCount(), GetChainWidth(), SortCompareStartpoints);
}

void
RainbowTable::SortTable(
    void
)
{
    if (!MapTable(false))
    {
        std::cerr << "Error mapping table for sort"  << std::endl;
        return;
    }

    uint8_t* start = m_MappedTable + sizeof(TableHeader);
    if (m_TableType == TypeUncompressed)
    {
        qsort_r(start, GetCount(), GetChainWidth(), SortCompareEndpoints, (void*)m_Max);
    }
    else
    {
        qsort(start, GetCount(), GetChainWidth(), SortCompareStartpoints);
    }
}

void
RainbowTable::RemoveStartpoints(
    void
)
{
    // Ensure the table is mapped writable
    if (!MapTable(false))
    {
        std::cerr << "Unable to map the table" << std::endl;
        return;
    }

    // Change the table type in the header
    (*(TableHeader*)m_MappedTable).type = TypeCompressed;

    // Loop through the chains
    uint8_t* tableBase = m_MappedTable + sizeof(TableHeader);
    uint8_t* writepointer = tableBase;
    for (size_t chain = 0; chain < GetCount(); chain++)
    {
        uint8_t* endpoint = tableBase + (chain * ChainWidthForType(TypeUncompressed, GetMax())) + sizeof(rowindex_t);
        memmove(writepointer, endpoint, GetMax());
        writepointer += GetMax();
    }

    // Truncate the file
    if (!UnmapTable())
    {
        std::cerr << "Error unmapping table after removing start points" << std::endl;
        return;
    }
    
    size_t newSize = sizeof(TableHeader) + (GetCount() * GetMax());
    auto result = truncate(m_Path.c_str(), newSize);
    if (result != 0)
    {
        std::cerr << "Error truncating file" << std::endl;
        return;
    }
}

void
RainbowTable::ChangeType(
    const std::filesystem::path& Destination,
    const TableType Type
)
{
    FILE* fhw;
    TableHeader hdr;

    if (m_TableType == Type)
    {
        std::cerr << "Won't convert to same type" << std::endl;
        return;
    }

    // Output some basic information about the
    // current table
    std::cout << "Table type: " << GetType() << std::endl;
    std::cout << "Chain width: " << GetChainWidth() << std::endl;
    std::cout << "Exporting " << m_Chains << " chains" << std::endl;

    // If we are converting to a compressed file we need to do
    // it in two stages. We need a full copy of the file so that
    // we can then sort it by start points. Then we remove all 
    // startpoints from the file
    if (Type == TypeCompressed)
    {
        if (!std::filesystem::copy_file(m_Path, Destination, std::filesystem::copy_options::overwrite_existing))
        {
            std::cerr << "Error copying file for conversion" << std::endl;
            return;
        }
    }
    else
    {
        // Map the current table to memory
        if (!MapTable(true))
        {
            std::cerr << "Error mapping table"  << std::endl;
            return;
        }

        // Copy the existing header but change the type to uncompressed
        hdr = *((TableHeader*)m_MappedTable);
        hdr.type = TypeUncompressed;

        // Open the destination for writing
        fhw = fopen(Destination.c_str(), "w");
        if (fhw == nullptr)
        {
            std::cerr << "Error opening desination table for write: " << Destination << std::endl;
            return;
        }

        // Write the header
        fwrite(&hdr, sizeof(hdr), 1, fhw);

        // Pointer to track the next read target
        uint8_t* next = m_MappedTable + sizeof(TableHeader);

        // Loop through and write each index followed by the next endpoint
        for (rowindex_t index = 0; index < m_Chains; index++, next += GetChainWidth())
        {
            fwrite(&index, sizeof(rowindex_t), 1, fhw);
            fwrite(next, sizeof(char), m_Max, fhw);
        }
        fclose(fhw);
    }

    // Perform sort and cleanup work on the new table    
    RainbowTable newtable;
    newtable.SetPath(Destination);

    if (!newtable.ValidTable())
    {
        std::cerr << "Decompressed table does not seem valid" << std::endl;
        return;
    }
    
    if (!newtable.LoadTable())
    {
        std::cerr << "Error loading new table" << std::endl;
        return;
    }

    std::cout << "Sorting " << newtable.GetCount() << " chains" << std::endl;

    if (m_TableType == TypeCompressed)
    {
        newtable.SortTable();
    }
    else
    {
        // Sort the table by startpoint
        newtable.SortStartpoints();
        // Remove all startpoints and truncate
        newtable.RemoveStartpoints();
    }
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

    rowindex_t start = Index;
    if (hdr.type == (uint8_t)TypeUncompressed)
    {
        fread(&start, sizeof(rowindex_t), 1, fh);
        
    }
    mpz_class lowerbound = WordGenerator::WordLengthIndex(hdr.min, charset);
    auto word = WordGenerator::GenerateWord(lowerbound + start, charset);
    chain.SetStart(word);

    std::string endpoint;
    endpoint.resize(hdr.max);
    fread(&endpoint[0], sizeof(char), hdr.max, fh);
    // Trim nulls
    endpoint.resize(strlen(endpoint.c_str()));
    chain.SetEnd(endpoint);

    return chain;
}

/* static */ const Chain
RainbowTable::ComputeChain(
    const size_t Index,
    const size_t Min,
    const size_t Max,
    const size_t Length,
    const HashAlgorithm Algorithm,
    const std::string& Charset
)
{
    mpz_class counter;
    Chain chain;
    size_t hashLength;
    std::string start;
    
    hashLength = GetHashWidth(Algorithm);

    chain.SetIndex(Index);
    chain.SetLength(Length);

    counter = WordGenerator::WordLengthIndex(Min, Charset);
    counter += Index;

    start = WordGenerator::GenerateWord(counter, Charset);
    chain.SetStart(start);

    auto reducer = GetReducer(Min, Max, hashLength, Charset);

    std::vector<uint8_t> hash(hashLength);
    std::vector<char> reduced(Max);
    size_t reducedLength = start.size();

    memcpy(&reduced[0], start.c_str(), reducedLength);
    
    for (size_t i = 0; i < Length; i++)
    {
        DoHash((uint8_t*)&reduced[0], reducedLength, &hash[0], Algorithm);
        reducedLength = reducer->Reduce(&reduced[0], Max, &hash[0], i);
    }

    chain.SetEnd(std::string(&reduced[0], &reduced[reducedLength]));
    return chain;
}