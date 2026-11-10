#include "evalcore/json.hpp"

#include <cctype>
#include <cstdio>

namespace jobeval {

Json Json::boolean(bool b) {
    Json j;
    j.type_ = Type::Bool;
    j.b_ = b;
    return j;
}
Json Json::number_text(std::string text) {
    Json j;
    j.type_ = Type::Number;
    j.s_ = std::move(text);
    return j;
}
Json Json::string(std::string s) {
    Json j;
    j.type_ = Type::String;
    j.s_ = std::move(s);
    return j;
}
Json Json::array() {
    Json j;
    j.type_ = Type::Array;
    return j;
}
Json Json::object() {
    Json j;
    j.type_ = Type::Object;
    return j;
}

void Json::push(Json v) {
    if (type_ != Type::Array) throw JsonError("push on non-array");
    vals_.push_back(std::move(v));
}

const Json* Json::find(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    for (size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i] == key) return &vals_[i];
    }
    return nullptr;
}

void Json::set(const std::string& key, Json v) {
    if (type_ != Type::Object) throw JsonError("set on non-object");
    for (size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i] == key) {
            vals_[i] = std::move(v);
            return;
        }
    }
    keys_.push_back(key);
    vals_.push_back(std::move(v));
}

namespace {

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    Json parse_document() {
        ws();
        Json v = value();
        ws();
        if (i_ != s_.size()) fail("unexpected trailing content");
        return v;
    }

private:
    const std::string& s_;
    size_t i_ = 0;
    int depth_ = 0;

    [[noreturn]] void fail(const std::string& msg) const {
        int line = 1, col = 1;
        for (size_t k = 0; k < i_ && k < s_.size(); ++k) {
            if (s_[k] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
        throw JsonError("JSON error at " + std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }

    void ws() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }

    bool consume(const char* lit) {
        size_t k = 0;
        while (lit[k]) {
            if (i_ + k >= s_.size() || s_[i_ + k] != lit[k]) return false;
            ++k;
        }
        i_ += k;
        return true;
    }

    Json value() {
        if (i_ >= s_.size()) fail("unexpected end of input");
        char c = s_[i_];
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') return Json::string(string_body());
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return number();
        if (consume("true")) return Json::boolean(true);
        if (consume("false")) return Json::boolean(false);
        if (consume("null")) return Json::null();
        fail(std::string("unexpected character '") + c + "'");
    }

    Json number() {
        size_t start = i_;
        if (s_[i_] == '-') ++i_;
        if (i_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[i_]))) fail("bad number");
        while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            if (i_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[i_]))) fail("bad number");
            while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
            if (i_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[i_]))) fail("bad number");
            while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
        }
        return Json::number_text(s_.substr(start, i_ - start));
    }

    unsigned hex4() {
        if (i_ + 4 > s_.size()) fail("bad \\u escape");
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s_[i_ + static_cast<size_t>(k)];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else fail("bad \\u escape");
        }
        i_ += 4;
        return v;
    }

    static void append_utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::string string_body() {
        ++i_;  // opening quote
        std::string out;
        while (true) {
            if (i_ >= s_.size()) fail("unterminated string");
            char c = s_[i_++];
            if (c == '"') break;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (i_ >= s_.size()) fail("unterminated escape");
            char e = s_[i_++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (!consume("\\u")) fail("lone high surrogate");
                        unsigned lo = hex4();
                        if (lo < 0xDC00 || lo > 0xDFFF) fail("bad low surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: fail("bad escape");
            }
        }
        return out;
    }

    Json array() {
        if (++depth_ > 200) fail("nesting too deep");
        ++i_;
        Json arr = Json::array();
        ws();
        if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            --depth_;
            return arr;
        }
        while (true) {
            ws();
            arr.push(value());
            ws();
            if (i_ >= s_.size()) fail("unterminated array");
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') {
                ++i_;
                break;
            }
            fail("expected ',' or ']'");
        }
        --depth_;
        return arr;
    }

    Json object() {
        if (++depth_ > 200) fail("nesting too deep");
        ++i_;
        Json obj = Json::object();
        ws();
        if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            --depth_;
            return obj;
        }
        while (true) {
            ws();
            if (i_ >= s_.size() || s_[i_] != '"') fail("expected string key");
            std::string key = string_body();
            if (obj.find(key)) fail("duplicate key '" + key + "'");
            ws();
            if (i_ >= s_.size() || s_[i_] != ':') fail("expected ':'");
            ++i_;
            ws();
            obj.set(key, value());
            ws();
            if (i_ >= s_.size()) fail("unterminated object");
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') {
                ++i_;
                break;
            }
            fail("expected ',' or '}'");
        }
        --depth_;
        return obj;
    }
};

void escape_to(std::string& out, const std::string& s) {
    out.push_back('"');
    for (char ch : s) {
        unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(ch);
                }
        }
    }
    out.push_back('"');
}

}  // namespace

Json Json::parse(const std::string& text) {
    Parser p(text);
    return p.parse_document();
}

void Json::dump_to(std::string& out, int indent, int level) const {
    const bool pretty = indent >= 0;
    auto nl = [&](int lvl) {
        if (!pretty) return;
        out.push_back('\n');
        out.append(static_cast<size_t>(lvl * indent), ' ');
    };
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += b_ ? "true" : "false"; break;
        case Type::Number: out += s_; break;
        case Type::String: escape_to(out, s_); break;
        case Type::Array:
            if (vals_.empty()) {
                out += "[]";
                break;
            }
            out.push_back('[');
            for (size_t i = 0; i < vals_.size(); ++i) {
                if (i) out.push_back(',');
                nl(level + 1);
                vals_[i].dump_to(out, indent, level + 1);
            }
            nl(level);
            out.push_back(']');
            break;
        case Type::Object:
            if (keys_.empty()) {
                out += "{}";
                break;
            }
            out.push_back('{');
            for (size_t i = 0; i < keys_.size(); ++i) {
                if (i) out.push_back(',');
                nl(level + 1);
                escape_to(out, keys_[i]);
                out.push_back(':');
                if (pretty) out.push_back(' ');
                vals_[i].dump_to(out, indent, level + 1);
            }
            nl(level);
            out.push_back('}');
            break;
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    dump_to(out, indent, 0);
    return out;
}

}  // namespace jobeval
