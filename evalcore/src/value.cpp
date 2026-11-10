#include "evalcore/value.hpp"

namespace jobeval {

Value Value::num(Decimal d) {
    Value v;
    v.kind_ = Kind::Num;
    v.num_ = d;
    return v;
}
Value Value::boolean(bool b) {
    Value v;
    v.kind_ = Kind::Bool;
    v.b_ = b;
    return v;
}
Value Value::str(std::string s) {
    Value v;
    v.kind_ = Kind::Str;
    v.s_ = std::move(s);
    return v;
}
Value Value::data(Json j) {
    Value v;
    v.kind_ = Kind::Data;
    v.data_ = std::make_shared<const Json>(std::move(j));
    return v;
}

Value Value::from_json(const Json& j) {
    switch (j.type()) {
        case Json::Type::Number: return num(Decimal::parse(j.str()));
        case Json::Type::Bool: return boolean(j.as_bool());
        case Json::Type::String: return str(j.str());
        case Json::Type::Null: throw EvalError("null is not a valid value");
        default: return data(j);
    }
}

std::string Value::kind_name() const {
    switch (kind_) {
        case Kind::Num: return "number";
        case Kind::Bool: return "boolean";
        case Kind::Str: return "string";
        case Kind::Data: return "table";
    }
    return "?";
}

const Decimal& Value::as_num() const {
    if (kind_ != Kind::Num) throw EvalError("expected a number but got a " + kind_name());
    return num_;
}
bool Value::as_bool() const {
    if (kind_ != Kind::Bool) throw EvalError("expected a boolean but got a " + kind_name());
    return b_;
}
const std::string& Value::as_str() const {
    if (kind_ != Kind::Str) throw EvalError("expected a string but got a " + kind_name());
    return s_;
}
const Json& Value::as_data() const {
    if (kind_ != Kind::Data) throw EvalError("expected a table but got a " + kind_name());
    return *data_;
}

std::string Value::describe() const {
    switch (kind_) {
        case Kind::Num: return num_.to_string();
        case Kind::Bool: return b_ ? "true" : "false";
        case Kind::Str: return "\"" + s_ + "\"";
        case Kind::Data: {
            std::string d = data_->dump();
            if (d.size() > 120) d = d.substr(0, 117) + "...";
            return d;
        }
    }
    return "?";
}

bool operator==(const Value& a, const Value& b) {
    if (a.kind_ != b.kind_) return false;
    switch (a.kind_) {
        case Value::Kind::Num: return a.num_ == b.num_;
        case Value::Kind::Bool: return a.b_ == b.b_;
        case Value::Kind::Str: return a.s_ == b.s_;
        case Value::Kind::Data: return a.data_->dump() == b.data_->dump();
    }
    return false;
}

}  // namespace jobeval
