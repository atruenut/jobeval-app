#pragma once

#include <memory>
#include <string>

#include "evalcore/decimal.hpp"
#include "evalcore/json.hpp"

namespace jobeval {

// A runtime value in the rule language: number, boolean, string, or a
// structured parameter (table / object) that functions such as bracket()
// interpret.
class Value {
public:
    enum class Kind { Num, Bool, Str, Data };

    Value() = default;
    static Value num(Decimal d);
    static Value boolean(bool b);
    static Value str(std::string s);
    static Value data(Json j);
    static Value from_json(const Json& j);  // number/bool/string become scalars

    Kind kind() const { return kind_; }
    bool is_num() const { return kind_ == Kind::Num; }
    bool is_bool() const { return kind_ == Kind::Bool; }
    bool is_str() const { return kind_ == Kind::Str; }
    bool is_data() const { return kind_ == Kind::Data; }

    const Decimal& as_num() const;  // throw EvalError if not a number
    bool as_bool() const;
    const std::string& as_str() const;
    const Json& as_data() const;

    std::string describe() const;  // for traces and error messages
    std::string kind_name() const;

    friend bool operator==(const Value& a, const Value& b);

private:
    Kind kind_ = Kind::Num;
    Decimal num_;
    bool b_ = false;
    std::string s_;
    std::shared_ptr<const Json> data_;
};

}  // namespace jobeval
