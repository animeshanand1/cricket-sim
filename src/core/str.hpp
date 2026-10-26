#pragma once
// cat(...): build a string from pieces, like a JS template literal. Integers print as integers;
// doubles print the way JS prints numbers.

#include <string>
#include <string_view>
#include <type_traits>

#include "core/jsmath.hpp"

namespace t20 {

namespace detail {
inline void append(std::string& out, std::string_view s) { out.append(s.data(), s.size()); }
inline void append(std::string& out, const std::string& s) { out.append(s); }
inline void append(std::string& out, const char* s) { out.append(s); }
inline void append(std::string& out, char c) { out.push_back(c); }
template <class T>
  requires(std::is_integral_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, bool>)
inline void append(std::string& out, T v) {
  out.append(std::to_string(v));
}
inline void append(std::string& out, double v) { out.append(js_num(v)); }
}  // namespace detail

template <class... A>
std::string cat(const A&... parts) {
  std::string out;
  out.reserve(64);
  (detail::append(out, parts), ...);
  return out;
}

}  // namespace t20
