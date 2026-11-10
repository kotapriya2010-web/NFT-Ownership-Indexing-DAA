// core_test.cpp - self-checks for the DAA core (no Crow needed).
// Build target: core_test   Run: ./core_test [--bench]
#include <cstdio>
#include <cstring>
#include <iostream>

#include "Benchmark.h"
#include "NFTManager.h"

static int failures = 0;
#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::printf("FAIL  line %d: %s\n", __LINE__, #cond);            \
            ++failures;                                                     \
        }                                                                   \
    } while (0)

int main(int argc, char** argv) {
    // 1. Both indexes find every token and agree with each other
    NFTManager m;
    m.generate(20000, 7, 4);
    CHECK(m.rebuildIndexes());
    CHECK(m.sortedArray().isSorted());
    CHECK(m.hashTable().size() == 20000);
    CHECK(m.sortedArray().size() == 20000);
    for (size_t i = 0; i < m.totalNFTs(); ++i) {
        size_t a = 0, b = 0, c1 = 0, c2 = 0;
        CHECK(m.hashLookup(m.record(i).tokenId, a, c1));
        CHECK(m.binaryLookup(m.record(i).tokenId, b, c2));
        CHECK(a == i && b == i);
        CHECK(m.record(i).history.size() >= 1);
        CHECK(m.record(i).currentOwner == m.record(i).history.back().newOwner);
        CHECK(m.record(i).history.front().previousOwner == "MINT");
    }
    size_t idx = 0, cmp = 0;
    CHECK(!m.hashLookup(5, idx, cmp));
    CHECK(!m.binaryLookup(5, idx, cmp));
    CHECK(!m.binaryLookup(1LL << 60, idx, cmp));

    // 2. Binary search comparisons are bounded by floor(log2 n) + 1
    size_t maxCmp = 0;
    for (size_t i = 0; i < m.totalNFTs(); ++i) {
        m.binaryLookup(m.record(i).tokenId, idx, cmp);
        if (cmp > maxCmp) maxCmp = cmp;
    }
    std::printf("max binary comparisons for n=20000: %zu (bound 15)\n", maxCmp);
    CHECK(maxCmp <= 15);

    // 3. Transfer appends history and never rewrites old entries
    int64_t id = m.record(100).tokenId;
    size_t before = m.record(100).history.size();
    Transaction oldFirst = m.record(100).history.front();
    std::string prevOwner = m.record(100).currentOwner;
    std::string err;
    Transaction tx;
    bool persisted = false;
    CHECK(m.transfer(id, "0xdeadbeef", err, tx, persisted));
    CHECK(m.record(100).history.size() == before + 1);
    CHECK(m.record(100).currentOwner == "0xdeadbeef");
    CHECK(tx.previousOwner == prevOwner && tx.newOwner == "0xdeadbeef");
    CHECK(m.record(100).history.front().newOwner == oldFirst.newOwner);
    CHECK(!m.transfer(id, "0xdeadbeef", err, tx, persisted));   // same owner
    CHECK(!m.transfer(5, "0xabc", err, tx, persisted));          // unknown id

    // 4. Sorted array insert keeps order; duplicates rejected
    SortedArray sa;
    int64_t keys[] = {50, 10, 30, 20, 40, 5, 60};
    for (size_t i = 0; i < 7; ++i) CHECK(sa.insert(keys[i], i));
    CHECK(!sa.insert(30, 99));
    CHECK(sa.isSorted() && sa.size() == 7);

    // 5. Hash table: duplicates rejected, rehash keeps everything findable
    HashTable ht(3);
    for (int64_t k = 1; k <= 500; ++k) CHECK(ht.insert(k * 7, static_cast<size_t>(k)));
    CHECK(!ht.insert(7, 1));
    CHECK(ht.size() == 500 && ht.loadFactor() <= 1.0);
    for (int64_t k = 1; k <= 500; ++k) {
        size_t r = 0, c = 0;
        CHECK(ht.find(k * 7, r, c) && r == static_cast<size_t>(k));
    }
    size_t chainSum = 0;
    for (size_t b = 0; b < ht.bucketCount(); ++b) chainSum += ht.chainLength(b);
    CHECK(chainSum == 500);

    // 6. CSV round trip
    NFTManager a;
    a.generate(3000, 99, 5);
    a.rebuildIndexes();
    a.setDataDir("core_test_data");
    std::string e;
    CHECK(a.saveCsv("core_test_data", e));
    a.transfer(a.record(5).tokenId, "0xfeedface", e, tx, persisted);
    CHECK(persisted);
    NFTManager b;
    CHECK(b.loadCsv("core_test_data", e));
    CHECK(b.totalNFTs() == 3000);
    CHECK(b.totalTransactions() == a.totalTransactions());
    CHECK(b.record(5).currentOwner == "0xfeedface");
    CHECK(b.record(5).history.size() == a.record(5).history.size());
    CHECK(b.uniqueOwners() == a.uniqueOwners());

    // 7. Small benchmark sanity
    std::vector<size_t> sizes = {1000, 10000};
    std::vector<BenchmarkRow> rows = runBenchmark(sizes, 2000, 3, 1);
    for (size_t i = 0; i < rows.size(); ++i) {
        CHECK(rows[i].foundHash == rows[i].totalLookups);
        CHECK(rows[i].foundBinary == rows[i].totalLookups);
    }

    if (argc > 1 && std::strcmp(argv[1], "--bench") == 0) {
        std::vector<size_t> all = {1000, 10000, 100000, 1000000};
        std::vector<BenchmarkRow> r = runBenchmark(all, 100000, 10, 12345);
        std::printf("\n%10s %12s %12s %9s %9s %9s %8s %8s %10s\n", "n", "hash ns", "binary ns", "speedup",
                    "h cmp", "b cmp", "buckets", "load", "collisions");
        for (size_t i = 0; i < r.size(); ++i) {
            std::printf("%10zu %12.2f %12.2f %9.2f %9.3f %9.3f %8zu %8.3f %10zu\n", r[i].datasetSize,
                        r[i].hashAvgNs, r[i].binaryAvgNs, r[i].speedup, r[i].hashAvgComparisons,
                        r[i].binaryAvgComparisons, r[i].buckets, r[i].loadFactor, r[i].collisions);
        }
        for (size_t i = 0; i < r.size(); ++i)
            std::printf("n=%zu build hash %.1f ms, build sorted %.1f ms, found %zu/%zu %zu/%zu\n",
                        r[i].datasetSize, r[i].hashBuildMs, r[i].binaryBuildMs, r[i].foundHash,
                        r[i].totalLookups, r[i].foundBinary, r[i].totalLookups);
    }

    std::printf(failures == 0 ? "ALL CORE TESTS PASSED\n" : "%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
