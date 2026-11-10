#include "Benchmark.h"

#include <chrono>

#include "NFTManager.h"

namespace {

using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point t0, Clock::time_point t1) {
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// Written to after every timed loop so the compiler cannot delete the lookups.
volatile uint64_t g_sink = 0;

}  // namespace

std::vector<BenchmarkRow> runBenchmark(const std::vector<size_t>& sizes, size_t queryCount,
                                       size_t repetitions, uint64_t seed) {
    std::vector<BenchmarkRow> rows;

    for (size_t s = 0; s < sizes.size(); ++s) {
        const size_t n = sizes[s];
        BenchmarkRow row;
        row.datasetSize = n;
        row.repetitions = repetitions;

        // ---- setup: NOT timed as lookup ----------------------------------
        NFTManager mgr;
        mgr.generate(n, seed, 1);  // 1 history entry: lookups never read history, saves RAM at 1M

        auto b0 = Clock::now();
        mgr.buildHashIndex();
        auto b1 = Clock::now();
        mgr.buildSortedIndex();
        auto b2 = Clock::now();
        row.hashBuildMs = msSince(b0, b1);
        row.binaryBuildMs = msSince(b1, b2);
        row.buckets = mgr.hashTable().bucketCount();
        row.loadFactor = mgr.hashTable().loadFactor();
        row.collisions = mgr.hashTable().collisions();

        // Identical query IDs for both methods (all exist in the dataset).
        const size_t q = queryCount;
        row.queryCount = q;
        row.totalLookups = q * repetitions;
        std::vector<int64_t> queries(q);
        uint64_t x = seed * 0x9e3779b97f4a7c15ULL + 12345;
        for (size_t i = 0; i < q; ++i) {
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;  // xorshift64
            queries[i] = mgr.record(static_cast<size_t>(x % n)).tokenId;
        }

        // ---- warm-up (untimed): fill caches, fault pages in --------------
        {
            uint64_t acc = 0;
            size_t idx = 0, cmp = 0;
            for (size_t i = 0; i < q; ++i) {
                mgr.hashLookup(queries[i], idx, cmp); acc += idx + cmp;
                mgr.binaryLookup(queries[i], idx, cmp); acc += idx + cmp;
            }
            g_sink = acc;
        }

        // ---- timed: hash table -------------------------------------------
        uint64_t hashCmp = 0, hashFound = 0, acc = 0;
        auto h0 = Clock::now();
        for (size_t r = 0; r < repetitions; ++r) {
            for (size_t i = 0; i < q; ++i) {
                size_t idx = 0, cmp = 0;
                bool ok = mgr.hashLookup(queries[i], idx, cmp);
                hashCmp += cmp;
                hashFound += ok;
                acc += mgr.record(idx).currentOwner.size();  // actually retrieve the record
            }
        }
        auto h1 = Clock::now();
        g_sink = acc;

        // ---- timed: binary search ----------------------------------------
        uint64_t binCmp = 0, binFound = 0;
        acc = 0;
        auto k0 = Clock::now();
        for (size_t r = 0; r < repetitions; ++r) {
            for (size_t i = 0; i < q; ++i) {
                size_t idx = 0, cmp = 0;
                bool ok = mgr.binaryLookup(queries[i], idx, cmp);
                binCmp += cmp;
                binFound += ok;
                acc += mgr.record(idx).currentOwner.size();
            }
        }
        auto k1 = Clock::now();
        g_sink = acc;

        const double total = static_cast<double>(row.totalLookups);
        row.hashTotalMs = msSince(h0, h1);
        row.binaryTotalMs = msSince(k0, k1);
        row.hashAvgNs = row.hashTotalMs * 1e6 / total;
        row.binaryAvgNs = row.binaryTotalMs * 1e6 / total;
        row.hashAvgComparisons = static_cast<double>(hashCmp) / total;
        row.binaryAvgComparisons = static_cast<double>(binCmp) / total;
        row.speedup = row.hashAvgNs > 0 ? row.binaryAvgNs / row.hashAvgNs : 0;
        row.foundHash = static_cast<size_t>(hashFound);
        row.foundBinary = static_cast<size_t>(binFound);
        rows.push_back(row);
    }
    return rows;
}
