#ifndef HASH_SET_SEQUENTIAL_H
#define HASH_SET_SEQUENTIAL_H

#include <cassert>
#include <vector>
#include <iostream>

#include "src/hash_set_base.h"

template <typename T> class HashSetSequential : public HashSetBase<T> {
public:
  explicit HashSetSequential(size_t initial_capacity) {
    table = std::vector<std::vector<T>>(initial_capacity);
    set_size = 0;
  }

  bool Add(T elem) final {
    std::vector<T>& bucket = GetBucket(elem);
    if (VectorContains(bucket, elem)) {
      return false;
    } else {
      bucket.push_back(elem);
      set_size++;
      return true;
    }
  }

  bool Remove(T elem) final {
    std::vector<T>& bucket = GetBucket(elem);
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
    std::vector<T>& bucket = GetBucket(elem);
    return VectorContains(bucket, elem);
  }

  [[nodiscard]] size_t Size() const final { return set_size; }

private:
  size_t set_size;
  std::vector<std::vector<T>> table;

  bool VectorContains(std::vector<T>& v, const T& elem) {
    for (auto it = v.begin(); it != v.end(); it++) {
      if (*it == elem) {
        return true;
      }
    }
    return false;
  }

  std::vector<T>& GetBucket(T elem) {
    return table[std::hash<T>()(elem) % table.size()];
  }
};

#endif // HASH_SET_SEQUENTIAL_H
