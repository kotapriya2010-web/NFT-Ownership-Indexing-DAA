// main.cpp - Crow REST server for the Fast NFT Ownership and History Lookup System.
//
// Usage:
//   nft_server [--port 8080] [--data ../data] [--frontend ../frontend]
//              [--size 10000] [--regenerate] [--seed 42]
//
// The server loads <data>/nfts.csv + transactions.csv if they exist; otherwise it
// generates --size NFTs and saves them. --regenerate forces a fresh dataset.

#include <crow.h>
#include <crow/middlewares/cors.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "Benchmark.h"
#include "NFTManager.h"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

namespace {

constexpr size_t kMaxOwnerLen = 64;

crow::response jsonResponse(int status, const json& body) {
    crow::response res(status, body.dump());
    res.set_header("Content-Type", "application/json");
    return res;
}

crow::response errorResponse(int status, const std::string& message) {
    return jsonResponse(status, json{{"success", false}, {"error", message}});
}

// Strict positive integer parse (digits only, <= 18 digits, no overflow).
bool parseTokenId(const std::string& s, int64_t& out) {
    if (s.empty() || s.size() > 18) return false;
    int64_t v = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
    }
    if (v <= 0) return false;
    out = v;
    return true;
}

// Owner names are written to CSV, so no commas / line breaks / control characters.
bool validOwner(const std::string& s, std::string& why) {
    if (s.empty()) { why = "newOwner must not be empty"; return false; }
    if (s.size() > kMaxOwnerLen) { why = "newOwner is longer than 64 characters"; return false; }
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 32 || c == ',' || c == 127) { why = "newOwner contains invalid characters (comma or control character)"; return false; }
    }
    if (s == MINT_OWNER) { why = "MINT is reserved"; return false; }
    return true;
}

json transactionJson(const Transaction& t, size_t index) {
    return json{{"index", index},
                {"previousOwner", t.previousOwner},
                {"newOwner", t.newOwner},
                {"timestamp", t.timestamp},
                {"time", formatTimestamp(t.timestamp)}};
}

