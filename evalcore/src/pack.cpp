#include "evalcore/pack.hpp"

#include <algorithm>
#include <functional>
#include <set>

namespace jobeval {

// ---------------------------------------------------------------- Diagnostics

bool Diagnostics::has_errors() const { return error_count() > 0; }

size_t Diagnostics::error_count() const {
    return static_cast<size_t>(std::count_if(items_.begin(), items_.end(),
                                             [](const Diagnostic& d) { return d.severity == Severity::Error; }));
}
size_t Diagnostics::warning_count() const { return items_.size() - error_count(); }

std::string Diagnostics::format() const {
    std::string out;
    for (const auto& d : items_) {
        out += d.severity == Severity::Error ? "error: " : "warning: ";
        if (!d.where.empty()) out += d.where + ": ";
        out += d.message + "\n";
    }
    return out;
}

// -------------------------------------------------------------------- helpers

bool is_iso_date(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (s[i] < '0' || s[i] > '9') return false;
    }
    int m = std::stoi(s.substr(5, 2));
    int d = std::stoi(s.substr(8, 2));
    return m >= 1 && m <= 12 && d >= 1 && d <= 31;
}

namespace {

const char* const kTiers[] = {"estimate", "partial", "full"};

int tier_rank(const std::string& t) {
    for (int i = 0; i < 3; ++i) {
        if (t == kTiers[i]) return i;
    }
    return -1;
}

bool is_official(const std::string& authority) {
    return authority == "official" || authority == "official_secondary";
}

// Returns the string at obj[key], or "" (reporting if required).
std::string get_str(const Json& obj, const std::string& key, bool required, const std::string& where,
                    Diagnostics& diag) {
    const Json* v = obj.find(key);
    if (!v) {
        if (required) diag.error(where, "missing required field '" + key + "'");
        return "";
    }
    if (!v->is_string()) {
        diag.error(where, "field '" + key + "' must be a string");
        return "";
    }
    return v->str();
}

std::vector<std::string> get_str_list(const Json& v, const std::string& where, Diagnostics& diag) {
    std::vector<std::string> out;
    if (!v.is_array()) {
        diag.error(where, "must be a list of strings");
        return out;
    }
    for (const auto& item : v.items()) {
        if (!item.is_string()) {
            diag.error(where, "must be a list of strings");
            return {};
        }
        out.push_back(item.str());
    }
    return out;
}

// "1.0" / ">=1.2" -> major/minor. Only ">=" constraints are supported.
bool engine_satisfies(const std::string& constraint, std::string& why) {
    std::string c = constraint;
    if (c.rfind(">=", 0) == 0) c = c.substr(2);
    else if (!c.empty() && std::isdigit(static_cast<unsigned char>(c[0]))) { /* bare version means >= */ }
    else {
        why = "unsupported requires_engine constraint '" + constraint + "' (use \">=MAJOR.MINOR\")";
        return false;
    }
    size_t dot = c.find('.');
    int major = 0, minor = 0;
    try {
        major = std::stoi(c.substr(0, dot));
        minor = dot == std::string::npos ? 0 : std::stoi(c.substr(dot + 1));
    } catch (...) {
        why = "malformed requires_engine version '" + constraint + "'";
        return false;
    }
    if (major > kEngineMajor || (major == kEngineMajor && minor > kEngineMinor)) {
        why = "pack requires engine " + c + " but this is " + std::to_string(kEngineMajor) + "." +
              std::to_string(kEngineMinor) + " (update the app to use this pack)";
        return false;
    }
    return true;
}

}  // namespace

bool engine_supports(const std::string& constraint, std::string& why) { return engine_satisfies(constraint, why); }

// ------------------------------------------------------------------ load_pack

