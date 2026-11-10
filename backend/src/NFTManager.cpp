#include "NFTManager.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

// splitmix64: tiny deterministic PRNG so the same seed gives the same dataset
// on every compiler/OS (std::mt19937 distributions are not portable).
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    uint64_t next() {
        uint64_t z = (s += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
    uint64_t below(uint64_t n) { return next() % n; }
};

std::string walletName(uint64_t v) {
    static const char* hex = "0123456789abcdef";
    std::string s = "0x";
    for (int i = 0; i < 8; ++i) s.push_back(hex[(v >> (4 * i)) & 0xF]);  // 10 chars: fits SSO
    return s;
}

std::vector<std::string> splitCsv(const std::string& line, size_t expected) {
    std::vector<std::string> out;
    size_t start = 0;
    while (out.size() + 1 < expected) {
        size_t c = line.find(',', start);
        if (c == std::string::npos) break;
        out.push_back(line.substr(start, c - start));
        start = c + 1;
    }
    std::string last = line.substr(start);
    if (!last.empty() && last.back() == '\r') last.pop_back();
    out.push_back(last);
    return out;
}

bool parseInt64(const std::string& s, int64_t& out) {
    if (s.empty() || s.size() > 19) return false;
    int64_t v = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
    }
    out = v;
    return true;
}

}  // namespace

void NFTManager::generate(size_t count, uint64_t seed, int maxHistory) {
    if (maxHistory < 1) maxHistory = 1;
    Rng rng(seed);

    size_t poolSize = count / 4;
    if (poolSize < 8) poolSize = 8;
    std::vector<std::string> pool;
    pool.reserve(poolSize);
    for (size_t i = 0; i < poolSize; ++i) pool.push_back(walletName(rng.next() ^ (i * 0x100000001b3ULL)));

    records_.clear();
    records_.reserve(count);
    totalTransactions_ = 0;

    const int64_t base = 1640995200;  // 2022-01-01 00:00:00 UTC
    int64_t tokenId = 1000;
    for (size_t i = 0; i < count; ++i) {
        tokenId += 1 + static_cast<int64_t>(rng.below(3));  // ascending, unique, small gaps
        NFTRecord rec;
        rec.tokenId = tokenId;
        size_t entries = 1 + static_cast<size_t>(rng.below(static_cast<uint64_t>(maxHistory)));
        int64_t ts = base + static_cast<int64_t>(rng.below(90ULL * 86400ULL));
        std::string prev = MINT_OWNER;
        rec.history.reserve(entries);
        for (size_t e = 0; e < entries; ++e) {
            std::string next = pool[rng.below(poolSize)];
            while (next == prev) next = pool[rng.below(poolSize)];
            rec.history.push_back(Transaction{prev, next, ts});
            prev = next;
            ts += 3600 + static_cast<int64_t>(rng.below(60ULL * 86400ULL));
        }
        rec.currentOwner = prev;
        totalTransactions_ += entries;
        records_.push_back(std::move(rec));
    }
    uniqueOwnersDirty_ = true;
}

bool NFTManager::buildHashIndex() {
    hash_.reset(HashTable::suggestedBucketCount(records_.size()));
    for (size_t i = 0; i < records_.size(); ++i) {
        if (!hash_.insert(records_[i].tokenId, i)) return false;
    }
    return true;
}

bool NFTManager::buildSortedIndex() {
    sorted_.buildFromRecords(records_);
    return sorted_.isSorted();  // false if duplicates exist
}

bool NFTManager::rebuildIndexes() {
    bool a = buildHashIndex();
    bool b = buildSortedIndex();
    return a && b;
}

bool NFTManager::transfer(int64_t tokenId, const std::string& newOwner, std::string& error,
                          Transaction& created, bool& persisted) {
    persisted = false;
    size_t cmp = 0, idx = 0;
    if (!hash_.find(tokenId, idx, cmp)) {
        error = "NFT not found";
        return false;
    }
    if (records_[idx].currentOwner == newOwner) {
        error = "New owner is already the current owner";
        return false;
    }
    // timestamps never go backwards inside one history
    int64_t now = static_cast<int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    int64_t last = records_[idx].history.back().timestamp;
    int64_t ts = now > last ? now : last + 1;

    if (!hash_.updateOwner(records_, tokenId, newOwner, ts, cmp)) {
        error = "Hash index update failed";
        return false;
    }
    created = records_[idx].history.back();
    ++totalTransactions_;
    uniqueOwnersDirty_ = true;

    // Consistency check: the independent sorted index must resolve to the same record.
    size_t idx2 = 0;
    if (!sorted_.find(tokenId, idx2, cmp) || idx2 != idx) {
        error = "Internal error: indexes disagree";
        return false;
    }

    if (!dataDir_.empty()) {
        std::ofstream out(dataDir_ + "/transactions.csv", std::ios::app);
        if (out) {
            out << tokenId << ',' << created.previousOwner << ',' << created.newOwner << ',' << created.timestamp << '\n';
            out.flush();
            persisted = static_cast<bool>(out);
        }
    }
    return true;
}

