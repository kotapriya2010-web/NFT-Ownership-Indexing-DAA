// Benchmark.h - experimental comparison of the two indexes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct BenchmarkRow {
    size_t datasetSize = 0;
    size_t queryCount = 0;        // distinct query IDs
    size_t repetitions = 0;       // timed passes over the same query list
    size_t totalLookups = 0;      // queryCount * repetitions (per method)

    double hashTotalMs = 0;       // timed section only
    double binaryTotalMs = 0;
    double hashAvgNs = 0;         // nanoseconds per lookup
    double binaryAvgNs = 0;
    double hashAvgComparisons = 0;
    double binaryAvgComparisons = 0;
    double speedup = 0;           // binaryAvgNs / hashAvgNs

    double hashBuildMs = 0;       // index construction (NOT part of lookup timing)
    double binaryBuildMs = 0;
    size_t buckets = 0;
    double loadFactor = 0;
    size_t collisions = 0;
    size_t foundHash = 0;         // sanity: every query must be found by both
    size_t foundBinary = 0;
};

// For every size: generate the dataset (outside the timer), build both
// indexes (timed separately), draw ONE list of random existing token IDs
// (seeded), run an untimed warm-up pass, then time `repetitions` passes over
// that same list with the hash table and with binary search.
std::vector<BenchmarkRow> runBenchmark(const std::vector<size_t>& sizes, size_t queryCount,
                                       size_t repetitions, uint64_t seed);
