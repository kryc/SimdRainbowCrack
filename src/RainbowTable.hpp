//
//  RainbowTable.hpp
//  SimdCrack
//
//  Created by Kryc on 15/02/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#ifndef RainbowTable_hpp
#define RainbowTable_hpp

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "DispatchQueue.hpp"
#include "simdhash.h"

#include "Chain.hpp"
#include "Reduce.hpp"


typedef enum _TableType
{
    TypeUncompressed,
    TypeCompressed,
    TypeInvalid
} TableType;

constexpr uint32_t kMagic = 'rt- ';

typedef struct  __attribute__((__packed__)) _TableHeader
{
    uint32_t magic;
    uint8_t  type:2;
    uint8_t  algorithm:6;
    uint8_t  min;
    uint8_t  max;
    uint8_t  charsetlen;
    uint64_t length;
    char     charset[128];
} TableHeader;

typedef uint32_t rowindex_t;

class RainbowTable
{
public:
    ~RainbowTable(void);
    void Reset(void);
    void InitAndRunBuild(void);
    bool ValidateConfig(void);
    void SetPath(std::filesystem::path Path) { m_Path = Path; };
    std::filesystem::path GetPath(void) const { return m_Path; };
    void SetAlgorithm(const std::string& Algorithm) { m_Algorithm = ParseHashAlgorithm(Algorithm.c_str()); };
    const std::string GetAlgorithmString(void) const { return HashAlgorithmToString(m_Algorithm); };
    const HashAlgorithm GetAlgorithm(void) const { return m_Algorithm; };
    void SetMin(const size_t Min) { m_Min = Min; };
    const size_t GetMin(void) const { return m_Min; };
    void SetMax(const size_t Max) { m_Max = Max; };
    const size_t GetMax(void) const { return m_Max; };
    void SetLength(const size_t Length) { m_Length = Length; };
    const size_t GetLength(void) const { return m_Length; };
    void SetBlocksize(const size_t Blocksize) { m_Blocksize = Blocksize; };
    void SetCount(const size_t Count) { m_Count = Count; };
    const size_t GetCount(void) const;
    void SetThreads(const size_t Threads) { m_Threads = Threads; };
    void SetCharset(const std::string Charset) { m_Charset = ParseCharset(Charset); };
    const std::string& GetCharset(void) const { return m_Charset; };
    void SetType(const TableType Type) { m_TableType = Type; };
    bool SetType(const std::string Type);
    std::string GetType(void) const { return m_TableType == TypeCompressed ? "Compressed" : "Uncompressed";  };
    bool TableExists(void) const { return std::filesystem::exists(m_Path); };
    static bool GetTableHeader(const std::filesystem::path& Path, TableHeader* Header);
    static bool IsTableFile(const std::filesystem::path& Path);
    bool IsTableFile(void) const { return IsTableFile(m_Path); };
    bool ValidTable(void) const { return TableExists() && IsTableFile(m_Path); };
    bool LoadTable(void);
    bool Complete(void) const { return m_ThreadsCompleted == m_Threads; };
    void Crack(std::string& Target);
    static const size_t ChainWidthForType(const TableType Type, const size_t Max) { return Type == TypeCompressed ? Max : sizeof(rowindex_t) + Max; };
    const size_t GetChainWidth(void) const { return ChainWidthForType(m_TableType, m_Max); };
    static void DoHash(const uint8_t* Data, const size_t Length, uint8_t* Digest, const HashAlgorithm);
    void DoHash(const uint8_t* Data, const size_t Length, uint8_t* Digest) const { DoHash(Data, Length, Digest, m_Algorithm); };
    void Decompress(const std::filesystem::path& Destination) { ChangeType(Destination, TypeUncompressed); };
    void Compress(const std::filesystem::path& Destination) { ChangeType(Destination, TypeCompressed); };
    void SortTable(void);
    static const Chain GetChain(const std::filesystem::path& Path, const size_t Index);
    static const Chain ComputeChain(const size_t Index, const size_t Min, const size_t Max, const size_t Length, const HashAlgorithm Algorithm, const std::string& Charset);
    static std::unique_ptr<Reducer> GetReducer(const size_t Min, const size_t Max, const size_t HashWidth, const std::string& Charset);
protected:
    void SortStartpoints(void);
    void RemoveStartpoints(void);
private:
    void ChangeType(const std::filesystem::path& Destination, const TableType Type);
    void StoreTableHeader(void) const;
    void GenerateBlock(const size_t ThreadId, const size_t BlockId);
    void SaveBlock(const size_t ThreadId, const size_t BlockId, const std::vector<Chain> Block, const uint64_t Time);
    void OutputStatus(const Chain& LastChain, const size_t BlockSize) const;
    void WriteBlock(const size_t BlockId, const ChainBlock& Block);
    void ThreadCompleted(const size_t ThreadId);
    std::optional<std::string> CrackOne(std::string& Target);
    const size_t FindEndpoint(const char* Endpoint, const size_t Length) const;
    std::optional<std::string> ValidateChain(const size_t ChainIndex, const uint8_t* Hash) const;
    bool TableMapped(void) { return m_MappedTableFd != nullptr; };
    bool MapTable(const bool ReadOnly = true);
    bool UnmapTable(void);
    std::unique_ptr<Reducer> GetReducer(void) const { return GetReducer(m_Min, m_Max, m_HashWidth, m_Charset); };
    void CrackWorker(const size_t ThreadId);
    /*std::vector<std::tuple<std::string, std::string>>*/ void CrackSimd(std::vector<std::string> Hashes);
    void ResultFound(const std::string Hash, const std::string Result);
    // General purpose
    std::filesystem::path m_Path;
    bool m_PathLoaded = false;
    HashAlgorithm m_Algorithm = HashUnknown;
    size_t m_Min = 0;
    size_t m_Max = 0;
    size_t m_Length = 0;
    size_t m_Blocksize = 1024;
    size_t m_Count = 0;
    size_t m_Threads = 0;
    std::string m_Charset;
    size_t m_HashWidth = 0;
    size_t m_ChainWidth = 0;
    size_t m_Chains = 0;
    TableType m_TableType = TypeCompressed;
    // For building
    size_t m_StartingChains = 0;
    FILE* m_WriteHandle = NULL;
    size_t m_NextWriteBlock = 0;
    std::map<size_t, ChainBlock> m_WriteCache;
    dispatch::DispatchPoolPtr m_DispatchPool;
    size_t m_ThreadsCompleted = 0;
    size_t m_ChainsWritten = 0;
    std::map<size_t, uint64_t> m_ThreadTimers;
    // For cracking
    uint8_t* m_MappedTable = nullptr;
    FILE* m_MappedTableFd = nullptr;
    size_t m_MappedFileSize;
    size_t m_MappedTableSize;
    size_t m_FalsePositives = 0;
    bool m_MappedReadOnly = false;
    std::ifstream m_HashFileStream;
    std::mutex m_HashFileStreamLock;
};

#endif /* RainbowTable_hpp */