bool load_pack(const Json& doc, const std::string& origin, Pack& out, Diagnostics& diag) {
    const size_t errors_before = diag.error_count();
    if (!doc.is_object()) {
        diag.error(origin, "pack document must be a JSON object");
        return false;
    }
    Pack p;
    const Json* schema = doc.find("schema");
    if (!schema || !schema->is_number() || schema->str() != std::to_string(kPackSchema)) {
        diag.error(origin, "unsupported or missing 'schema' (expected " + std::to_string(kPackSchema) + ")");
        return false;
    }
    p.id = get_str(doc, "id", true, origin, diag);
    const std::string where = p.id.empty() ? origin : p.id;
    p.kind = get_str(doc, "kind", true, where, diag);
    if (!p.kind.empty() && p.kind != "country" && p.kind != "subregion" && p.kind != "locality") {
        diag.error(where, "kind must be country, subregion or locality");
    }
    p.extends = get_str(doc, "extends", false, where, diag);
    if (p.kind != "country" && p.extends.empty() && !p.kind.empty()) {
        diag.error(where, "a " + p.kind + " pack must declare 'extends'");
    }
    p.tier = get_str(doc, "tier", true, where, diag);
    if (!p.tier.empty() && tier_rank(p.tier) < 0) diag.error(where, "tier must be full, partial or estimate");
    p.currency = get_str(doc, "currency", p.kind == "country", where, diag);

    if (const Json* rev = doc.find("revision")) {
        if (rev->is_number()) p.revision = std::atoi(rev->str().c_str());
        else diag.error(where, "revision must be a number");
    } else {
        diag.error(where, "missing required field 'revision'");
    }

    if (const Json* region = doc.find("region")) {
        if (region->is_object()) {
            p.country = get_str(*region, "country", true, where + ".region", diag);
            p.subregion = get_str(*region, "subregion", false, where + ".region", diag);
        } else {
            diag.error(where, "region must be an object");
        }
    } else {
        diag.error(where, "missing required field 'region'");
    }

    if (const Json* valid = doc.find("valid")) {
        if (valid->is_object()) {
            p.valid_from = get_str(*valid, "from", true, where + ".valid", diag);
            p.valid_to = get_str(*valid, "to", false, where + ".valid", diag);
            if (!p.valid_from.empty() && !is_iso_date(p.valid_from)) diag.error(where, "valid.from is not YYYY-MM-DD");
            if (!p.valid_to.empty() && !is_iso_date(p.valid_to)) diag.error(where, "valid.to is not YYYY-MM-DD");
            if (is_iso_date(p.valid_from) && is_iso_date(p.valid_to) && p.valid_to < p.valid_from) {
                diag.error(where, "valid.to is before valid.from");
            }
        } else {
            diag.error(where, "valid must be an object");
        }
    } else {
        diag.error(where, "missing required field 'valid'");
    }

    if (const Json* req = doc.find("requires_engine")) {
        std::string why;
        if (!req->is_string()) diag.error(where, "requires_engine must be a string");
        else if (!engine_satisfies(req->str(), why)) diag.error(where, why);
    }

    if (const Json* labels = doc.find("labels")) {
        if (labels->is_object()) {
            for (size_t i = 0; i < labels->keys().size(); ++i) {
                if (labels->value_at(i).is_string()) p.labels[labels->keys()[i]] = labels->value_at(i).str();
            }
        }
    }

    // sources
    if (const Json* sources = doc.find("sources")) {
        if (!sources->is_object()) {
            diag.error(where, "sources must be an object");
        } else {
            for (size_t i = 0; i < sources->keys().size(); ++i) {
                const std::string sid = sources->keys()[i];
                const Json& s = sources->value_at(i);
                const std::string sw = where + ".sources." + sid;
                if (!s.is_object()) {
                    diag.error(sw, "must be an object");
                    continue;
                }
                SourceDef def;
                def.id = sid;
                def.authority = get_str(s, "authority", true, sw, diag);
                if (!def.authority.empty() && def.authority != "official" && def.authority != "official_secondary" &&
                    def.authority != "third_party") {
                    diag.error(sw, "authority must be official, official_secondary or third_party");
                }
                def.title = get_str(s, "title", false, sw, diag);
                def.url = get_str(s, "url", false, sw, diag);
                def.retrieved = get_str(s, "retrieved", false, sw, diag);
                if (def.title.empty() && def.url.empty()) diag.error(sw, "needs a title or a url");
                p.sources[sid] = def;
            }
        }
    }

    // parameters
    if (const Json* params = doc.find("parameters")) {
        if (!params->is_object()) {
            diag.error(where, "parameters must be an object");
        } else {
            for (size_t i = 0; i < params->keys().size(); ++i) {
                const std::string pid = params->keys()[i];
                const Json& pj = params->value_at(i);
                const std::string pw = where + ".parameters." + pid;
                if (!pj.is_object()) {
                    diag.error(pw, "must be an object with 'source' and 'value' (or 'effective')");
                    continue;
                }
                ParamDef def;
                def.id = pid;
                def.pack = p.id;
                def.source = get_str(pj, "source", false, pw, diag);
                const Json* value = pj.find("value");
                const Json* eff = pj.find("effective");
                if ((value != nullptr) == (eff != nullptr)) {
                    diag.error(pw, "needs exactly one of 'value' or 'effective'");
                    continue;
                }
                if (value) {
                    def.value = *value;
                    if (def.source.empty()) diag.error(pw, "missing required field 'source'");
                } else {
                    def.dated = true;
                    if (!eff->is_array() || eff->items().empty()) {
                        diag.error(pw, "'effective' must be a non-empty list");
                        continue;
                    }
                    std::string prev;
                    for (const auto& e : eff->items()) {
                        if (!e.is_object()) {
                            diag.error(pw, "each effective entry must be an object");
                            continue;
                        }
                        EffectiveValue ev;
                        ev.from = get_str(e, "from", true, pw, diag);
                        if (!ev.from.empty() && !is_iso_date(ev.from)) diag.error(pw, "'from' is not YYYY-MM-DD");
                        if (const Json* v = e.find("value")) ev.value = *v;
                        else diag.error(pw, "effective entry missing 'value'");
                        ev.source = get_str(e, "source", false, pw, diag);
                        if (ev.source.empty()) ev.source = def.source;
                        if (ev.source.empty()) diag.error(pw, "effective entry for " + ev.from + " has no source");
                        if (!prev.empty() && ev.from <= prev) {
                            diag.error(pw, "effective entries must be in strictly ascending 'from' order");
                        }
                        prev = ev.from;
                        def.history.push_back(std::move(ev));
                    }
                }
                p.params.push_back(std::move(def));
            }
        }
    }

    // inputs
    if (const Json* inputs = doc.find("inputs")) {
        if (!inputs->is_object()) {
            diag.error(where, "inputs must be an object");
        } else {
            for (size_t i = 0; i < inputs->keys().size(); ++i) {
                const std::string name = inputs->keys()[i];
                const Json& ij = inputs->value_at(i);
                const std::string iw = where + ".inputs." + name;
                if (!ij.is_object()) {
                    diag.error(iw, "must be an object");
                    continue;
                }
                InputDef in;
                in.name = name;
                in.pack = p.id;
                in.type = get_str(ij, "type", true, iw, diag);
                in.label = get_str(ij, "label", false, iw, diag);
                if (const Json* d = ij.find("default")) {
                    std::string err;
                    if (d->is_number() && Decimal::try_parse(d->str(), in.default_value, &err)) in.has_default = true;
                    else diag.error(iw, "default must be a number");
                }
                p.inputs.push_back(std::move(in));
            }
        }
    }

    // rules
    if (const Json* rules = doc.find("rules")) {
        if (!rules->is_array()) {
            diag.error(where, "rules must be a list");
        } else {
            std::set<std::string> seen;
            for (const auto& rj : rules->items()) {
                if (!rj.is_object()) {
                    diag.error(where, "each rule must be an object");
                    continue;
                }
                Rule r;
                r.pack = p.id;
                r.id = get_str(rj, "id", true, where + ".rules", diag);
                const std::string rw = where + ".rules[" + r.id + "]";
                if (!r.id.empty() && !seen.insert(r.id).second) diag.error(rw, "duplicate rule id in this pack");
                if (const Json* label = rj.find("label")) {
                    if (label->is_object()) {
                        if (const Json* en = label->find("en")) {
                            if (en->is_string()) r.label = en->str();
                        }
                    } else if (label->is_string()) {
                        r.label = label->str();
                    }
                }
                r.note = get_str(rj, "note", false, rw, diag);
                if (const Json* pub = rj.find("public")) {
                    if (pub->is_bool()) r.is_public = pub->as_bool();
                    else diag.error(rw, "'public' must be true or false");
                }
                const std::string text = get_str(rj, "expr", true, rw, diag);
                if (!text.empty()) {
                    try {
                        r.expr = parse_expr(text);
                    } catch (const ParseError& e) {
                        diag.error(rw, std::string("expression error: ") + e.what() + "  in: " + text);
                    }
                }
                p.rules.push_back(std::move(r));
            }
        }
    }

    auto read_slots = [&](const char* key, std::vector<std::pair<std::string, std::vector<std::string>>>& dst) {
        if (const Json* o = doc.find(key)) {
            if (!o->is_object()) {
                diag.error(where, std::string(key) + " must be an object of slot -> [rule ids]");
                return;
            }
            for (size_t i = 0; i < o->keys().size(); ++i) {
                dst.emplace_back(o->keys()[i], get_str_list(o->value_at(i), where + "." + key + "." + o->keys()[i], diag));
            }
        }
    };
    read_slots("outputs", p.outputs);
    read_slots("append", p.appends);

    if (const Json* st = doc.find("statutory")) {
        if (!st->is_object()) {
            diag.error(where, "statutory must be an object of name -> expression");
        } else {
            for (size_t i = 0; i < st->keys().size(); ++i) {
                const Json& v = st->value_at(i);
                std::string text;
                if (v.is_string()) text = v.str();
                else if (v.is_number()) text = v.str();
                else diag.error(where + ".statutory." + st->keys()[i], "must be an expression string or number");
                p.statutory.emplace_back(st->keys()[i], text);
            }
        }
    }

    if (const Json* r = doc.find("replace")) p.replace = get_str_list(*r, where + ".replace", diag);
    if (const Json* nm = doc.find("not_modelled")) p.not_modelled = get_str_list(*nm, where + ".not_modelled", diag);
    if (const Json* prog = doc.find("programs")) p.programs = *prog;

    if (diag.error_count() > errors_before) return false;
    out = std::move(p);
    return true;
}

