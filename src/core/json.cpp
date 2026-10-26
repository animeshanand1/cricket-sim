#include "core/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <unordered_map>

#include "core/jsmath.hpp"

namespace t20 {

namespace {
const std::string kEmpty;
const Json kNull;

void writeString(std::string& out, std::string_view s) {
  static const char* hex = "0123456789abcdef";
  out += '"';
  for (unsigned char ch : s) {
    switch (ch) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (ch < 0x20) {
          out += "\\u00";
          out += hex[ch >> 4];
          out += hex[ch & 15];
        } else {
          out += static_cast<char>(ch);
        }
    }
  }
  out += '"';
}

void appendUtf8(std::string& out, uint32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
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

struct Parser {
  std::string_view s;
  size_t i = 0;
  std::string err;
  int depth = 0;

  void ws() {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
  }
  bool fail(const char* m) {
    if (err.empty()) err = std::string(m) + " at " + std::to_string(i);
    return false;
  }
  bool lit(std::string_view w) {
    if (s.substr(i, w.size()) != w) return fail("bad literal");
    i += w.size();
    return true;
  }
  bool hex4(uint32_t& v) {
    if (i + 4 > s.size()) return fail("short \\u escape");
    v = 0;
    for (int k = 0; k < 4; ++k) {
      const char c = s[i++];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= static_cast<uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f') v |= static_cast<uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= static_cast<uint32_t>(c - 'A' + 10);
      else return fail("bad hex digit");
    }
    return true;
  }
  bool str(std::string& out) {
    if (i >= s.size() || s[i] != '"') return fail("expected string");
    ++i;
    while (i < s.size()) {
      const char c = s[i++];
      if (c == '"') return true;
      if (c != '\\') {
        out += c;
        continue;
      }
      if (i >= s.size()) return fail("bad escape");
      const char e = s[i++];
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
          uint32_t cp;
          if (!hex4(cp)) return false;
          if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
            i += 2;
            uint32_t lo;
            if (!hex4(lo)) return false;
            if (lo >= 0xDC00 && lo <= 0xDFFF) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          }
          appendUtf8(out, cp);
          break;
        }
        default: return fail("bad escape");
      }
    }
    return fail("unterminated string");
  }
  bool value(Json& out) {
    if (++depth > 200) return fail("too deep");
    ws();
    if (i >= s.size()) return fail("unexpected end");
    const char c = s[i];
    bool ok = true;
    if (c == '{') {
      ++i;
      out = Json::object();
      ws();
      if (i < s.size() && s[i] == '}') {
        ++i;
      } else {
        for (;;) {
          ws();
          std::string key;
          if (!str(key)) return false;
          ws();
          if (i >= s.size() || s[i] != ':') return fail("expected :");
          ++i;
          Json v;
          if (!value(v)) return false;
          out.add(std::move(key), std::move(v));
          ws();
          if (i < s.size() && s[i] == ',') {
            ++i;
            continue;
          }
          if (i < s.size() && s[i] == '}') {
            ++i;
            break;
          }
          return fail("expected , or }");
        }
      }
    } else if (c == '[') {
      ++i;
      out = Json::array();
      ws();
      if (i < s.size() && s[i] == ']') {
        ++i;
      } else {
        for (;;) {
          Json v;
          if (!value(v)) return false;
          out.push(std::move(v));
          ws();
          if (i < s.size() && s[i] == ',') {
            ++i;
            continue;
          }
          if (i < s.size() && s[i] == ']') {
            ++i;
            break;
          }
          return fail("expected , or ]");
        }
      }
    } else if (c == '"') {
      std::string v;
      ok = str(v);
      out = Json(std::move(v));
    } else if (c == 't') {
      ok = lit("true");
      out = Json(true);
    } else if (c == 'f') {
      ok = lit("false");
      out = Json(false);
    } else if (c == 'n') {
      ok = lit("null");
      out = Json();
    } else {
      const size_t start = i;
      if (s[i] == '-') ++i;
      while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.' || s[i] == 'e' ||
                              s[i] == 'E' || s[i] == '+' || s[i] == '-'))
        ++i;
      if (i == start) return fail("unexpected character");
      const std::string num(s.substr(start, i - start));
      char* end = nullptr;
      const double d = std::strtod(num.c_str(), &end);
      if (!end || *end) return fail("bad number");
      out = Json(d);
    }
    --depth;
    return ok;
  }
};

bool emptyish(const Json* j) {
  if (!j) return true;
  switch (j->type()) {
    case Json::Type::Null: return true;
    case Json::Type::Bool: return !j->boolean();
    case Json::Type::Array:
    case Json::Type::Object: return j->size() == 0;
    default: return false;
  }
}

