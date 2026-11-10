// NFTManager.h - owns the central record vector and the two independent indexes.
// NOT thread-safe by itself: the HTTP server wraps it in a shared_mutex.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "HashTable.h"
#include "NFTRecord.h"
#include "SortedArray.h"

class NFTManager {
public:
    // ---- dataset ---------------------------------------------------------
    // Deterministic synthetic data. Token IDs ascend with small random gaps,
    // owners come from a pool of wallet-like names, every NFT has between 1
    // and maxHistory transactions (the first is the mint).
    void generate(size_t count, uint64_t seed, int maxHistory);

    // CSV persistence: <dir>/nfts.csv and <dir>/transactions.csv.
    bool loadCsv(const std::string& dir, std::string& error);
    bool saveCsv(const std::string& dir, std::string& error) const;
    void setDataDir(const std::string& dir) { dataDir_ = dir; }

    // ---- indexes (independent of each other) -----------------------------
    bool buildHashIndex();      // false on duplicate token id
    bool buildSortedIndex();
    bool rebuildIndexes();

    bool hashLookup(int64_t tokenId, size_t& recordIndex, size_t& comparisons) const {
        return hash_.find(tokenId, recordIndex, comparisons);
    }
    bool binaryLookup(int64_t tokenId, size_t& recordIndex, size_t& comparisons) const {
        return sorted_.find(tokenId, recordIndex, comparisons);
    }

    // ---- ownership transfer ----------------------------------------------
    // Updates via the hash index, then cross-checks that the sorted index
    // resolves to the same record. Appends one line to transactions.csv when a
    // data dir is set (persisted tells whether that write succeeded).
    bool transfer(int64_t tokenId, const std::string& newOwner, std::string& error,
                  Transaction& created, bool& persisted);

    // ---- accessors -------------------------------------------------------
    const NFTRecord& record(size_t i) const { return records_[i]; }
    size_t totalNFTs() const { return records_.size(); }
    size_t totalTransactions() const { return totalTransactions_; }
    size_t totalTransfers() const { return totalTransactions_ - records_.size(); }  // minus mints
    size_t uniqueOwners() const;  // cached; recomputed after a transfer
    const HashTable& hashTable() const { return hash_; }
    const SortedArray& sortedArray() const { return sorted_; }
    const std::string& dataDir() const { return dataDir_; }

private:
    std::vector<NFTRecord> records_;
    HashTable hash_;
    SortedArray sorted_;
    size_t totalTransactions_ = 0;
    std::string dataDir_;
    mutable size_t uniqueOwnersCache_ = 0;
    mutable bool uniqueOwnersDirty_ = true;
};
