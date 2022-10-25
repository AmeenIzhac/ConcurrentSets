#ifndef HASH_SET_COARSE_GRAINED_H
#define HASH_SET_COARSE_GRAINED_H

#include <cassert>
#include <mutex>
#include <vector>

#include "src/hash_set_base.h"

template <typename T> class HashSetCoarseGrained : public HashSetBase<T> {
public:
  explicit HashSetCoarseGrained(size_t initial_capacity) {
    table_ = std::vector<std::vector<T>>(initial_capacity);
    set_size = 0;
  }

  bool Add(T elem) final {
    std::scoped_lock<std::mutex> lock(mutex_);
    std::vector<T> &bucket = GetBucket(elem);
    if (VectorContains(bucket, elem)) {
      return false;
    } else {
      bucket.push_back(elem);
      set_size++;
      if (policy()) {
        resize();
      }
      return true;
    }
  }

  bool Remove(T elem) final {
    std::scoped_lock<std::mutex> lock(mutex_);
    std::vector<T> &bucket = GetBucket(elem);
    for (auto it = bucket.begin(); it != bucket.end(); it++) {
      if (*it == elem) {
        bucket.erase(it);
        set_size--;
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool Contains(T elem) final {
    std::scoped_lock<std::mutex> lock(mutex_);
    std::vector<T> &bucket = GetBucket(elem);
    return VectorContains(bucket, elem);
  }

  [[nodiscard]] size_t Size() const final { return set_size; }

private:
  size_t set_size;
  std::vector<std::vector<T>> table_;
  std::mutex mutex_;

  bool VectorContains(std::vector<T> &v, const T &elem) {
    for (auto it = v.begin(); it != v.end(); it++) {
      if (*it == elem) {
        return true;
      }
    }
    return false;
  }

  std::vector<T> &GetBucket(T elem) {
    return table_[std::hash<T>()(elem) % table_.size()];
  }

  bool policy() { return set_size / table_.size() > 4; }

  void resize() {
    std::vector<std::vector<T>> old_table = table_;
    table_ = std::vector<std::vector<T>>(old_table.size() * 2);
    for (auto &bucket : old_table) {
      for (auto &elem : bucket) {
        GetBucket(elem).push_back(elem);
      }
    }
  }
};
#endif // HASH_SET_COARSE_GRAINED_H
