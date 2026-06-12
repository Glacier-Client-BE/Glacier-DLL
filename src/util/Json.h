#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

// Tiny, allocation-light JSON builder. Just enough to let each module serialize
// its own HUD payload without every call site re-implementing comma handling
// and string escaping. Not a general-purpose JSON library — write-only, no
// parsing, no validation beyond escaping.
namespace glacier::json {

inline std::string escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out += std::format("\\u{:04x}", static_cast<int>(c));
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// Builds a `"key":value,"key":value` body; wrap with str() to get `{...}`.
class Object {
public:
    Object& set(std::string_view key, std::string_view value) {
        comma();
        m_body += std::format("\"{}\":\"{}\"", escape(key), escape(value));
        return *this;
    }
    Object& set(std::string_view key, const char* value) {
        return set(key, std::string_view{ value });
    }
    Object& set(std::string_view key, bool value) {
        comma();
        m_body += std::format("\"{}\":{}", escape(key), value ? "true" : "false");
        return *this;
    }
    Object& set(std::string_view key, int value) {
        comma();
        m_body += std::format("\"{}\":{}", escape(key), value);
        return *this;
    }
    Object& set(std::string_view key, double value) {
        comma();
        m_body += std::format("\"{}\":{}", escape(key), value);
        return *this;
    }
    Object& set(std::string_view key, float value) {
        return set(key, static_cast<double>(value));
    }
    // Inserts an already-serialized array/object (e.g. a nested slot list).
    Object& raw(std::string_view key, std::string_view rawJson) {
        comma();
        m_body += std::format("\"{}\":{}", escape(key), rawJson);
        return *this;
    }

    std::string str() const { return "{" + m_body + "}"; }

private:
    void comma() { if (!m_first) m_body += ','; m_first = false; }
    std::string m_body;
    bool m_first = true;
};

// Builds a `[a,b,c]` array of arbitrary serialized values.
class Array {
public:
    Array& push(const Object& obj) { return pushRaw(obj.str()); }
    Array& pushRaw(std::string_view rawJson) {
        comma();
        m_body += rawJson;
        return *this;
    }
    bool empty() const { return m_first; }
    std::string str() const { return "[" + m_body + "]"; }

private:
    void comma() { if (!m_first) m_body += ','; m_first = false; }
    std::string m_body;
    bool m_first = true;
};

} // namespace glacier::json
