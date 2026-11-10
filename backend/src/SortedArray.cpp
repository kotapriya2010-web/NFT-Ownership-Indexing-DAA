#include "SortedArray.h"

bool SortedArray::isSorted() const {
    for (size_t i = 1; i < data_.size(); ++i) {
        if (data_[i - 1].tokenId >= data_[i].tokenId) return false;  // strict: ids are unique
    }
    return true;
}

// Bottom-up merge sort on (tokenId, recordIndex) pairs.
// Time O(n log n), extra space O(n).
void SortedArray::mergeSort() {
    const size_t n = data_.size();
    std::vector<SortedEntry> tmp(n);
    for (size_t width = 1; width < n; width *= 2) {
        for (size_t lo = 0; lo < n; lo += 2 * width) {
            size_t mid = lo + width < n ? lo + width : n;
            size_t hi = lo + 2 * width < n ? lo + 2 * width : n;
            size_t i = lo, j = mid, k = lo;
            while (i < mid && j < hi) {
                if (data_[i].tokenId <= data_[j].tokenId) tmp[k++] = data_[i++];
                else tmp[k++] = data_[j++];
            }
            while (i < mid) tmp[k++] = data_[i++];
            while (j < hi) tmp[k++] = data_[j++];
        }
        data_.swap(tmp);
    }
}

void SortedArray::buildFromRecords(const std::vector<NFTRecord>& records) {
    data_.clear();
    data_.reserve(records.size());
    for (size_t i = 0; i < records.size(); ++i) {
        data_.push_back(SortedEntry{records[i].tokenId, i});
    }
    if (!isSorted()) mergeSort();
}

// Manual binary search over the half-open range [lo, hi).
bool SortedArray::find(int64_t tokenId, size_t& recordIndex, size_t& comparisons) const {
    comparisons = 0;
    size_t lo = 0, hi = data_.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;  // no overflow
        ++comparisons;
        int64_t midId = data_[mid].tokenId;
        if (midId == tokenId) {
            recordIndex = data_[mid].recordIndex;
            return true;
        }
        if (midId < tokenId) lo = mid + 1;
        else hi = mid;
    }
    return false;
}

bool SortedArray::insert(int64_t tokenId, size_t recordIndex) {
    // lower bound by hand: first position whose id >= tokenId
    size_t lo = 0, hi = data_.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (data_[mid].tokenId < tokenId) lo = mid + 1;
        else hi = mid;
    }
    if (lo < data_.size() && data_[lo].tokenId == tokenId) return false;

    // Shift the tail one slot right: O(n) in the worst case (insert at front).
    data_.push_back(SortedEntry{0, 0});
    for (size_t i = data_.size() - 1; i > lo; --i) data_[i] = data_[i - 1];
    data_[lo] = SortedEntry{tokenId, recordIndex};
    return true;
}

bool SortedArray::updateOwner(std::vector<NFTRecord>& records, int64_t tokenId,
                              const std::string& newOwner, int64_t timestamp, size_t& comparisons) {
    size_t idx = 0;
    if (!find(tokenId, idx, comparisons)) return false;
    appendTransfer(records[idx], newOwner, timestamp);
    return true;
}
