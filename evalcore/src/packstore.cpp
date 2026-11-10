#include "evalcore/packstore.hpp"

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "evalcore/hash.hpp"

namespace fs = std::filesystem;

namespace jobeval {

bool read_whole_file(const std::string& path, std::string& out) {
    std::ifstream f(fs::u8path(path), std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool write_file_atomic(const std::string& path, const std::string& data, std::string& error) {
    std::error_code ec;
    const fs::path target = fs::u8path(path);
    fs::create_directories(target.parent_path(), ec);
    if (ec) {
        error = "cannot create " + target.parent_path().string() + ": " + ec.message();
        return false;
    }
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            error = "cannot write " + tmp.string();
            return false;
        }
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        f.flush();
        if (!f) {
            error = "write failed for " + tmp.string();
            return false;
        }
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        fs::remove(tmp, ec);
        error = "cannot replace " + target.string();
        return false;
    }
    return true;
}

bool DirFetcher::get(const std::string& path, std::string& out, std::string& error) {
    if (path != "index.json" && path != "index.json.minisig" && !is_safe_pack_path(path)) {
        error = "refusing to fetch unexpected path '" + path + "'";
        return false;
    }
    if (!read_whole_file(root_ + "/" + path, out)) {
        error = "cannot read " + root_ + "/" + path;
        return false;
    }
    return true;
}

// ------------------------------------------------------------------- catalog

bool PackStore::load_cached_catalog(Catalog& out, Diagnostics& diag) const {
    std::string index, sig;
    if (!read_whole_file(dir_ + "/catalog/index.json", index) || !read_whole_file(dir_ + "/catalog/index.json.minisig", sig)) {
        diag.error("catalog", "no cached catalog");
        return false;
    }
    SignatureCheck chk = verify_minisign(key_, index, sig);
    if (!chk.ok) {
        diag.error("catalog", "cached catalog failed signature check: " + chk.error);
        return false;
    }
    return parse_catalog(index, out, diag);
}

bool PackStore::refresh_catalog(Fetcher& fetcher, Catalog& out, Diagnostics& diag) {
    std::string index, sig, err;
    if (!fetcher.get("index.json", index, err)) {
        diag.error("catalog", "could not download index.json: " + err);
        return false;
    }
    if (!fetcher.get("index.json.minisig", sig, err)) {
        diag.error("catalog", "could not download index.json.minisig (unsigned catalogs are refused): " + err);
        return false;
    }
    SignatureCheck chk = verify_minisign(key_, index, sig);
    if (!chk.ok) {
        diag.error("catalog", "signature check failed: " + chk.error);
        return false;
    }
    Catalog fresh;
    if (!parse_catalog(index, fresh, diag)) return false;

    Catalog cached;
    Diagnostics quiet;
    if (load_cached_catalog(cached, quiet) && fresh.generated < cached.generated) {
        diag.error("catalog", "the downloaded catalog (" + fresh.generated + ") is older than the one already cached (" +
                                  cached.generated + "); refusing it as a possible rollback");
        return false;
    }
    if (!write_file_atomic(dir_ + "/catalog/index.json", index, err) ||
        !write_file_atomic(dir_ + "/catalog/index.json.minisig", sig, err)) {
        diag.error("catalog", err);
        return false;
    }
    out = std::move(fresh);
    return true;
}

// --------------------------------------------------------------------- state

bool PackStore::read_state(State& st, Diagnostics* diag) const {
    st = State();
    std::string text;
    if (!read_whole_file(dir_ + "/installed.json", text)) return true;  // nothing installed yet
    try {
        Json doc = Json::parse(text);
        auto load = [&](const char* key, std::map<std::string, InstallRecord>& dst) {
            const Json* m = doc.find(key);
            if (!m || !m->is_object()) return;
            for (size_t i = 0; i < m->keys().size(); ++i) {
                const Json& r = m->value_at(i);
                InstallRecord rec;
                rec.id = m->keys()[i];
                const Json* f = r.find("file");
                const Json* h = r.find("sha256");
                const Json* v = r.find("revision");
                if (!f || !h || !v || !f->is_string() || !h->is_string() || !v->is_number() || !is_safe_pack_path(f->str())) {
                    throw JsonError("bad record for '" + rec.id + "'");
                }
                rec.file = f->str();
                rec.sha256 = h->str();
                rec.revision = std::atoi(v->str().c_str());
                dst[rec.id] = rec;
            }
        };
        load("installed", st.installed);
        load("previous", st.previous);
    } catch (const JsonError& e) {
        if (diag) diag->error("installed.json", std::string("unreadable: ") + e.what());
        st = State();
        return false;
    }
    return true;
}

bool PackStore::write_state(const State& st, Diagnostics& diag) const {
    auto dump = [](const std::map<std::string, InstallRecord>& m) {
        Json o = Json::object();
        for (const auto& kv : m) {
            Json r = Json::object();
            r.set("revision", Json::number_text(std::to_string(kv.second.revision)));
            r.set("file", Json::string(kv.second.file));
            r.set("sha256", Json::string(kv.second.sha256));
            o.set(kv.first, r);
        }
        return o;
    };
    Json doc = Json::object();
    doc.set("state", Json::number_text("1"));
    doc.set("installed", dump(st.installed));
    doc.set("previous", dump(st.previous));
    std::string err;
    if (!write_file_atomic(dir_ + "/installed.json", doc.dump(2) + "\n", err)) {
        diag.error("installed.json", err);
        return false;
    }
    return true;
}

std::map<std::string, InstallRecord> PackStore::installed() const {
    State st;
    read_state(st, nullptr);
    return st.installed;
}

std::map<std::string, InstallRecord> PackStore::previous() const {
    State st;
    read_state(st, nullptr);
    return st.previous;
}

std::vector<std::string> PackStore::updates_available(const Catalog& catalog) const {
    std::vector<std::string> out;
    for (const auto& kv : installed()) {
        const CatalogEntry* e = catalog.find(kv.first);
        if (e && e->sha256 != kv.second.sha256) out.push_back(kv.first);
    }
    return out;
}

bool PackStore::read_cached_pack(const InstallRecord& rec, std::string& text, Diagnostics& diag) const {
    if (!read_whole_file(dir_ + "/" + rec.file, text)) {
        diag.error(rec.id, "cached pack file is missing (" + rec.file + "); reinstall it");
        return false;
    }
    if (sha256_hex(text) != rec.sha256) {
        diag.error(rec.id, "cached pack file does not match its recorded hash (corrupted or altered); reinstall it");
        return false;
    }
    return true;
}

// ------------------------------------------------------------------- install

bool PackStore::install(const Catalog& catalog, const std::string& pack_id, Fetcher& fetcher, Diagnostics& diag) {
    const size_t before = diag.error_count();
    State st;
    if (!read_state(st, &diag)) return false;

    std::vector<const CatalogEntry*> plan;
    if (!plan_install(catalog, pack_id, st.installed, plan, diag)) return false;
    if (plan.empty()) return true;  // already up to date

    // 1. Download and verify everything in memory first.
    struct Staged {
        const CatalogEntry* entry;
        std::string text;
        Json doc;
    };
    std::vector<Staged> staged;
    for (const CatalogEntry* e : plan) {
        Staged s;
        s.entry = e;
        std::string err;
        if (!fetcher.get(e->file, s.text, err)) {
            diag.error(e->id, "download failed: " + err);
            return false;
        }
        if (s.text.size() != e->size) {
            diag.error(e->id, "size mismatch: catalog says " + std::to_string(e->size) + " bytes, got " + std::to_string(s.text.size()));
            return false;
        }
        if (sha256_hex(s.text) != e->sha256) {
            diag.error(e->id, "hash mismatch: the downloaded file does not match the signed catalog; refusing it");
            return false;
        }
        try {
            s.doc = Json::parse(s.text);
        } catch (const JsonError& ex) {
            diag.error(e->id, ex.what());
            return false;
        }
        staged.push_back(std::move(s));
    }

    // 2. Validate the resulting set as a whole (already-installed packs plus the new ones).
    PackSet set;
    std::set<std::string> staged_ids;
    for (const auto& s : staged) staged_ids.insert(s.entry->id);
    for (const auto& kv : st.installed) {
        if (staged_ids.count(kv.first)) continue;
        std::string text;
        if (!read_cached_pack(kv.second, text, diag)) return false;
        try {
            set.add(Json::parse(text), kv.first, diag);
        } catch (const JsonError& ex) {
            diag.error(kv.first, ex.what());
            return false;
        }
    }
    for (const auto& s : staged) {
        Diagnostics local;
        Pack probe;
        if (!load_pack(s.doc, s.entry->id, probe, local)) {
            for (const auto& d : local.items()) diag.error(d.where, d.message);
            return false;
        }
        if (probe.id != s.entry->id || probe.revision != s.entry->revision) {
            diag.error(s.entry->id, "pack contents (" + probe.id + " r" + std::to_string(probe.revision) +
                                        ") do not match the catalog entry (" + s.entry->id + " r" +
                                        std::to_string(s.entry->revision) + ")");
            return false;
        }
        set.add(s.doc, s.entry->id, diag);
    }
    for (const auto& id : set.ids()) {
        Diagnostics local;
        if (!set.bundle_for(id, local)) {
            for (const auto& d : local.items()) {
                if (d.severity == Severity::Error) diag.error(d.where, d.message);
            }
            return false;
        }
    }
    if (diag.error_count() > before) return false;

    // 3. Commit: files first, then the state that points at them.
    std::string err;
    for (const auto& s : staged) {
        if (!write_file_atomic(dir_ + "/" + s.entry->file, s.text, err)) {
            diag.error(s.entry->id, err);
            return false;
        }
    }
    State next = st;
    for (const auto& s : staged) {
        const std::string& id = s.entry->id;
        InstallRecord rec{id, s.entry->revision, s.entry->file, s.entry->sha256};
        auto cur = next.installed.find(id);
        if (cur != next.installed.end()) {
            // Keep exactly one previous revision on disk.
            auto old_prev = next.previous.find(id);
            if (old_prev != next.previous.end() && old_prev->second.file != cur->second.file &&
                old_prev->second.file != rec.file) {
                std::error_code ec;
                fs::remove(fs::u8path(dir_ + "/" + old_prev->second.file), ec);
            }
            next.previous[id] = cur->second;
        }
        next.installed[id] = rec;
    }
    return write_state(next, diag);
}

bool PackStore::rollback(const std::string& pack_id, Diagnostics& diag) {
    State st;
    if (!read_state(st, &diag)) return false;
    auto prev = st.previous.find(pack_id);
    auto cur = st.installed.find(pack_id);
    if (prev == st.previous.end() || cur == st.installed.end()) {
        diag.error(pack_id, "there is no previous revision to go back to");
        return false;
    }
    std::string text;
    if (!read_cached_pack(prev->second, text, diag)) return false;
    std::swap(prev->second, cur->second);
    return write_state(st, diag);
}

bool PackStore::load_installed(PackSet& set, Diagnostics& diag) const {
    State st;
    if (!read_state(st, &diag)) return false;
    bool ok = true;
    for (const auto& kv : st.installed) {
        std::string text;
        if (!read_cached_pack(kv.second, text, diag)) {
            ok = false;
            continue;
        }
        try {
            if (!set.add(Json::parse(text), kv.first, diag)) ok = false;
        } catch (const JsonError& ex) {
            diag.error(kv.first, ex.what());
            ok = false;
        }
    }
    return ok;
}

}  // namespace jobeval