// -------------------------------------------------------------------- PackSet

bool PackSet::add(const Json& doc, const std::string& origin, Diagnostics& diag) {
    Pack p;
    if (!load_pack(doc, origin, p, diag)) return false;
    if (packs_.count(p.id)) {
        diag.error(origin, "duplicate pack id '" + p.id + "'");
        return false;
    }
    packs_[p.id] = std::move(p);
    return true;
}

const Pack* PackSet::find(const std::string& id) const {
    auto it = packs_.find(id);
    return it == packs_.end() ? nullptr : &it->second;
}

std::vector<std::string> PackSet::ids() const {
    std::vector<std::string> v;
    for (const auto& kv : packs_) v.push_back(kv.first);
    return v;
}

bool split_param_path(const std::map<std::string, ParamDef>& params, const std::string& path,
                      std::string& param_id, std::vector<std::string>& fields) {
    std::string prefix = path;
    std::string rest;
    while (true) {
        if (params.count(prefix)) {
            param_id = prefix;
            fields.clear();
            size_t start = 0;
            while (start < rest.size()) {
                size_t dot = rest.find('.', start);
                if (dot == std::string::npos) dot = rest.size();
                fields.push_back(rest.substr(start, dot - start));
                start = dot + 1;
            }
            return true;
        }
        size_t dot = prefix.rfind('.');
        if (dot == std::string::npos) return false;
        rest = prefix.substr(dot + 1) + (rest.empty() ? "" : "." + rest);
        prefix = prefix.substr(0, dot);
    }
}

