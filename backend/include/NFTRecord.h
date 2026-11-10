// NFTRecord.h - common record types shared by both index structures.
#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

// One ownership change. The first transaction of every NFT is its "mint":
// previousOwner == "MINT".
struct Transaction {
    std::string previousOwner;
    std::string newOwner;
    int64_t timestamp = 0;  // Unix time, seconds (UTC)
};

// Central record. Both indexes store only (tokenId -> position in the central
// vector), so there is exactly one copy of the data and the two indexes can
// never disagree about an owner.
struct NFTRecord {
    int64_t tokenId = 0;
    std::string currentOwner;
    std::vector<Transaction> history;  // append-only
};

static const char* const MINT_OWNER = "MINT";

// The ONLY function that changes ownership. It appends to the history and then
// updates currentOwner. Nothing in the project ever erases or edits an
// existing Transaction.   Time O(1) amortised, space O(1) per transfer.
inline void appendTransfer(NFTRecord& record, const std::string& newOwner, int64_t timestamp) {
    Transaction t;
    t.previousOwner = record.currentOwner;
    t.newOwner = newOwner;
    t.timestamp = timestamp;
    record.history.push_back(t);
    record.currentOwner = newOwner;
}

// "2024-03-05 14:07:09 UTC"
inline std::string formatTimestamp(int64_t ts) {
    std::time_t tt = static_cast<std::time_t>(ts);
    std::tm tmv{};
#ifdef _WIN32
    gmtime_s(&tmv, &tt);
#else
    gmtime_r(&tt, &tmv);
#endif
    char buf[40];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tmv);
    return std::string(buf);
}
