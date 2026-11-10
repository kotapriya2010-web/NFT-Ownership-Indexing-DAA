// HashTable.h - hand-written hash table, separate chaining, singly linked lists.
// No unordered_map / map is used anywhere.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "NFTRecord.h"

// One link in a bucket's chain.
struct HashNode {
    int64_t tokenId;
    size_t recordIndex;  // position in the central NFT vector
    HashNode* next;
};

class HashTable {
public:
    explicit HashTable(size_t bucketCount = 17);
    ~HashTable();
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    // Smallest prime >= expected / 0.75 (target load factor 0.75). A prime
    // modulus spreads the keys evenly even if the hash has weak low bits.
    static size_t suggestedBucketCount(size_t expectedElements);

    // Drop everything and start again with a new bucket count.
    void reset(size_t bucketCount);

    // Insert. Returns false if tokenId already exists.
    // Avg O(1 + alpha), worst O(n) (duplicate check walks the chain).
    // If the load factor would exceed 1.0 the table doubles (rehash, O(n)).
    bool insert(int64_t tokenId, size_t recordIndex);

    // Search. comparisons = number of chain nodes examined.
    // Avg O(1 + alpha), worst O(n) when every key lands in one bucket.
    bool find(int64_t tokenId, size_t& recordIndex, size_t& comparisons) const;

    // Ownership update through the hash index: find, then appendTransfer().
    // The hash key (tokenId) never changes, so the structure is untouched.
    // Time = one lookup.
    bool updateOwner(std::vector<NFTRecord>& records, int64_t tokenId,
                     const std::string& newOwner, int64_t timestamp, size_t& comparisons);

    // Statistics (all computed by hand).
    size_t bucketCount() const { return buckets_.size(); }
    size_t size() const { return size_; }
    size_t collisions() const { return collisions_; }   // inserts into a non-empty bucket
    double loadFactor() const;                          // size / buckets
    size_t chainLength(size_t bucket) const;            // O(chain)
    size_t longestChain() const;                        // O(m + n)
    size_t emptyBuckets() const;                        // O(m + n)
    size_t approxMemoryBytes() const;                   // O(1)

private:
    size_t bucketOf(int64_t tokenId) const;
    void rehash(size_t newBucketCount);
    void freeAll();

    std::vector<HashNode*> buckets_;
    size_t size_ = 0;
    size_t collisions_ = 0;
};
