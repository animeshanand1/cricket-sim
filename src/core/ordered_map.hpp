#pragma once
// Insertion-ordered map with JS object semantics: iteration follows insertion order, erase keeps
// the order of the rest. The engine relies on this order (ties go to the first key).
// Only string-like keys are used, never integer-like ones, so JS's "integer keys first" rule
// does not apply.

#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace t20 {

template <class K, class V>
class OrderedMap {
 public:
  using value_type = std::pair<K, V>;
  using iterator = typename std::vector<value_type>::iterator;
  using const_iterator = typename std::vector<value_type>::const_iterator;

  V* find(const K& k) {
    auto it = index_.find(k);
    return it == index_.end() ? nullptr : &items_[it->second].second;
  }
  const V* find(const K& k) const {
    auto it = index_.find(k);
    return it == index_.end() ? nullptr : &items_[it->second].second;
  }
  bool contains(const K& k) const { return index_.count(k) != 0; }

  // Like obj[k] on write: inserts a default value at the end if missing.
  V& operator[](const K& k) {
    if (V* v = find(k)) return *v;
    index_.emplace(k, items_.size());
    items_.emplace_back(k, V{});
    return items_.back().second;
  }
  V& at(const K& k) {
    if (V* v = find(k)) return *v;
    throw std::out_of_range("OrderedMap::at: missing key");
  }
  const V& at(const K& k) const {
    if (const V* v = find(k)) return *v;
    throw std::out_of_range("OrderedMap::at: missing key");
  }
  V& set(const K& k, V v) {
    V& slot = (*this)[k];
    slot = std::move(v);
    return slot;
  }
  bool erase(const K& k) {
    auto it = index_.find(k);
    if (it == index_.end()) return false;
    const size_t i = it->second;
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i));
    index_.erase(it);
    for (auto& entry : index_)
      if (entry.second > i) --entry.second;
    return true;
  }
  void clear() {
    items_.clear();
    index_.clear();
  }
  void reserve(size_t n) {
    items_.reserve(n);
    index_.reserve(n);
  }

  size_t size() const { return items_.size(); }
  bool empty() const { return items_.empty(); }
  iterator begin() { return items_.begin(); }
  iterator end() { return items_.end(); }
  const_iterator begin() const { return items_.begin(); }
  const_iterator end() const { return items_.end(); }
  const value_type& front() const { return items_.front(); }
  const value_type& back() const { return items_.back(); }

 private:
  std::vector<value_type> items_;
  std::unordered_map<K, size_t> index_;
};

}  // namespace t20
