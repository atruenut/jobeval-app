#include "evalcore/catalog.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace jobeval {

const CatalogEntry* Catalog::find(const std::string& id) const {
    for (const auto& p : packs) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

bool is_safe_pack_path(const std::string& path) {
    static const std::string prefix = "packs/";
    static const std::string suffix = ".json";
    if (path.size() <= prefix.size() + suffix.size()) return false;
    if (path.compare(0, prefix.size(), prefix) != 0) return false;
    if (path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
    const std::string name = path.substr(prefix.size());
    if (name[0] == '.') return false;
    for (char c : name) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-')) return false;
    }
    return name.find("..") == std::string::npos;
}

namespace {

bool is_hex64(const std::string& s) {
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

std::string str_field(const Json& o, const char* key) {
    const Json* v = o.find(key);
    return v && v->is_string() ? v->str() : std::string();
}

}  // namespace

bool parse_catalog(const std::string& text, Catalog& out, Diagnostics& diag) {
    const size_t before = diag.error_count();
    Json doc;
    try {
        doc = Json::parse(text);
    } catch (const JsonError& e) {
        diag.error("catalog", e.what());
        return false;
    }
    if (!doc.is_object()) {
        diag.error("catalog", "must be a JSON object");
        return false;
    }
    Catalog c;
    const Json* ver = doc.find("catalog");
    if (!ver || !ver->is_number() || ver->str() != "1") {
        diag.error("catalog", "unsupported catalog version (this app understands version 1; update the app)");
        return false;
    }
    c.version = 1;
    c.generated = str_field(doc, "generated");
    if (c.generated.empty()) diag.error("catalog", "missing 'generated' timestamp");
    const Json* packs = doc.find("packs");
    if (!packs || !packs->is_array()) {
        diag.error("catalog", "missing 'packs' list");
        return false;
    }
    std::set<std::string> ids;
    for (const auto& pj : packs->items()) {
        CatalogEntry e;
        e.id = str_field(pj, "id");
        const std::string where = "catalog." + (e.id.empty() ? std::string("(pack)") : e.id);
        if (e.id.empty()) {
            diag.error(where, "entry without id");
            continue;
        }
        if (!ids.insert(e.id).second) diag.error(where, "duplicate id");
        e.kind = str_field(pj, "kind");
        e.extends = str_field(pj, "extends");
        e.country = str_field(pj, "country");
        e.subregion = str_field(pj, "subregion");
        e.tier = str_field(pj, "tier");
        e.valid_from = str_field(pj, "valid_from");
        e.valid_to = str_field(pj, "valid_to");
        e.requires_engine = str_field(pj, "requires_engine");
        e.last_verified = str_field(pj, "last_verified");
        e.file = str_field(pj, "file");
        e.sha256 = str_field(pj, "sha256");
        if (const Json* lab = pj.find("labels")) {
            if (lab->is_object()) {
                for (size_t i = 0; i < lab->keys().size(); ++i) {
                    if (lab->value_at(i).is_string()) e.labels[lab->keys()[i]] = lab->value_at(i).str();
                }
            }
        }
        if (const Json* rev = pj.find("revision")) {
            if (rev->is_number()) e.revision = std::atoi(rev->str().c_str());
        }
        if (e.revision <= 0) diag.error(where, "bad revision");
        if (const Json* sz = pj.find("size")) {
            if (sz->is_number()) e.size = static_cast<size_t>(std::strtoull(sz->str().c_str(), nullptr, 10));
        }
        if (e.size == 0 || e.size > 16u * 1024u * 1024u) diag.error(where, "bad size");
        if (!is_safe_pack_path(e.file)) diag.error(where, "unsafe or malformed file path '" + e.file + "'");
        if (!is_hex64(e.sha256)) diag.error(where, "sha256 must be 64 lowercase hex digits");
        if (e.tier != "estimate" && e.tier != "partial" && e.tier != "full") diag.error(where, "bad tier");
        if (e.kind != "country" && e.extends.empty() == false && e.extends == e.id) diag.error(where, "extends itself");
        c.packs.push_back(std::move(e));
    }
    if (diag.error_count() > before) return false;
    out = std::move(c);
    return true;
}

bool plan_install(const Catalog& catalog, const std::string& pack_id,
                  const std::map<std::string, InstallRecord>& installed,
                  std::vector<const CatalogEntry*>& plan, Diagnostics& diag) {
    const size_t before = diag.error_count();
    std::vector<const CatalogEntry*> chain;
    const CatalogEntry* cur = catalog.find(pack_id);
    if (!cur) {
        diag.error(pack_id, "this pack is not in the catalog");
        return false;
    }
    while (cur) {
        if (chain.size() > 16) {
            diag.error(pack_id, "extends chain is too long (or loops)");
            return false;
        }
        std::string why;
        if (!cur->requires_engine.empty() && !engine_supports(cur->requires_engine, why)) {
            diag.error(cur->id, why);
        }
        chain.push_back(cur);
        if (cur->extends.empty()) break;
        const CatalogEntry* parent = catalog.find(cur->extends);
        if (!parent) {
            diag.error(cur->id, "needs '" + cur->extends + "' which is not in the catalog");
            return false;
        }
        cur = parent;
    }
    if (diag.error_count() > before) return false;
    std::reverse(chain.begin(), chain.end());
    plan.clear();
    for (const CatalogEntry* e : chain) {
        auto it = installed.find(e->id);
        if (it != installed.end() && it->second.sha256 == e->sha256) continue;
        plan.push_back(e);
    }
    return true;
}

}  // namespace jobeval
