// The pack catalog (index.json): which packs exist, their hashes, and what they require.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "evalcore/pack.hpp"

namespace jobeval {

struct CatalogEntry {
    std::string id;
    std::string kind;
    std::string extends;  // empty for country packs
    std::string country;
    std::string subregion;
    std::map<std::string, std::string> labels;
    std::string tier;
    int revision = 0;
    std::string valid_from;
    std::string valid_to;
    std::string requires_engine;
    std::string last_verified;
    std::string file;    // "packs/<name>.json"
    std::string sha256;  // lowercase hex of the file's bytes
    size_t size = 0;
};

struct Catalog {
    int version = 0;
    std::string generated;  // UTC ISO-8601; compared as text to detect rollback
    std::vector<CatalogEntry> packs;
    const CatalogEntry* find(const std::string& id) const;
};

// Parses and sanity-checks a catalog document. The text must already be signature-verified.
bool parse_catalog(const std::string& text, Catalog& out, Diagnostics& diag);

// Only "packs/<letters digits . _ ->.json" is ever fetched or written, so even a
// malicious catalog cannot name a path outside the cache.
bool is_safe_pack_path(const std::string& path);

struct InstallRecord {
    std::string id;
    int revision = 0;
    std::string file;
    std::string sha256;
};

// Packs to download so that `pack_id` can be used: its extends chain, parents first,
// leaving out anything already installed with the same hash. Fails if a pack is missing
// from the catalog or needs a newer engine.
bool plan_install(const Catalog& catalog, const std::string& pack_id,
                  const std::map<std::string, InstallRecord>& installed,
                  std::vector<const CatalogEntry*>& plan, Diagnostics& diag);

}  // namespace jobeval