bool parseSizeList(const std::string& text, std::vector<size_t>& out, std::string& err) {
    out.clear();
    size_t start = 0;
    while (start <= text.size()) {
        size_t comma = text.find(',', start);
        std::string part = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        int64_t v = 0;
        if (!parseTokenId(part, v) || v > 2000000) {
            err = "sizes must be a comma-separated list of integers between 1 and 2000000";
            return false;
        }
        out.push_back(static_cast<size_t>(v));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    if (out.empty() || out.size() > 8) { err = "provide between 1 and 8 sizes"; return false; }
    return true;
}

bool parseBoundedInt(const char* raw, int64_t lo, int64_t hi, int64_t def, int64_t& out) {
    if (raw == nullptr) { out = def; return true; }
    int64_t v = 0;
    if (!parseTokenId(raw, v) || v < lo || v > hi) return false;
    out = v;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    int port = 8080;
    std::string dataDir = "../data";
    std::string frontendDir = "../frontend";
    size_t datasetSize = 10000;
    uint64_t seed = 42;
    bool regenerate = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) { std::cerr << "Missing value for " << name << "\n"; std::exit(1); }
            return argv[++i];
        };
        if (a == "--port") port = std::atoi(next("--port").c_str());
        else if (a == "--data") dataDir = next("--data");
        else if (a == "--frontend") frontendDir = next("--frontend");
        else if (a == "--size") datasetSize = static_cast<size_t>(std::strtoull(next("--size").c_str(), nullptr, 10));
        else if (a == "--seed") seed = std::strtoull(next("--seed").c_str(), nullptr, 10);
        else if (a == "--regenerate") regenerate = true;
        else { std::cerr << "Unknown option: " << a << "\n"; return 1; }
    }
    if (port < 1 || port > 65535 || datasetSize < 1 || datasetSize > 2000000) {
        std::cerr << "Invalid --port or --size (size must be 1..2000000)\n";
        return 1;
    }

    // ---- load or generate the dataset --------------------------------------
    NFTManager manager;
    std::string error;
    bool loaded = false;
    namespace fs = std::filesystem;
    if (!regenerate && fs::exists(fs::path(dataDir) / "nfts.csv") && fs::exists(fs::path(dataDir) / "transactions.csv")) {
        auto t0 = Clock::now();
        if (manager.loadCsv(dataDir, error)) {
            loaded = true;
            std::cout << "Loaded " << manager.totalNFTs() << " NFTs from " << dataDir << " in "
                      << std::chrono::duration<double, std::milli>(Clock::now() - t0).count() << " ms\n";
        } else {
            std::cerr << "Could not load existing data (" << error << ").\n"
                      << "Fix or delete the CSV files, or run with --regenerate.\n";
            return 1;
        }
    }
    if (!loaded) {
        std::cout << "Generating " << datasetSize << " NFTs (seed " << seed << ") ...\n";
        manager.generate(datasetSize, seed, 4);
        if (!manager.rebuildIndexes()) { std::cerr << "Index build failed\n"; return 1; }
        if (!manager.saveCsv(dataDir, error)) { std::cerr << error << "\n"; return 1; }
        std::cout << "Saved dataset to " << dataDir << "\n";
    }
    manager.setDataDir(dataDir);

    std::shared_mutex dataMutex;   // many readers (lookups) or one writer (transfer)
    std::mutex benchMutex;         // one benchmark at a time

    // ---- Crow app + CORS -----------------------------------------------------
    crow::App<crow::CORSHandler> app;
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin("*")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post, crow::HTTPMethod::Options)
        .headers("Content-Type", "Accept");

    // GET /api/health
    CROW_ROUTE(app, "/api/health")([&]() {
        std::shared_lock<std::shared_mutex> lock(dataMutex);
        return jsonResponse(200, json{{"success", true}, {"status", "ok"}, {"nfts", manager.totalNFTs()}});
    });

    // GET /api/dashboard
    CROW_ROUTE(app, "/api/dashboard")([&]() {
        std::shared_lock<std::shared_mutex> lock(dataMutex);
        const HashTable& h = manager.hashTable();
        size_t n = manager.totalNFTs();
        json samples = json::array();
        if (n > 0) {
            samples.push_back(manager.record(0).tokenId);
            samples.push_back(manager.record(n / 2).tokenId);
            samples.push_back(manager.record(n - 1).tokenId);
        }
        return jsonResponse(200, json{{"success", true},
                                      {"totalNFTs", n},
                                      {"uniqueOwners", manager.uniqueOwners()},
                                      {"totalTransfers", manager.totalTransfers()},
                                      {"totalTransactions", manager.totalTransactions()},
                                      {"loadFactor", h.loadFactor()},
                                      {"collisions", h.collisions()},
                                      {"buckets", h.bucketCount()},
                                      {"sampleTokenIds", samples}});
    });

    // GET /api/nfts/<id>
    CROW_ROUTE(app, "/api/nfts/<string>")([&](const std::string& idText) {
        int64_t id;
        if (!parseTokenId(idText, id)) return errorResponse(400, "Token ID must be a positive integer");
        std::shared_lock<std::shared_mutex> lock(dataMutex);
        size_t idx = 0, cmp = 0;
        if (!manager.hashLookup(id, idx, cmp)) return errorResponse(404, "NFT " + idText + " does not exist");
        const NFTRecord& r = manager.record(idx);
        return jsonResponse(200, json{{"success", true},
                                      {"tokenId", r.tokenId},
                                      {"currentOwner", r.currentOwner},
                                      {"transactionCount", r.history.size()},
                                      {"transferCount", r.history.size() - 1}});
    });

    // GET /api/nfts/<id>/history
    CROW_ROUTE(app, "/api/nfts/<string>/history")([&](const std::string& idText) {
        int64_t id;
        if (!parseTokenId(idText, id)) return errorResponse(400, "Token ID must be a positive integer");
        std::shared_lock<std::shared_mutex> lock(dataMutex);
        size_t idx = 0, cmp = 0;
        if (!manager.hashLookup(id, idx, cmp)) return errorResponse(404, "NFT " + idText + " does not exist");
        const NFTRecord& r = manager.record(idx);
        json txs = json::array();
        for (size_t i = 0; i < r.history.size(); ++i) txs.push_back(transactionJson(r.history[i], i));
        return jsonResponse(200, json{{"success", true},
                                      {"tokenId", r.tokenId},
                                      {"currentOwner", r.currentOwner},
                                      {"count", r.history.size()},
                                      {"history", txs}});
    });

    // GET /api/search/hash/<id>  and  /api/search/binary/<id>
    auto searchHandler = [&](const std::string& idText, bool useHash) {
        int64_t id;
        if (!parseTokenId(idText, id)) return errorResponse(400, "Token ID must be a positive integer");
        const char* method = useHash ? "Hash Table (separate chaining)" : "Binary Search (sorted array)";
        std::shared_lock<std::shared_mutex> lock(dataMutex);
        size_t idx = 0, cmp = 0;
        // Only the lookup itself is inside the timer.
        auto t0 = Clock::now();
        bool found = useHash ? manager.hashLookup(id, idx, cmp) : manager.binaryLookup(id, idx, cmp);
        auto t1 = Clock::now();
        long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        json body = {{"success", found},
                     {"found", found},
                     {"tokenId", id},
                     {"method", method},
                     {"executionTimeNs", ns},
                     {"executionTimeUs", static_cast<double>(ns) / 1000.0},
                     {"comparisons", cmp},
                     {"timingNote", "single-lookup timing is noisy (clock overhead is comparable to the lookup); use the benchmark page for reliable averages"}};
        if (!found) {
            body["error"] = "NFT " + idText + " does not exist";
            return jsonResponse(404, body);
        }
        body["currentOwner"] = manager.record(idx).currentOwner;
        return jsonResponse(200, body);
    };
    CROW_ROUTE(app, "/api/search/hash/<string>")([&](const std::string& id) { return searchHandler(id, true); });
    CROW_ROUTE(app, "/api/search/binary/<string>")([&](const std::string& id) { return searchHandler(id, false); });

    // POST /api/nfts/<id>/transfer   body: {"newOwner": "0x..."}
    CROW_ROUTE(app, "/api/nfts/<string>/transfer").methods(crow::HTTPMethod::Post)([&](const crow::request& req, const std::string& idText) {
        int64_t id;
        if (!parseTokenId(idText, id)) return errorResponse(400, "Token ID must be a positive integer");
        json body = json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.is_object() || !body.contains("newOwner") || !body["newOwner"].is_string())
            return errorResponse(400, "Body must be JSON like {\"newOwner\": \"0xabc123\"}");
        std::string newOwner = body["newOwner"].get<std::string>();
        std::string why;
        if (!validOwner(newOwner, why)) return errorResponse(400, why);

        std::unique_lock<std::shared_mutex> lock(dataMutex);
        Transaction created;
        bool persisted = false;
        std::string err;
        if (!manager.transfer(id, newOwner, err, created, persisted)) {
            int status = err == "NFT not found" ? 404 : (err.rfind("Internal", 0) == 0 || err.rfind("Hash", 0) == 0 ? 500 : 409);
            return errorResponse(status, err == "NFT not found" ? "NFT " + idText + " does not exist" : err);
        }
        return jsonResponse(200, json{{"success", true},
                                      {"tokenId", id},
                                      {"transaction", transactionJson(created, 0)},
                                      {"persisted", persisted}});
    });

    // GET /api/hash-stats
    CROW_ROUTE(app, "/api/hash-stats")([&]() {
        std::shared_lock<std::shared_mutex> lock(dataMutex);
        const HashTable& h = manager.hashTable();
        const size_t m = h.bucketCount();

        // chain-length histogram (last bin = "8 or more")
        const size_t kBins = 9;
        std::vector<size_t> hist(kBins, 0);
        size_t longest = 0;
        // bucket groups: at most 200 cells, each = number of elements in a contiguous range of buckets
        const size_t cells = m < 200 ? m : 200;
        std::vector<size_t> groupElems(cells, 0), groupBuckets(cells, 0);
        for (size_t b = 0; b < m; ++b) {
            size_t len = h.chainLength(b);
            hist[len < kBins - 1 ? len : kBins - 1]++;
            if (len > longest) longest = len;
            size_t g = b * cells / m;
            groupElems[g] += len;
            groupBuckets[g] += 1;
        }
        json histJson = json::array();
        for (size_t i = 0; i < kBins; ++i)
            histJson.push_back({{"chainLength", i == kBins - 1 ? "8+" : std::to_string(i)}, {"buckets", hist[i]}});
        json groups = json::array();
        for (size_t g = 0; g < cells; ++g)
            groups.push_back({{"buckets", groupBuckets[g]}, {"elements", groupElems[g]}});

        size_t empty = hist[0];
        size_t used = m - empty;
        return jsonResponse(200, json{{"success", true},
                                      {"buckets", m},
                                      {"elements", h.size()},
                                      {"loadFactor", h.loadFactor()},
                                      {"collisions", h.collisions()},
                                      {"emptyBuckets", empty},
                                      {"usedBuckets", used},
                                      {"longestChain", longest},
                                      {"averageChainLengthNonEmpty", used ? static_cast<double>(h.size()) / used : 0.0},
                                      {"hashFunction", "MurmurHash3 fmix64, then mod prime bucket count"},
                                      {"chainLengthHistogram", histJson},
                                      {"bucketGroups", groups}});
    });

    // GET /api/benchmark?sizes=1000,10000&queries=100000&reps=10&seed=12345
    CROW_ROUTE(app, "/api/benchmark")([&](const crow::request& req) {
        std::vector<size_t> sizes = {1000, 10000, 100000, 1000000};
        std::string err;
        if (const char* s = req.url_params.get("sizes")) {
            if (!parseSizeList(s, sizes, err)) return errorResponse(400, err);
        }
        int64_t queries = 0, reps = 0, seed = 0;
        if (!parseBoundedInt(req.url_params.get("queries"), 1, 1000000, 100000, queries))
            return errorResponse(400, "queries must be an integer between 1 and 1000000");
        if (!parseBoundedInt(req.url_params.get("reps"), 1, 100, 10, reps))
            return errorResponse(400, "reps must be an integer between 1 and 100");
        if (!parseBoundedInt(req.url_params.get("seed"), 1, 1000000000, 12345, seed))
            return errorResponse(400, "seed must be a positive integer");
        if (queries * reps > 20000000) return errorResponse(400, "queries * reps must not exceed 20,000,000");

        std::unique_lock<std::mutex> bl(benchMutex, std::try_to_lock);
        if (!bl.owns_lock()) return errorResponse(429, "A benchmark is already running; try again when it finishes");

        // The benchmark builds its own datasets, so it never touches the live data.
        auto t0 = Clock::now();
        std::vector<BenchmarkRow> rows = runBenchmark(sizes, static_cast<size_t>(queries), static_cast<size_t>(reps), static_cast<uint64_t>(seed));
        double totalMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

        json results = json::array();
        for (size_t i = 0; i < rows.size(); ++i) {
            const BenchmarkRow& r = rows[i];
            results.push_back({{"datasetSize", r.datasetSize},
                               {"queryCount", r.queryCount},
                               {"repetitions", r.repetitions},
                               {"totalLookups", r.totalLookups},
                               {"hashTotalMs", r.hashTotalMs},
                               {"binaryTotalMs", r.binaryTotalMs},
                               {"hashAvgNs", r.hashAvgNs},
                               {"binaryAvgNs", r.binaryAvgNs},
                               {"hashAvgComparisons", r.hashAvgComparisons},
                               {"binaryAvgComparisons", r.binaryAvgComparisons},
                               {"speedup", r.speedup},
                               {"hashBuildMs", r.hashBuildMs},
                               {"binaryBuildMs", r.binaryBuildMs},
                               {"buckets", r.buckets},
                               {"loadFactor", r.loadFactor},
                               {"collisions", r.collisions},
                               {"allFoundHash", r.foundHash == r.totalLookups},
                               {"allFoundBinary", r.foundBinary == r.totalLookups}});
        }
        return jsonResponse(200, json{{"success", true},
                                      {"timingUnit", "nanoseconds per lookup (avg over all lookups)"},
                                      {"clock", "std::chrono::steady_clock"},
                                      {"seed", seed},
                                      {"elapsedMs", totalMs},
                                      {"results", results}});
    });

    // ---- static frontend (so http://localhost:PORT/ just works) ---------------
    CROW_ROUTE(app, "/")([frontendDir](const crow::request&, crow::response& res) {
        res.set_static_file_info_unsafe((fs::path(frontendDir) / "index.html").string());
        res.end();
    });
    CROW_ROUTE(app, "/<path>")([frontendDir](const crow::request&, crow::response& res, const std::string& path) {
        const fs::path relativePath(path);
        if (path.rfind("api/", 0) == 0 || path.find("..") != std::string::npos ||
            path.find('\\') != std::string::npos || relativePath.is_absolute() || relativePath.has_root_name()) {
            res = errorResponse(404, "Unknown API endpoint");
            res.end();
            return;
        }
        res.set_static_file_info_unsafe((fs::path(frontendDir) / relativePath).string());
        res.end();
    });

    std::cout << "NFT server: " << manager.totalNFTs() << " NFTs, hash buckets " << manager.hashTable().bucketCount()
              << "\nOpen http://localhost:" << port << "/  (API under /api/)\n";
    app.bindaddr("127.0.0.1").port(static_cast<uint16_t>(port)).multithreaded().run();
    return 0;
}
