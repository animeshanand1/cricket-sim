#pragma once
// Compact JSON writer whose output matches JSON.stringify byte for byte (keys in the order
// written, numbers formatted like JS, UTF-8 passed through, same escapes).

#include <string>
#include <string_view>
#include <vector>

#include "core/jsmath.hpp"

namespace t20 {

class JsonWriter {
 public:
  JsonWriter& beginObject() { open('{'); return *this; }
  JsonWriter& endObject() { close('}'); return *this; }
  JsonWriter& beginArray() { open('['); return *this; }
  JsonWriter& endArray() { close(']'); return *this; }

  JsonWriter& key(std::string_view k) {
    comma();
    writeString(k);
    out_ += ':';
    afterKey_ = true;
    return *this;
  }
  JsonWriter& value(double v) {
    comma();
    out_ += (std::isfinite(v) ? js_num(v) : "null");
    return *this;
  }
  JsonWriter& value(int v) { return value(static_cast<double>(v)); }
  JsonWriter& value(bool v) {
    comma();
    out_ += v ? "true" : "false";
    return *this;
  }
  JsonWriter& value(std::string_view v) {
    comma();
    writeString(v);
    return *this;
  }
  JsonWriter& value(const char* v) { return value(std::string_view(v)); }
  JsonWriter& value(const std::string& v) { return value(std::string_view(v)); }
  JsonWriter& null() {
    comma();
    out_ += "null";
    return *this;
  }

  const std::string& str() const { return out_; }

 private:
  void comma() {
    if (afterKey_) {
      afterKey_ = false;
      return;
    }
    if (!first_.empty()) {
      if (!first_.back()) out_ += ',';
      first_.back() = false;
    }
  }
  void open(char ch) {
    comma();
    out_ += ch;
    first_.push_back(true);
  }
  void close(char ch) {
    out_ += ch;
    first_.pop_back();
  }
  void writeString(std::string_view s) {
    static const char* hex = "0123456789abcdef";
    out_ += '"';
    for (unsigned char ch : s) {
      switch (ch) {
        case '"': out_ += "\\\""; break;
        case '\\': out_ += "\\\\"; break;
        case '\b': out_ += "\\b"; break;
        case '\f': out_ += "\\f"; break;
        case '\n': out_ += "\\n"; break;
        case '\r': out_ += "\\r"; break;
        case '\t': out_ += "\\t"; break;
        default:
          if (ch < 0x20) {
            out_ += "\\u00";
            out_ += hex[ch >> 4];
            out_ += hex[ch & 15];
          } else {
            out_ += static_cast<char>(ch);
          }
      }
    }
    out_ += '"';
  }

  std::string out_;
  std::vector<bool> first_;
  bool afterKey_ = false;
};

}  // namespace t20
