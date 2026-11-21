#pragma once
// Minimal JSON value, parser and serializer (no external dependencies).
// Objects keep insertion order so files written by the engine stay readable and diff-friendly.

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace npc::json {

class Value;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;

class Value {
public:
    enum class Type : std::uint8_t { Null, Bool, Number, String, Array, Object };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : data_(b) {}
    Value(int v) : data_(static_cast<double>(v)) {}
    Value(unsigned v) : data_(static_cast<double>(v)) {}
    Value(long v) : data_(static_cast<double>(v)) {}
    Value(unsigned long v) : data_(static_cast<double>(v)) {}
    Value(long long v) : data_(static_cast<double>(v)) {}
    Value(unsigned long long v) : data_(static_cast<double>(v)) {}
    Value(float v) : data_(static_cast<double>(v)) {}
    Value(double v) : data_(v) {}
    Value(const char* s) : data_(std::string(s)) {}
    Value(std::string s) : data_(std::move(s)) {}
    Value(std::string_view s) : data_(std::string(s)) {}
    Value(Array a) : data_(std::move(a)) {}
    Value(Object o) : data_(std::move(o)) {}

    static Value object() { return Value(Object{}); }
    static Value array() { return Value(Array{}); }

    Type type() const { return static_cast<Type>(data_.index()); }
    bool is_null() const { return type() == Type::Null; }
    bool is_bool() const { return type() == Type::Bool; }
    bool is_number() const { return type() == Type::Number; }
    bool is_string() const { return type() == Type::String; }
    bool is_array() const { return type() == Type::Array; }
    bool is_object() const { return type() == Type::Object; }

    bool as_bool(bool fallback = false) const { return is_bool() ? std::get<bool>(data_) : fallback; }
    double as_number(double fallback = 0.0) const { return is_number() ? std::get<double>(data_) : fallback; }
    float as_float(float fallback = 0.f) const { return is_number() ? static_cast<float>(std::get<double>(data_)) : fallback; }
    long long as_int(long long fallback = 0) const { return is_number() ? static_cast<long long>(std::get<double>(data_)) : fallback; }
    const std::string& as_string() const;
    std::string as_string(std::string_view fallback) const { return is_string() ? std::get<std::string>(data_) : std::string(fallback); }
    const Array& as_array() const;
    const Object& as_object() const;
    Array& as_array_mut();
    Object& as_object_mut();

    // Object access. The const operator returns a shared null value for missing keys.
    const Value& operator[](std::string_view key) const;
    Value& operator[](std::string_view key); // inserts (converting null to object)
    bool contains(std::string_view key) const;
    const Value* find(std::string_view key) const;
    Value& set(std::string_view key, Value v);

    // Array access
    const Value& operator[](std::size_t i) const;
    void push_back(Value v);
    std::size_t size() const; // array/object element count, 0 otherwise

    // Convenience getters with defaults
    double num(std::string_view key, double fallback) const { return (*this)[key].as_number(fallback); }
    float flt(std::string_view key, float fallback) const { return (*this)[key].as_float(fallback); }
    long long integer(std::string_view key, long long fallback) const { return (*this)[key].as_int(fallback); }
    bool boolean(std::string_view key, bool fallback) const { return (*this)[key].as_bool(fallback); }
    std::string str(std::string_view key, std::string_view fallback = {}) const { return (*this)[key].as_string(fallback); }

    std::string dump(int indent = -1) const;

    friend bool operator==(const Value& a, const Value& b) { return a.data_ == b.data_; }

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data_{nullptr};
};

struct ParseResult {
    Value value;
    std::string error; // empty on success
    bool ok() const { return error.empty(); }
};

ParseResult parse(std::string_view text);
/// Reads and parses a UTF-8 file. Error message includes the path on failure.
ParseResult parse_file(const std::string& path);
bool write_file(const std::string& path, const Value& value, int indent = 2);

/// Escapes a string for embedding in JSON (without surrounding quotes).
std::string escape(std::string_view s);

} // namespace npc::json
