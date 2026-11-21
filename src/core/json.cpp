#include "core/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace npc::json {

namespace {
const Value& null_value() {
    static const Value v;
    return v;
}
const std::string& empty_string() {
    static const std::string s;
    return s;
}
const Array& empty_array() {
    static const Array a;
    return a;
}
const Object& empty_object() {
    static const Object o;
    return o;
}
} // namespace

const std::string& Value::as_string() const { return is_string() ? std::get<std::string>(data_) : empty_string(); }
const Array& Value::as_array() const { return is_array() ? std::get<Array>(data_) : empty_array(); }
const Object& Value::as_object() const { return is_object() ? std::get<Object>(data_) : empty_object(); }
Array& Value::as_array_mut() {
    if (!is_array()) data_ = Array{};
    return std::get<Array>(data_);
}
Object& Value::as_object_mut() {
    if (!is_object()) data_ = Object{};
    return std::get<Object>(data_);
}

const Value* Value::find(std::string_view key) const {
    if (!is_object()) return nullptr;
    for (const auto& [k, v] : std::get<Object>(data_))
        if (k == key) return &v;
    return nullptr;
}
const Value& Value::operator[](std::string_view key) const {
    const Value* v = find(key);
    return v ? *v : null_value();
}
Value& Value::operator[](std::string_view key) {
    auto& obj = as_object_mut();
    for (auto& [k, v] : obj)
        if (k == key) return v;
    obj.emplace_back(std::string(key), Value{});
    return obj.back().second;
}
bool Value::contains(std::string_view key) const { return find(key) != nullptr; }
Value& Value::set(std::string_view key, Value v) {
    Value& slot = (*this)[key];
    slot = std::move(v);
    return slot;
}
const Value& Value::operator[](std::size_t i) const {
    const auto& arr = as_array();
    return i < arr.size() ? arr[i] : null_value();
}
void Value::push_back(Value v) { as_array_mut().push_back(std::move(v)); }
std::size_t Value::size() const {
    if (is_array()) return std::get<Array>(data_).size();
    if (is_object()) return std::get<Object>(data_).size();
    return 0;
}

// ---------------------------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------------------------
std::string escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

namespace {
void format_number(std::string& out, double d) {
    if (!std::isfinite(d)) { out += "null"; return; }
    if (d == std::floor(d) && std::abs(d) < 1e15) {
        char buf[32];
        auto [p, ec] = std::to_chars(buf, buf + sizeof buf, static_cast<long long>(d));
        out.append(buf, p);
        return;
    }
    char buf[64];
    auto [p, ec] = std::to_chars(buf, buf + sizeof buf, d, std::chars_format::general, 9);
    out.append(buf, p);
}

void dump_value(const Value& v, std::string& out, int indent, int depth) {
    auto newline = [&](int d) {
        if (indent < 0) return;
        out += '\n';
        out.append(static_cast<std::size_t>(indent * d), ' ');
    };
    switch (v.type()) {
        case Value::Type::Null: out += "null"; break;
        case Value::Type::Bool: out += v.as_bool() ? "true" : "false"; break;
        case Value::Type::Number: format_number(out, v.as_number()); break;
        case Value::Type::String: out += '"'; out += escape(v.as_string()); out += '"'; break;
        case Value::Type::Array: {
            const auto& arr = v.as_array();
            if (arr.empty()) { out += "[]"; break; }
            // Arrays of scalars stay on one line for compact data files.
            bool scalar = true;
            for (const auto& e : arr) scalar &= !e.is_array() && !e.is_object();
            out += '[';
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (i) out += indent >= 0 && scalar ? ", " : ",";
                if (!scalar) newline(depth + 1);
                dump_value(arr[i], out, indent, depth + 1);
            }
            if (!scalar) newline(depth);
            out += ']';
            break;
        }
        case Value::Type::Object: {
            const auto& obj = v.as_object();
            if (obj.empty()) { out += "{}"; break; }
            out += '{';
            for (std::size_t i = 0; i < obj.size(); ++i) {
                if (i) out += ',';
                newline(depth + 1);
                out += '"'; out += escape(obj[i].first); out += indent >= 0 ? "\": " : "\":";
                dump_value(obj[i].second, out, indent, depth + 1);
            }
            newline(depth);
            out += '}';
            break;
        }
    }
}
} // namespace

std::string Value::dump(int indent) const {
    std::string out;
    dump_value(*this, out, indent, 0);
    return out;
}

// ---------------------------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------------------------
namespace {
class Parser {
public:
    explicit Parser(std::string_view s) : s_(s) {}

    Value parse_document() {
        skip_ws();
        Value v = parse_value(0);
        skip_ws();
        if (pos_ != s_.size()) fail("trailing characters");
        return v;
    }

