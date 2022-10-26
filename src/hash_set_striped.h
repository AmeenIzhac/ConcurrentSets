#ifndef HASH_SET_STRIPED_H
#define HASH_SET_STRIPED_H

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include "src/hash_set_base.h"

template <typename T> class HashSetStriped : public HashSetBase<T> {
public:
  explicit HashSetStriped(size_t initial_capacity) {
    table = std::vector<std::vector<T>>(initial_capacity);
    set_size = 0;
    for (size_t i = 0; i < initial_capacity; i++) {
      mutex_ptrs_.push_back(std::make_unique<std::mutex>());
    }
  }

  bool Add(T elem) final {
    size_t elem_hash = std::hash<T>()(elem);
    std::unique_lock<std::mutex> uniqueLock(*GetLock(elem_hash));
    std::vector<T> &bucket = GetBucket(elem_hash);
    if (VectorContains(bucket, elem)) {
      return false;
    } else {
      bucket.push_back(elem);
      set_size++;
      if (policy()) {
        uniqueLock.unlock();
        resize();
      }
      return true;
    }
  }

  bool Remove(T elem) final {
    size_t elem_hash = std::hash<T>()(elem);
    std::scoped_lock<std::mutex> scopedLock(*GetLock(elem_hash));
    std::vector<T> &bucket = GetBucket(elem_hash);
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
    size_t elem_hash = std::hash<T>()(elem);
    std::scoped_lock<std::mutex> scopedLock(*GetLock(elem_hash));
    return VectorContains(GetBucket(elem_hash), elem);
  }

  [[nodiscard]] size_t Size() const final { return set_size; }

private:
  std::atomic<std::size_t> set_size;
  std::vector<std::vector<T>> table;
  std::vector<std::unique_ptr<std::mutex>> mutex_ptrs_;

  bool VectorContains(std::vector<T> &v, const T &elem) {
    for (auto it = v.begin(); it != v.end(); it++) {
      if (*it == elem) {
        return true;
      }
    }
    return false;
  }

  std::vector<T> &GetBucket(size_t hash) { return table[hash % table.size()]; }

  std::unique_ptr<std::mutex> &GetLock(size_t hash) {
    return mutex_ptrs_[hash % mutex_ptrs_.size()];
  }

  bool policy() { return set_size / table.size(); }

  void resize() {
    size_t old_size = table.size();

    std::vector<std::unique_ptr<std::scoped_lock<std::mutex>>> locks;
    for (size_t i = 0; i < mutex_ptrs_.size(); i++) {
      locks.push_back(
          std::make_unique<std::scoped_lock<std::mutex>>(*mutex_ptrs_[i]));
    }

    if (old_size == table.size()) {
      std::vector<std::vector<T>> old_table = table;
      table = std::vector<std::vector<T>>(old_table.size() * 2);
      for (auto &bucket : old_table) {
        for (auto &elem : bucket) {
          GetBucket(std::hash<T>()(elem)).push_back(elem);
        }
      }
    }
  }
};

#endif // HASH_SET_STRIPED_H
