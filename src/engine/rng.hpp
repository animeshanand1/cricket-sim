#pragma once
// Seeded PRNG (mulberry32), bit for bit the same as rng.js. A match keeps its stream state in
// Match::rngState so every match is reproducible from its seed.

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/jsmath.hpp"

namespace t20 {

struct RandOut {
  double value;
  int32_t state;
};

// rng.js nextRandom
inline RandOut nextRandom(int32_t state) {
  const int32_t t = js_add32(state, 0x6d2b79f5);
  int32_t r = js_imul(static_cast<int32_t>(static_cast<uint32_t>(t) ^ js_ushr(t, 15)), 1 | t);
  r = static_cast<int32_t>(
      static_cast<uint32_t>(js_add32(r, js_imul(static_cast<int32_t>(static_cast<uint32_t>(r) ^ js_ushr(r, 7)), 61 | r))) ^
      static_cast<uint32_t>(r));
  const uint32_t out = static_cast<uint32_t>(r) ^ js_ushr(r, 14);
  return {static_cast<double>(out) / 4294967296.0, t};
}

// createRng(seed): a small stateful stream
class Rng {
 public:
  explicit Rng(int32_t seed) : s_(seed) {}
  double operator()() {
    const RandOut o = nextRandom(s_);
    s_ = o.state;
    return o.value;
  }
  int32_t state() const { return s_; }

 private:
  int32_t s_;
};

// rand(holder): reads from a stream whose state lives elsewhere (Match::rngState)
struct StateStream {
  int32_t* state;
  double operator()() const {
    const RandOut o = nextRandom(*state);
    *state = o.state;
    return o.value;
  }
};

// A non-owning reference to any `double()` source of random numbers (the JS passes `r` or
// `rng` functions around). Binds to lvalues only, so it cannot dangle on a temporary.
class RandFn {
 public:
  template <class F, class = std::enable_if_t<!std::is_same_v<std::remove_cvref_t<F>, RandFn>>>
  RandFn(F& f) noexcept  // NOLINT(google-explicit-constructor)
      : obj_(static_cast<void*>(&f)), fn_([](void* o) -> double { return (*static_cast<F*>(o))(); }) {}
  double operator()() const { return fn_(obj_); }

 private:
  void* obj_;
  double (*fn_)(void*);
};

inline int randInt(RandFn rng, int min, int max) {
  return min + static_cast<int>(std::floor(rng() * (max - min + 1)));
}

template <class T>
const T& pick(RandFn rng, const std::vector<T>& arr) {
  return arr[static_cast<size_t>(std::floor(rng() * static_cast<double>(arr.size())))];
}

// Weighted choice from an ordered list of (key, weight), in the order JS would iterate the
// object. NaN weights behave as in JS: the loop never returns and the last key wins.
template <class Key>
Key weighted(RandFn rng, const std::vector<std::pair<Key, double>>& weights) {
  double total = 0;
  for (const auto& kv : weights) total += kv.second;
  double x = rng() * total;
  for (const auto& kv : weights) {
    x -= kv.second;
    if (x <= 0) return kv.first;
  }
  return weights.back().first;
}

// The modules' hashRng(...parts): h = imul(h ^ charCode, 2654435761) over the characters of
// String(part) for every part, starting from a per-module salt. Parts are strings or integers.
namespace detail {
inline int32_t hashPart(int32_t h, std::string_view s) {
  for (unsigned char ch : s) h = js_imul(h ^ static_cast<int32_t>(ch), kGolden);
  return h;
}
inline int32_t hashPart(int32_t h, const std::string& s) { return hashPart(h, std::string_view(s)); }
inline int32_t hashPart(int32_t h, const char* s) { return hashPart(h, std::string_view(s)); }
inline int32_t hashPart(int32_t h, int v) { return hashPart(h, std::string_view(std::to_string(v))); }
inline int32_t hashPart(int32_t h, size_t v) { return hashPart(h, std::string_view(std::to_string(v))); }
}  // namespace detail

template <class... P>
Rng hashRng(int32_t salt, const P&... parts) {
  int32_t h = salt;
  ((h = detail::hashPart(h, parts)), ...);
  return Rng(h);
}

// Roughly bell-shaped value in [min, max] (average of 3 uniforms), drawn in order.
inline double bell(RandFn rng, double min, double max) {
  double u = rng();
  u += rng();
  u += rng();
  u /= 3;
  return js_round(min + u * (max - min));
}

}  // namespace t20