    [[noreturn]] void fail(const char* what) const {
        std::size_t line = 1, col = 1;
        for (std::size_t i = 0; i < pos_ && i < s_.size(); ++i) {
            if (s_[i] == '\n') { ++line; col = 1; } else ++col;
        }
        throw std::runtime_error(std::string(what) + " at line " + std::to_string(line) + ", column " + std::to_string(col));
    }

private:
    void skip_ws() {
        while (pos_ < s_.size()) {
            const char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++pos_; continue; }
            // Comments (// and /* */) are accepted so hand-written data files can be annotated.
            if (c == '/' && pos_ + 1 < s_.size() && s_[pos_ + 1] == '/') {
                while (pos_ < s_.size() && s_[pos_] != '\n') ++pos_;
                continue;
            }
            if (c == '/' && pos_ + 1 < s_.size() && s_[pos_ + 1] == '*') {
                const auto end = s_.find("*/", pos_ + 2);
                if (end == std::string_view::npos) fail("unterminated comment");
                pos_ = end + 2;
                continue;
            }
            break;
        }
    }

    Value parse_value(int depth) {
        if (depth > 256) fail("nesting too deep");
        if (pos_ >= s_.size()) fail("unexpected end of input");
        const char c = s_[pos_];
        if (c == '{') return parse_object(depth);
        if (c == '[') return parse_array(depth);
        if (c == '"') return Value(parse_string());
        if (c == 't') { expect("true"); return Value(true); }
        if (c == 'f') { expect("false"); return Value(false); }
        if (c == 'n') { expect("null"); return Value(); }
        if (c == '-' || (c >= '0' && c <= '9')) return Value(parse_number());
        fail("unexpected character");
    }

    void expect(std::string_view word) {
        if (s_.substr(pos_, word.size()) != word) fail("invalid literal");
        pos_ += word.size();
    }

    Value parse_object(int depth) {
        ++pos_;
        Value v = Value::object();
        auto& obj = v.as_object_mut();
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return v; }
        while (true) {
            skip_ws();
            if (pos_ >= s_.size() || s_[pos_] != '"') fail("expected object key");
            std::string key = parse_string();
            skip_ws();
            if (pos_ >= s_.size() || s_[pos_] != ':') fail("expected ':'");
            ++pos_;
            skip_ws();
            obj.emplace_back(std::move(key), parse_value(depth + 1));
            skip_ws();
            if (pos_ < s_.size() && s_[pos_] == ',') {
                ++pos_;
                skip_ws();
                if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return v; } // trailing comma tolerated
                continue;
            }
            if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return v; }
            fail("expected ',' or '}'");
        }
    }

    Value parse_array(int depth) {
        ++pos_;
        Value v = Value::array();
        auto& arr = v.as_array_mut();
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return v; }
        while (true) {
            skip_ws();
            arr.push_back(parse_value(depth + 1));
            skip_ws();
            if (pos_ < s_.size() && s_[pos_] == ',') {
                ++pos_;
                skip_ws();
                if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return v; }
                continue;
            }
            if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return v; }
            fail("expected ',' or ']'");
        }
    }

    static void append_utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    std::uint32_t parse_hex4() {
        if (pos_ + 4 > s_.size()) fail("bad \\u escape");
        std::uint32_t cp = 0;
        for (int i = 0; i < 4; ++i) {
            const char h = s_[pos_++];
            cp <<= 4;
            if (h >= '0' && h <= '9') cp |= static_cast<std::uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f') cp |= static_cast<std::uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') cp |= static_cast<std::uint32_t>(h - 'A' + 10);
            else fail("bad hex digit");
        }
        return cp;
    }

    std::string parse_string() {
        ++pos_; // opening quote
        std::string out;
        while (true) {
            if (pos_ >= s_.size()) fail("unterminated string");
            const char c = s_[pos_++];
            if (c == '"') return out;
            if (c != '\\') { out += c; continue; }
            if (pos_ >= s_.size()) fail("unterminated escape");
            const char e = s_[pos_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    std::uint32_t cp = parse_hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 6 <= s_.size() && s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                        pos_ += 2;
                        const std::uint32_t lo = parse_hex4();
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: fail("invalid escape");
            }
        }
    }

    double parse_number() {
        const std::size_t start = pos_;
        if (s_[pos_] == '-') ++pos_;
        while (pos_ < s_.size() && ((s_[pos_] >= '0' && s_[pos_] <= '9') || s_[pos_] == '.' || s_[pos_] == 'e' ||
                                    s_[pos_] == 'E' || s_[pos_] == '+' || s_[pos_] == '-'))
            ++pos_;
        double d = 0;
        auto [p, ec] = std::from_chars(s_.data() + start, s_.data() + pos_, d);
        if (ec != std::errc{} || p != s_.data() + pos_) fail("invalid number");
        return d;
    }

    std::string_view s_;
    std::size_t pos_ = 0;
};
} // namespace

ParseResult parse(std::string_view text) {
    ParseResult r;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
        text.remove_prefix(3); // tolerate a UTF-8 BOM
    try {
        Parser p(text);
        r.value = p.parse_document();
    } catch (const std::exception& e) {
        r.error = e.what();
    }
    return r;
}

ParseResult parse_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {Value{}, "cannot open " + path};
    std::ostringstream ss;
    ss << in.rdbuf();
    auto r = parse(ss.str());
    if (!r.ok()) r.error = path + ": " + r.error;
    return r;
}

bool write_file(const std::string& path, const Value& value, int indent) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << value.dump(indent) << '\n';
    return static_cast<bool>(out);
}

} // namespace npc::json
