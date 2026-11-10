# NFT Ownership Indexing and Benchmarking System

An undergraduate Design and Analysis of Algorithms project that compares two hand-built indexing strategies for NFT ownership lookup: a separate-chaining hash table and binary search over a sorted array. The system models ownership and append-only transaction histories, then measures both indexes on identical datasets and queries through an interactive web dashboard.

This project is an educational simulation. It does not connect to a blockchain, mint real NFTs, or handle cryptocurrency.

## Project goals

- Implement separate chaining and binary search without relying on STL associative containers or search algorithms.
- Keep NFT records in one central collection, with each index storing only a Token ID and its record position.
- Preserve ownership history as an append-only sequence of timestamped transactions.
- Compare measured lookup time and comparison counts with expected O(1) average hash lookup and O(log n) binary search.
- Present ownership lookups, histories, benchmark results, and hash-table statistics in a browser dashboard.

## Architecture

```text
Browser dashboard (HTML, CSS, JavaScript)
                 | JSON over HTTP
                 v
          Crow REST server
                 |
                 v
             NFTManager
        /         |         \
 NFT records  HashTable  SortedArray
        |
   CSV persistence
```

The C++ manager owns the only copy of each NFT record. The hash table and sorted array independently map Token IDs to positions in that collection. Since transfers change an owner but never a Token ID, ownership updates do not alter either index.

## Algorithms and data structures

### Separate-chaining hash table

The hash index uses a hand-written 64-bit fmix64 function followed by modulo a prime-sized bucket array. Each bucket points to a singly linked chain. The implementation tracks collisions, load factor, empty buckets, longest chain, and chain-length distribution.

- Average lookup: O(1) when keys are distributed well and the load factor is controlled.
- Worst-case lookup: O(n) if many keys land in one chain.
- Insert: O(1) on average, with occasional O(n) rehashing.

### Sorted array and binary search

The second index stores `(tokenId, recordIndex)` entries in ascending order. It is built with a hand-written bottom-up merge sort when the input is not already ordered, and searched with a manual binary search.

- Lookup: O(log n).
- Single insertion: O(n) in the worst case because later entries may need to shift.
- The sorted representation also supports ordered traversal and range-oriented operations.

### Ownership history

Each NFT has a current owner and a transaction history. The first entry represents minting; each later transfer records the previous owner, new owner, and timestamp. Transfers append a transaction rather than rewriting earlier history. CSV persistence stores the NFT snapshot and transaction sequence.

## Benchmark methodology

For each selected dataset size, the benchmark creates a deterministic synthetic dataset and builds both indexes outside the lookup timers. It then generates one seeded list of existing Token IDs, warms both lookup methods, and times repeated passes over the same queries. Results include average nanoseconds per lookup, average comparisons, index construction times, hash-table statistics, and a check that all queries were found.

Benchmark results depend on hardware, compiler optimizations, cache behavior, and system load. The comparison is intended to illustrate algorithmic trends, not to claim universally applicable timings.

## Dashboard features

- Dataset summary, owner and transfer counts, and sample Token IDs.
- Current-owner lookup through the hash index, binary search, or both.
- Chronological ownership-history display and simulated transfers.
- Side-by-side lookup benchmark chart, table, and measured-data analysis.
- Hash-table chain-length histogram and bucket-distribution visualization.

## Technology

- C++17 core and Crow REST backend
- nlohmann/json for JSON responses
- HTML, CSS, and vanilla JavaScript frontend
- CMake build configuration
- Self-checking C++ core tests

## Academic scope

The project focuses on data structures, algorithmic complexity, empirical benchmarking, and the trade-offs between hash-based and ordered indexing. All marketplace records and ownership changes are simulated; no real wallets, assets, or blockchain network are involved.
