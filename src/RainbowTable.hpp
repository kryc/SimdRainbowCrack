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
#include <map>
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

class RainbowTable
{
public:
    ~RainbowTable(void);
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
    void SetCharset(const std::string Charset) { m_Charset = Charset; };
    std::string GetCharset(void) const { return m_Charset; };
    void SetType(const std::string Type);
    std::string GetType(void) const { return m_TableType == TypeCompressed ? "Compressed" : "Uncompressed";  };
    bool TableExists(void) const { return std::filesystem::exists(m_Path); };
    bool IsTableFile(void) const;
    bool ValidTable(void) const { return TableExists() && IsTableFile(); };
    bool LoadTable(void);
    bool Complete(void) const { return m_ThreadsCompleted == m_Threads; };
    void Crack(std::string& Hash);
    const size_t ChainWidthForType(const TableType Type) const { return Type == TypeCompressed ? m_Max : sizeof(uint64_t) + m_Max; };
    const size_t GetChainWidth(void) const { return ChainWidthForType(m_TableType); };
    void DoHash(const uint8_t* Data, const size_t Length, uint8_t* Digest);
private:
    void StoreTableHeader(void) const;
    void GenerateBlock(const size_t ThreadId, const size_t BlockId);
    void SaveBlock(const size_t BlockId, const std::vector<Chain> Block);
    void WriteBlock(const size_t BlockId, const ChainBlock& Block);
    void ThreadCompleted(const size_t ThreadId);
    const size_t FindEndpoint(const char* Endpoint, const size_t Length);
    std::optional<std::string> ValidateChain(const size_t ChainIndex, const uint8_t* Hash);
    bool MapTable(const bool ReadOnly = true);
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
    size_t m_HashWidth;
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
    // For cracking
    uint8_t* m_MappedTable = nullptr;
    FILE* m_MappedTableFd = nullptr;
    size_t m_MappedTableSize;
    size_t m_FalsePositives = 0;
};

#endif /* RainbowTable_hpp */