std::string brief(const Json* j) {
  if (!j) return "(missing)";
  std::string d = j->dump();
  if (d.size() > 80) d = d.substr(0, 77) + "...";
  return d;
}

}  // namespace

const std::string& Json::str() const { return t_ == Type::String ? s_ : kEmpty; }

bool Json::truthy() const {
  switch (t_) {
    case Type::Null: return false;
    case Type::Bool: return b_;
    case Type::Number: return n_ != 0 && !std::isnan(n_);
    case Type::String: return !s_.empty();
    default: return true;
  }
}

Json& Json::set(std::string key, Json v) {
  if (t_ != Type::Object) {
    *this = object();
  }
  for (auto& kv : o_)
    if (kv.first == key) {
      kv.second = std::move(v);
      return kv.second;
    }
  o_.emplace_back(std::move(key), std::move(v));
  return o_.back().second;
}

const Json* Json::find(std::string_view key) const {
  if (t_ != Type::Object) return nullptr;
  for (const auto& kv : o_)
    if (kv.first == key) return &kv.second;
  return nullptr;
}

const Json& Json::operator[](std::string_view key) const {
  const Json* j = find(key);
  return j ? *j : kNull;
}

void Json::dumpTo(std::string& out) const {
  switch (t_) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += b_ ? "true" : "false"; break;
    case Type::Number: out += std::isfinite(n_) ? js_num(n_) : "null"; break;
    case Type::String: writeString(out, s_); break;
    case Type::Array: {
      out += '[';
      for (size_t k = 0; k < a_.size(); ++k) {
        if (k) out += ',';
        a_[k].dumpTo(out);
      }
      out += ']';
      break;
    }
    case Type::Object: {
      out += '{';
      bool first = true;
      for (const auto& kv : o_) {
        if (!first) out += ',';
        first = false;
        writeString(out, kv.first);
        out += ':';
        kv.second.dumpTo(out);
      }
      out += '}';
      break;
    }
  }
}

std::string Json::dump() const {
  std::string out;
  dumpTo(out);
  return out;
}

std::optional<Json> Json::parse(std::string_view text, std::string* error) {
  Parser p;
  p.s = text;
  Json out;
  if (!p.value(out)) {
    if (error) *error = p.err;
    return std::nullopt;
  }
  p.ws();
  if (p.i != text.size()) {
    if (error) *error = "trailing characters at " + std::to_string(p.i);
    return std::nullopt;
  }
  return out;
}

std::optional<std::string> jsonDiff(const Json& a, const Json& b, const std::string& path) {
  const bool ea = emptyish(&a), eb = emptyish(&b);
  if (ea && eb) return std::nullopt;
  auto here = [&]() { return (path.empty() ? std::string("(root)") : path) + ": " + brief(&a) + " vs " + brief(&b); };
  if (a.type() != b.type()) return here();
  switch (a.type()) {
    case Json::Type::Bool: return a.boolean() == b.boolean() ? std::nullopt : std::optional<std::string>(here());
    case Json::Type::Number: {
      const double x = a.num(), y = b.num();
      if (x == y || (std::isnan(x) && std::isnan(y))) return std::nullopt;
      return here();
    }
    case Json::Type::String: return a.str() == b.str() ? std::nullopt : std::optional<std::string>(here());
    case Json::Type::Array: {
      if (a.size() != b.size())
        return (path.empty() ? std::string("(root)") : path) + ": length " + std::to_string(a.size()) + " vs " +
               std::to_string(b.size());
      for (size_t k = 0; k < a.size(); ++k)
        if (auto d = jsonDiff(a.at(k), b.at(k), path + "[" + std::to_string(k) + "]")) return d;
      return std::nullopt;
    }
    case Json::Type::Object: {
      std::unordered_map<std::string_view, const Json*> ma, mb;
      std::vector<std::string_view> keys;
      for (const auto& kv : a.items())
        if (ma.emplace(kv.first, &kv.second).second) keys.push_back(kv.first);
      for (const auto& kv : b.items())
        if (mb.emplace(kv.first, &kv.second).second && !ma.count(kv.first)) keys.push_back(kv.first);
      for (const auto& kv : keys) {
        const std::string k(kv);
        const Json* x = ma.count(kv) ? ma[kv] : nullptr;
        const Json* y = mb.count(kv) ? mb[kv] : nullptr;
        const std::string sub = path.empty() ? k : path + "." + k;
        if (emptyish(x) && emptyish(y)) continue;
        if (!x || !y) return sub + ": " + brief(x) + " vs " + brief(y);
        if (auto d = jsonDiff(*x, *y, sub)) return d;
      }
      return std::nullopt;
    }
    default: return std::nullopt;
  }
}

}  // namespace t20
