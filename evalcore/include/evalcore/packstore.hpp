// Verified, offline-capable local cache of jurisdiction packs.
//
// Trust chain: the catalog (index.json) must carry a valid minisign signature from the
// embedded key; each pack must match the SHA-256 and size listed in that catalog and pass
// pack validation. Nothing reaches the cache unless every check passes, and the cached
// files are re-checked against their recorded hashes every time they are loaded.
//
// This class does no networking. Callers supply a Fetcher (Qt in the app, a directory in
// tests and CI).
#pragma once

#include <map>
#include <string>
#include <vector>

#include "evalcore/catalog.hpp"
#include "evalcore/minisign.hpp"
#include "evalcore/pack.hpp"

namespace jobeval {

class Fetcher {
public:
    virtual ~Fetcher() = default;
    // `path` is relative to the catalog root: "index.json", "index.json.minisig" or "packs/<name>.json".
    virtual bool get(const std::string& path, std::string& out, std::string& error) = 0;
};

// Serves a catalog from a local directory (a downloaded copy of the published site).
class DirFetcher : public Fetcher {
public:
    explicit DirFetcher(std::string root) : root_(std::move(root)) {}
    bool get(const std::string& path, std::string& out, std::string& error) override;

private:
    std::string root_;
};

class PackStore {
public:
    PackStore(std::string cache_dir, MinisignKey key) : dir_(std::move(cache_dir)), key_(key) {}

    // Downloads and verifies the catalog, refuses one older than the cached copy (rollback
    // protection), then caches it. `out` is only set on success.
    bool refresh_catalog(Fetcher& fetcher, Catalog& out, Diagnostics& diag);
    // The last verified catalog, re-verified against its cached signature.
    bool load_cached_catalog(Catalog& out, Diagnostics& diag) const;

    // Installs `pack_id` and the packs it extends. All-or-nothing: on any failure the cache
    // is unchanged. A replaced revision is kept as "previous" for rollback.
    bool install(const Catalog& catalog, const std::string& pack_id, Fetcher& fetcher, Diagnostics& diag);

    // Loads every installed pack into `set`, re-verifying each file's hash.
    bool load_installed(PackSet& set, Diagnostics& diag) const;

    // Swaps `pack_id` back to its previous revision.
    bool rollback(const std::string& pack_id, Diagnostics& diag);

    std::map<std::string, InstallRecord> installed() const;
    std::map<std::string, InstallRecord> previous() const;
    // Installed packs whose catalog entry has a different hash (a newer published revision).
    std::vector<std::string> updates_available(const Catalog& catalog) const;

private:
    struct State {
        std::map<std::string, InstallRecord> installed;
        std::map<std::string, InstallRecord> previous;
    };
    bool read_state(State& st, Diagnostics* diag) const;
    bool write_state(const State& st, Diagnostics& diag) const;
    bool read_cached_pack(const InstallRecord& rec, std::string& text, Diagnostics& diag) const;

    std::string dir_;
    MinisignKey key_;
};

bool read_whole_file(const std::string& path, std::string& out);
bool write_file_atomic(const std::string& path, const std::string& data, std::string& error);

}  // namespace jobeval