const Json& param_value_on(const ParamDef& p, const std::string& date) {
    if (!p.dated) return p.value;
    const EffectiveValue* best = nullptr;
    for (const auto& e : p.history) {
        if (e.from <= date) best = &e;
    }
    if (!best) {
        throw EvalError("parameter '" + p.id + "' has no value in effect on " + date + " (earliest is " +
                        p.history.front().from + ")");
    }
    return best->value;
}

namespace {

// Walks `fields` into a Json value; returns null if the path does not exist.
const Json* walk_fields(const Json& root, const std::vector<std::string>& fields) {
    const Json* cur = &root;
    for (const auto& f : fields) {
        if (!cur->is_object()) return nullptr;
        cur = cur->find(f);
        if (!cur) return nullptr;
    }
    return cur;
}

}  // namespace

std::shared_ptr<const Bundle> PackSet::bundle_for(const std::string& id, Diagnostics& diag) const {
    const size_t errors_before = diag.error_count();

    // Resolve the extends chain, parent first.
    std::vector<const Pack*> chain;
    {
        std::set<std::string> seen;
        const Pack* cur = find(id);
        if (!cur) {
            diag.error(id, "no such pack");
            return nullptr;
        }
        while (cur) {
            if (!seen.insert(cur->id).second) {
                diag.error(id, "extends chain loops back to '" + cur->id + "'");
                return nullptr;
            }
            chain.push_back(cur);
            if (cur->extends.empty()) break;
            const Pack* parent = find(cur->extends);
            if (!parent) {
                diag.error(cur->id, "extends '" + cur->extends + "' which is not loaded");
                return nullptr;
            }
            cur = parent;
        }
        std::reverse(chain.begin(), chain.end());
    }

    auto b = std::make_shared<Bundle>();
    int weakest = 2;
    for (const Pack* p : chain) {
        b->chain.push_back(p->id);
        weakest = std::min(weakest, tier_rank(p->tier));
        if (b->currency.empty()) b->currency = p->currency;
        else if (!p->currency.empty() && p->currency != b->currency) {
            diag.error(p->id, "currency " + p->currency + " differs from parent's " + b->currency);
        }
        if (b->valid_from.empty() || p->valid_from > b->valid_from) b->valid_from = p->valid_from;
        if (!p->valid_to.empty() && (b->valid_to.empty() || p->valid_to < b->valid_to)) b->valid_to = p->valid_to;
        for (const auto& nm : p->not_modelled) b->not_modelled.push_back(p->id + ": " + nm);

        const std::set<std::string> replace(p->replace.begin(), p->replace.end());
        std::set<std::string> replaced_used;
        auto allow = [&](bool exists, const std::string& what, const std::string& name) {
            if (!exists) return true;
            if (replace.count(name)) {
                replaced_used.insert(name);
                return true;
            }
            diag.error(p->id, what + " '" + name + "' is already defined by a parent pack (list it under `replace` to override it deliberately)");
            return false;
        };

        for (const auto& kv : p->sources) {
            if (b->sources.count(kv.first)) diag.error(p->id, "source id '" + kv.first + "' collides with a parent pack's");
            else b->sources[kv.first] = kv.second;
        }
        for (const auto& pd : p->params) {
            if (allow(b->params.count(pd.id) > 0, "parameter", pd.id)) b->params[pd.id] = pd;
        }
        for (const auto& in : p->inputs) {
            if (allow(b->inputs.count(in.name) > 0, "input", in.name)) b->inputs[in.name] = in;
        }
        auto add_rule = [&](const Rule& r) {
            const bool exists = b->rules.count(r.id) > 0;
            if (allow(exists, "rule", r.id)) {
                if (!exists) b->rule_order.push_back(r.id);
                b->rules[r.id] = r;
            }
        };
        for (const auto& r : p->rules) add_rule(r);
        for (const auto& st : p->statutory) {
            Rule r;
            r.id = "statutory." + st.first;
            r.pack = p->id;
            r.label = st.first;
            try {
                r.expr = parse_expr(st.second);
            } catch (const ParseError& e) {
                diag.error(p->id + ".statutory." + st.first, std::string("expression error: ") + e.what());
                continue;
            }
            add_rule(r);
        }
        for (const auto& o : p->outputs) {
            if (allow(b->slots.count(o.first) > 0, "output slot", o.first)) b->slots[o.first] = o.second;
        }
        for (const auto& a : p->appends) {
            auto& list = b->slots[a.first];
            list.insert(list.end(), a.second.begin(), a.second.end());
        }
        for (const auto& name : p->replace) {
            if (!replaced_used.count(name)) {
                diag.warn(p->id, "`replace` lists '" + name + "' which does not override anything");
            }
        }
    }
    b->tier = kTiers[weakest];

    for (const auto& rid : b->rule_order) {
        if (b->inputs.count(rid)) {
            diag.error(b->rules[rid].pack + ".rules[" + rid + "]", "rule id collides with an input of the same name");
        }
    }

    // Reference resolution.
    std::map<std::string, std::set<std::string>> deps;  // rule -> rules it depends on
    std::set<std::string> referenced_rules, referenced_params;
    for (const auto& rid : b->rule_order) {
        const Rule& r = b->rules[rid];
        const std::string rw = r.pack + ".rules[" + rid + "]";
        Refs refs;
        collect_refs(*r.expr->root, refs);
        for (const auto& name : refs.rules) {
            if (b->rules.count(name)) {
                deps[rid].insert(name);
                referenced_rules.insert(name);
            } else if (!b->inputs.count(name)) {
                diag.error(rw, "'" + name + "' is neither a rule nor a declared input");
            }
        }
        for (const auto& path : refs.params) {
            std::string pid;
            std::vector<std::string> fields;
            if (!split_param_path(b->params, path, pid, fields)) {
                diag.error(rw, "unknown parameter '$" + path + "'");
                continue;
            }
            referenced_params.insert(pid);
            const ParamDef& pd = b->params[pid];
            if (!fields.empty() && !pd.dated) {
                if (!walk_fields(pd.value, fields)) diag.error(rw, "parameter '$" + path + "' has no such field");
            }
        }
        for (const auto& slot : refs.slots) {
            auto it = b->slots.find(slot);
            if (it == b->slots.end()) {
                diag.error(rw, "unknown output slot '@" + slot + "'");
                continue;
            }
            for (const auto& member : it->second) {
                if (b->rules.count(member)) deps[rid].insert(member);
            }
        }
    }
    for (const auto& slot : b->slots) {
        for (const auto& member : slot.second) {
            if (!b->rules.count(member)) diag.error("slot " + slot.first, "lists unknown rule '" + member + "'");
            referenced_rules.insert(member);
        }
    }

    // Cycle detection (iterative DFS would be overkill: depth is tiny).
    {
        std::map<std::string, int> color;  // 0 new, 1 visiting, 2 done
        std::vector<std::string> stack;
        std::function<bool(const std::string&)> visit = [&](const std::string& n) -> bool {
            color[n] = 1;
            stack.push_back(n);
            for (const auto& d : deps[n]) {
                if (color[d] == 1) {
                    std::string cyc;
                    auto it = std::find(stack.begin(), stack.end(), d);
                    for (; it != stack.end(); ++it) cyc += *it + " -> ";
                    diag.error("rules", "dependency cycle: " + cyc + d);
                    return false;
                }
                if (color[d] == 0 && !visit(d)) return false;
            }
            stack.pop_back();
            color[n] = 2;
            return true;
        };
        for (const auto& rid : b->rule_order) {
            if (color[rid] == 0 && !visit(rid)) break;
        }
    }

    // Hygiene warnings.
    for (const auto& rid : b->rule_order) {
        const Rule& r = b->rules[rid];
        if (!referenced_rules.count(rid) && !r.is_public && rid.rfind("statutory.", 0) != 0) {
            diag.warn(r.pack + ".rules[" + rid + "]", "rule is not used by any slot or other rule");
        }
    }
    for (const auto& kv : b->params) {
        if (!referenced_params.count(kv.first)) diag.warn(kv.second.pack + ".parameters." + kv.first, "parameter is never referenced");
    }

    // Source references and tier gating.
    for (const auto& kv : b->params) {
        const ParamDef& pd = kv.second;
        auto check_source = [&](const std::string& sid) {
            if (!b->sources.count(sid)) diag.error(pd.pack + ".parameters." + pd.id, "unknown source '" + sid + "'");
        };
        if (pd.dated) {
            for (const auto& e : pd.history) check_source(e.source);
        } else {
            check_source(pd.source);
        }
    }
    for (const Pack* p : chain) {
        if (p->tier != "full") continue;
        if (!p->not_modelled.empty()) {
            diag.error(p->id, "tier 'full' is not allowed while `not_modelled` is non-empty");
        }
        for (const auto& pd : p->params) {
            std::vector<std::string> srcs;
            if (pd.dated) for (const auto& e : pd.history) srcs.push_back(e.source);
            else srcs.push_back(pd.source);
            for (const auto& sid : srcs) {
                auto it = b->sources.find(sid);
                if (it != b->sources.end() && !is_official(it->second.authority)) {
                    diag.error(p->id + ".parameters." + pd.id,
                               "tier 'full' requires official sources, but '" + sid + "' is " + it->second.authority);
                }
            }
        }
    }

    if (diag.error_count() > errors_before) return nullptr;
    return b;
}

}  // namespace jobeval
