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
    set_size_ = 0;
  }

  // Finds the bucket corresponding to the elems hash and inserts the element to
  // that bucket
  bool Add(T elem) final {
    std::scoped_lock<std::mutex> lock(mutex_);
    std::vector<T> &bucket = GetBucket(elem);
    if (VectorContains(bucket, elem)) {
      return false;
    } else {
      bucket.push_back(elem);
      set_size_++;
      if (Policy()) {
        Resize();
      }
      return true;
    }
  }

  // Finds the bucket corresponding to the elems hash and removes the element
  // from that bucket
  bool Remove(T elem) final {
    std::scoped_lock<std::mutex> lock(mutex_);
    std::vector<T> &bucket = GetBucket(elem);
    for (auto it = bucket.begin(); it != bucket.end(); it++) {
      if (*it == elem) {
        bucket.erase(it);
        set_size_--;
        return true;
      }
    }
    return false;
  }

  // Returns true if the element is contained in the HashSet and false otherwise
  [[nodiscard]] bool Contains(T elem) final {
    std::scoped_lock<std::mutex> lock(mutex_);
    std::vector<T> &bucket = GetBucket(elem);
    return VectorContains(bucket, elem);
  }

  // Returns the total amount of elements in hashset
  [[nodiscard]] size_t Size() const final { return set_size_; }

private:
  size_t set_size_;
  std::vector<std::vector<T>> table_;
  std::mutex mutex_;

  // Helper to Contains returning true iff an element is contained in a bucket
  bool VectorContains(std::vector<T> &v, const T &elem) {
    for (auto it = v.begin(); it != v.end(); it++) {
      if (*it == elem) {
        return true;
      }
    }
    return false;
  }

  // Helper to return corresponding bucket for elem based on it's hash
  std::vector<T> &GetBucket(T elem) {
    return table_[std::hash<T>()(elem) % table_.size()];
  }

  // Returns true iff the average bucket is holding more than 4 items
  bool Policy() { return set_size_ / table_.size() > 4; }

  // Creates a new table twice the size of the old table and reinserts all the
  // old elements
  void Resize() {
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
