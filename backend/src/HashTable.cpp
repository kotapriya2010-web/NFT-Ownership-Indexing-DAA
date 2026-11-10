#include "HashTable.h"

namespace {

// Hash function: the 64-bit finaliser of MurmurHash3 ("fmix64").
// Sequential token IDs (1000, 1001, 1002 ...) would all land in neighbouring
// buckets with a plain "id % m". fmix64 is an "avalanche" mix: every input bit
// flips about half of the output bits, so consecutive IDs scatter uniformly.
// Cost: 2 multiplications, 3 shifts, 3 xors = O(1).
inline uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

bool isPrime(size_t n) {
    if (n < 2) return false;
    if (n % 2 == 0) return n == 2;
    for (size_t d = 3; d * d <= n; d += 2) {
        if (n % d == 0) return false;
    }
    return true;
}

}  // namespace

HashTable::HashTable(size_t bucketCount) : buckets_(bucketCount == 0 ? 1 : bucketCount, nullptr) {}

HashTable::~HashTable() { freeAll(); }

void HashTable::freeAll() {
    for (size_t i = 0; i < buckets_.size(); ++i) {
        HashNode* cur = buckets_[i];
        while (cur != nullptr) {
            HashNode* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
        buckets_[i] = nullptr;
    }
    size_ = 0;
    collisions_ = 0;
}

size_t HashTable::suggestedBucketCount(size_t expectedElements) {
    size_t target = static_cast<size_t>(static_cast<double>(expectedElements) / 0.75) + 1;
    if (target < 17) target = 17;
    while (!isPrime(target)) ++target;
    return target;
}

void HashTable::reset(size_t bucketCount) {
    freeAll();
    buckets_.assign(bucketCount == 0 ? 1 : bucketCount, nullptr);
}

size_t HashTable::bucketOf(int64_t tokenId) const {
    return static_cast<size_t>(mix64(static_cast<uint64_t>(tokenId)) % buckets_.size());
}

double HashTable::loadFactor() const {
    return buckets_.empty() ? 0.0 : static_cast<double>(size_) / static_cast<double>(buckets_.size());
}

bool HashTable::insert(int64_t tokenId, size_t recordIndex) {
    if (static_cast<double>(size_ + 1) / static_cast<double>(buckets_.size()) > 1.0) {
        rehash(buckets_.size() * 2 + 1);
    }
    size_t b = bucketOf(tokenId);

    // Duplicate check: walk the chain.
    for (HashNode* cur = buckets_[b]; cur != nullptr; cur = cur->next) {
        if (cur->tokenId == tokenId) return false;
    }

    // Collision = the target bucket already holds at least one element.
    if (buckets_[b] != nullptr) ++collisions_;

    // Insert at the head of the chain: O(1).
    HashNode* node = new HashNode{tokenId, recordIndex, buckets_[b]};
    buckets_[b] = node;
    ++size_;
    return true;
}

bool HashTable::find(int64_t tokenId, size_t& recordIndex, size_t& comparisons) const {
    comparisons = 0;
    for (HashNode* cur = buckets_[bucketOf(tokenId)]; cur != nullptr; cur = cur->next) {
        ++comparisons;
        if (cur->tokenId == tokenId) {
            recordIndex = cur->recordIndex;
            return true;
        }
    }
    return false;
}

bool HashTable::updateOwner(std::vector<NFTRecord>& records, int64_t tokenId,
                            const std::string& newOwner, int64_t timestamp, size_t& comparisons) {
    size_t idx = 0;
    if (!find(tokenId, idx, comparisons)) return false;
    appendTransfer(records[idx], newOwner, timestamp);
    return true;
}

// Re-distribute existing nodes into a bigger bucket array. O(m_old + n).
// Collisions are recounted because the layout changes.
void HashTable::rehash(size_t newBucketCount) {
    std::vector<HashNode*> old;
    old.swap(buckets_);
    buckets_.assign(newBucketCount, nullptr);
    collisions_ = 0;
    for (size_t i = 0; i < old.size(); ++i) {
        HashNode* cur = old[i];
        while (cur != nullptr) {
            HashNode* nxt = cur->next;
            size_t b = bucketOf(cur->tokenId);
            if (buckets_[b] != nullptr) ++collisions_;
            cur->next = buckets_[b];
            buckets_[b] = cur;
            cur = nxt;
        }
    }
}

size_t HashTable::chainLength(size_t bucket) const {
    size_t len = 0;
    for (HashNode* cur = buckets_[bucket]; cur != nullptr; cur = cur->next) ++len;
    return len;
}

size_t HashTable::longestChain() const {
    size_t best = 0;
    for (size_t i = 0; i < buckets_.size(); ++i) {
        size_t len = chainLength(i);
        if (len > best) best = len;
    }
    return best;
}

size_t HashTable::emptyBuckets() const {
    size_t n = 0;
    for (size_t i = 0; i < buckets_.size(); ++i) {
        if (buckets_[i] == nullptr) ++n;
    }
    return n;
}

size_t HashTable::approxMemoryBytes() const {
    // bucket array + nodes (rounded to 32 bytes each for allocator overhead)
    return buckets_.size() * sizeof(HashNode*) + size_ * 32;
}