size_t NFTManager::uniqueOwners() const {
    if (!uniqueOwnersDirty_) return uniqueOwnersCache_;
    std::vector<const std::string*> owners;
    owners.reserve(records_.size());
    for (size_t i = 0; i < records_.size(); ++i) owners.push_back(&records_[i].currentOwner);
    std::sort(owners.begin(), owners.end(),
              [](const std::string* a, const std::string* b) { return *a < *b; });
    size_t count = 0;
    for (size_t i = 0; i < owners.size(); ++i) {
        if (i == 0 || *owners[i] != *owners[i - 1]) ++count;
    }
    uniqueOwnersCache_ = count;
    uniqueOwnersDirty_ = false;
    return count;
}

bool NFTManager::saveCsv(const std::string& dir, std::string& error) const {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::ofstream nfts(dir + "/nfts.csv", std::ios::trunc);
    std::ofstream txs(dir + "/transactions.csv", std::ios::trunc);
    if (!nfts || !txs) {
        error = "Cannot write CSV files in " + dir;
        return false;
    }
    nfts << "tokenId,currentOwner\n";
    txs << "tokenId,previousOwner,newOwner,timestamp\n";
    for (size_t i = 0; i < records_.size(); ++i) {
        const NFTRecord& r = records_[i];
        nfts << r.tokenId << ',' << r.currentOwner << '\n';
        for (size_t t = 0; t < r.history.size(); ++t) {
            const Transaction& tx = r.history[t];
            txs << r.tokenId << ',' << tx.previousOwner << ',' << tx.newOwner << ',' << tx.timestamp << '\n';
        }
    }
    nfts.flush();
    txs.flush();
    if (!nfts || !txs) {
        error = "Write error while saving CSV files";
        return false;
    }
    return true;
}

bool NFTManager::loadCsv(const std::string& dir, std::string& error) {
    std::ifstream nfts(dir + "/nfts.csv");
    std::ifstream txs(dir + "/transactions.csv");
    if (!nfts || !txs) {
        error = "CSV files not found in " + dir;
        return false;
    }
    records_.clear();
    totalTransactions_ = 0;
    uniqueOwnersDirty_ = true;

    std::string line;
    std::getline(nfts, line);  // header
    while (std::getline(nfts, line)) {
        if (line.empty() || line == "\r") continue;
        std::vector<std::string> f = splitCsv(line, 2);
        NFTRecord r;
        if (f.size() != 2 || !parseInt64(f[0], r.tokenId)) {
            error = "Bad line in nfts.csv: " + line;
            return false;
        }
        r.currentOwner = f[1];
        records_.push_back(std::move(r));
    }
    if (!rebuildIndexes()) {
        error = "nfts.csv contains duplicate token IDs";
        return false;
    }

    std::getline(txs, line);  // header
    size_t lineNo = 1;
    while (std::getline(txs, line)) {
        ++lineNo;
        if (line.empty() || line == "\r") continue;
        std::vector<std::string> f = splitCsv(line, 4);
        int64_t id = 0, ts = 0;
        if (f.size() != 4 || !parseInt64(f[0], id) || !parseInt64(f[3], ts)) {
            error = "Bad line " + std::to_string(lineNo) + " in transactions.csv";
            return false;
        }
        size_t idx = 0, cmp = 0;
        if (!sorted_.find(id, idx, cmp)) {
            error = "transactions.csv refers to unknown token " + f[0];
            return false;
        }
        records_[idx].history.push_back(Transaction{f[1], f[2], ts});
        ++totalTransactions_;
    }
    for (size_t i = 0; i < records_.size(); ++i) {
        if (records_[i].history.empty()) {
            error = "Token " + std::to_string(records_[i].tokenId) + " has no history";
            return false;
        }
        // The history is authoritative for the current owner.
        records_[i].currentOwner = records_[i].history.back().newOwner;
    }
    return true;
}
