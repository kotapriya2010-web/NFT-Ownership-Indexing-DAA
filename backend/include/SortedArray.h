// SortedArray.h - index kept in ascending tokenId order, searched with a
// hand-written binary search. std::sort / std::binary_search / std::map are
// not used.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "NFTRecord.h"

struct SortedEntry {
    int64_t tokenId;
    size_t recordIndex;  // position in the central NFT vector
};

class SortedArray {
public:
    // Build from the central vector. Uses a manual bottom-up merge sort
    // (O(n log n) time, O(n) extra space); skipped if already sorted (O(n)).
    void buildFromRecords(const std::vector<NFTRecord>& records);

    // Insert one entry, keeping order. Binary search finds the slot in
    // O(log n) but shifting the tail is O(n): this is the price of the
    // sorted-array layout. Returns false on duplicate.
    bool insert(int64_t tokenId, size_t recordIndex);

    // Manual binary search. comparisons = number of probes (one per midpoint
    // examined). Time O(log n), space O(1).
    bool find(int64_t tokenId, size_t& recordIndex, size_t& comparisons) const;

    // Ownership update via this index: binary search, then appendTransfer().
    // Keys never change, so the order is never disturbed.
    bool updateOwner(std::vector<NFTRecord>& records, int64_t tokenId,
                     const std::string& newOwner, int64_t timestamp, size_t& comparisons);

    size_t size() const { return data_.size(); }
    bool isSorted() const;  // O(n) self-check
    size_t approxMemoryBytes() const { return data_.capacity() * sizeof(SortedEntry); }
    void clear() { data_.clear(); }

private:
    void mergeSort();
    std::vector<SortedEntry> data_;
};
