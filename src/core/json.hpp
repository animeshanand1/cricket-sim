#pragma once
// A small JSON value: parse, build, and write exactly like JSON.stringify (keys in insertion order,
// numbers formatted like JS). Used for the save files and for parity checks against the JS game.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace t20 {

class Json {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  Json() = default;
  Json(std::nullptr_t) {}
  Json(bool b) : t_(Type::Bool), b_(b) {}
  Json(double n) : t_(Type::Number), n_(n) {}
  Json(int n) : t_(Type::Number), n_(n) {}
  Json(long n) : t_(Type::Number), n_(static_cast<double>(n)) {}
  Json(long long n) : t_(Type::Number), n_(static_cast<double>(n)) {}
  Json(unsigned n) : t_(Type::Number), n_(n) {}
  Json(unsigned long n) : t_(Type::Number), n_(static_cast<double>(n)) {}
  Json(unsigned long long n) : t_(Type::Number), n_(static_cast<double>(n)) {}
  Json(const char* s) : t_(Type::String), s_(s) {}
  Json(std::string s) : t_(Type::String), s_(std::move(s)) {}
  Json(std::string_view s) : t_(Type::String), s_(s) {}

  static Json array() {
    Json j;
    j.t_ = Type::Array;
    return j;
  }
  static Json object() {
    Json j;
    j.t_ = Type::Object;
    return j;
  }

  Type type() const { return t_; }
  bool isNull() const { return t_ == Type::Null; }
  bool isBool() const { return t_ == Type::Bool; }
  bool isNumber() const { return t_ == Type::Number; }
  bool isString() const { return t_ == Type::String; }
  bool isArray() const { return t_ == Type::Array; }
  bool isObject() const { return t_ == Type::Object; }

  bool boolean() const { return t_ == Type::Bool && b_; }
  double num(double fallback = 0) const { return t_ == Type::Number ? n_ : fallback; }
  int integer(int fallback = 0) const { return t_ == Type::Number ? static_cast<int>(n_) : fallback; }
  const std::string& str() const;
  // Truthy like JS (for flags read from saves)
  bool truthy() const;

  // Arrays
  void push(Json v) { a_.push_back(std::move(v)); }
  size_t size() const { return t_ == Type::Array ? a_.size() : o_.size(); }
  const Json& at(size_t i) const { return a_[i]; }
  const std::vector<Json>& elements() const { return a_; }

  // Objects: set appends (or replaces); find returns null when missing
  Json& set(std::string key, Json v);
  // Append without looking for an existing key (for keys known to be new: maps, the parser)
  Json& add(std::string key, Json v) {
    if (t_ != Type::Object) *this = object();
    o_.emplace_back(std::move(key), std::move(v));
    return o_.back().second;
  }
  const Json* find(std::string_view key) const;
  // A member or a shared null
  const Json& operator[](std::string_view key) const;
  const std::vector<std::pair<std::string, Json>>& items() const { return o_; }

  std::string dump() const;
  static std::optional<Json> parse(std::string_view text, std::string* error = nullptr);

 private:
  void dumpTo(std::string& out) const;

  Type t_ = Type::Null;
  bool b_ = false;
  double n_ = 0;
  std::string s_;
  std::vector<Json> a_;
  std::vector<std::pair<std::string, Json>> o_;
};

// The first difference between two JSON values, as "path: a vs b", or none if they match.
// Object key order is ignored; a missing key, null, false and empty arrays or objects count as the
// same (the JS game uses `?? []`, `?? false` and `??=` freely). Numbers must be equal.
std::optional<std::string> jsonDiff(const Json& a, const Json& b, const std::string& path = "");

}  // namespace t20
