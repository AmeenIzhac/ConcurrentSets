#ifndef HASH_SET_STRIPED_H
#define HASH_SET_STRIPED_H

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <vector>

#include "src/hash_set_base.h"

template <typename T> class HashSetStriped : public HashSetBase<T> {
public:
  explicit HashSetStriped(size_t initial_capacity) {
    table_ = std::vector<std::vector<T>>(initial_capacity);
    set_size_ = 0;
    for (size_t i = 0; i < initial_capacity; i++) {
      mutex_ptrs_.push_back(std::make_unique<std::mutex>());
    }
  }

  // Finds the bucket corresponding to the elems hash and inserts the element to
  // that bucket Unique lock is needed here to unlock before call to Resize()
  bool Add(T elem) final {
    size_t elem_hash = std::hash<T>()(elem);
    std::unique_lock<std::mutex> uniqueLock(*GetLock(elem_hash));
    std::vector<T> &bucket = GetBucket(elem_hash);
    if (VectorContains(bucket, elem)) {
      return false;
    } else {
      bucket.push_back(elem);
      set_size_++;
      if (Policy()) {
        // Cannot hold any locks when calling resize as
        uniqueLock.unlock();
        Resize();
      }
      return true;
    }
  }

  // Finds the bucket corresponding to the elems hash and removes the element
  // from that bucket
  bool Remove(T elem) final {
    size_t elem_hash = std::hash<T>()(elem);
    std::scoped_lock<std::mutex> scopedLock(*GetLock(elem_hash));
    std::vector<T> &bucket = GetBucket(elem_hash);
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
    size_t elem_hash = std::hash<T>()(elem);
    std::scoped_lock<std::mutex> scopedLock(*GetLock(elem_hash));
    return VectorContains(GetBucket(elem_hash), elem);
  }

  // Returns total size of HashSet
  [[nodiscard]] size_t Size() const final { return set_size_; }

private:
  std::atomic<std::size_t> set_size_;
  std::vector<std::vector<T>> table_;
  // List of mutexes corresponding to every initial bucket bucket
  std::vector<std::unique_ptr<std::mutex>> mutex_ptrs_;

  // Helper to Contains returning true iff an element is contained in a bucket
  bool VectorContains(std::vector<T> &v, const T &elem) {
    for (auto it = v.begin(); it != v.end(); it++) {
      if (*it == elem) {
        return true;
      }
    }
    return false;
  }

  std::vector<T> &GetBucket(size_t hash) {
    return table_[hash % table_.size()];
  }

  // returns the the mutex corresponding to the hash,
  std::mutex *GetLock(size_t hash) {
    return mutex_ptrs_[hash % mutex_ptrs_.size()].get();
  }

  // Average length of bucket is greater than 4
  bool Policy() { return set_size_ / table_.size(); }

  // Doubles bucket vector and puts elements into new buckets
  void Resize() {
    size_t old_size = table_.size();

    // Locks every mutex in a list of scopedlocks, ensures the set is not
    // modified during resizing.
    // These locks are unlocked once resising finishes (assumes no locks are
    // held)
    std::vector<std::unique_ptr<std::scoped_lock<std::mutex>>> locks;
    for (size_t i = 0; i < mutex_ptrs_.size(); i++) {
      locks.push_back(
          std::make_unique<std::scoped_lock<std::mutex>>(*mutex_ptrs_[i]));
    }

    if (old_size == table_.size()) {
      std::vector<std::vector<T>> old_table = table_;
      table_ = std::vector<std::vector<T>>(old_table.size() * 2);
      for (auto &bucket : old_table) {
        for (auto &elem : bucket) {
          GetBucket(std::hash<T>()(elem)).push_back(elem);
        }
      }
    }
  }
};

#endif // HASH_SET_STRIPED_H
