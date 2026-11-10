// Minimal JSON document model. Numbers keep their original text so that
// "0.0595" reaches the Decimal parser exactly as the pack author wrote it.
// Objects preserve key order (diffs of compiled packs stay readable).
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace jobeval {

struct JsonError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    static Json null() { return Json(); }
    static Json boolean(bool b);
    static Json number_text(std::string text);
    static Json string(std::string s);
    static Json array();
    static Json object();

    static Json parse(const std::string& text);  // throws JsonError with line:col

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool as_bool() const { return b_; }
    const std::string& str() const { return s_; }  // string value or number text

    // Arrays.
    size_t size() const { return type_ == Type::Array ? vals_.size() : keys_.size(); }
    const Json& at(size_t i) const { return vals_.at(i); }
    const std::vector<Json>& items() const { return vals_; }
    void push(Json v);

    // Objects.
    const std::vector<std::string>& keys() const { return keys_; }
    const Json* find(const std::string& key) const;
    const Json& value_at(size_t i) const { return vals_.at(i); }
    void set(const std::string& key, Json v);

    std::string dump(int indent = -1) const;  // indent < 0 means compact

private:
    void dump_to(std::string& out, int indent, int level) const;

    Type type_ = Type::Null;
    bool b_ = false;
    std::string s_;
    std::vector<std::string> keys_;  // objects only
    std::vector<Json> vals_;         // array items or object values
};

}  // namespace jobeval
