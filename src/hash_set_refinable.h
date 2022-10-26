#ifndef HASH_SET_REFINABLE_H
#define HASH_SET_REFINABLE_H

#include <atomic>
#include <cassert>
#include <mutex>
#include <thread>
#include <vector>

class AtomicMarkableReference {
public:
  explicit AtomicMarkableReference(std::thread::id initial_owner,
                                   bool resizing) {
    owner = initial_owner;
    is_resizing = resizing;
  }

  std::thread::id get(bool *resizing) {
    std::scoped_lock<std::mutex> scopedLock(mutex);
    *resizing = is_resizing;
    return owner;
  }

  bool CompareAndSet(std::thread::id expected_owner, std::thread::id new_owner,
                     bool expected_bool, bool new_bool) {
    std::scoped_lock<std::mutex> scopedLock(mutex);
    if (expected_owner == owner && expected_bool == is_resizing) {
      owner = new_owner;
      is_resizing = new_bool;
      return true;
    }
    return false;
  }

private:
  std::thread::id owner;
  bool is_resizing = false;
  std::mutex mutex;
};

#include "src/hash_set_base.h"

template <typename T> class HashSetRefinable : public HashSetBase<T> {
public:
  explicit HashSetRefinable(size_t initial_capacity) {
    table = std::vector<std::vector<T>>(initial_capacity);
    owner = new AtomicMarkableReference(std::this_thread::get_id(), false);
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
        set_size--;
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
  [[nodiscard]] size_t Size() const final { return set_size; }

private:
  std::atomic<std::size_t> set_size;
  std::vector<std::vector<T>> table;
  AtomicMarkableReference *owner;
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

  // Acquires specific lock for elem
  void acquire(T elem) {
    bool mark = true;
    std::thread::id this_thread = std::this_thread::get_id();
    std::thread::id current_owner;
    while (true) {
      do {
        current_owner = owner->get(&mark);
        // Prevents threads from acquiring locks whilst the table is being
        // resized.
      } while (mark && current_owner != this_thread);
      std::vector<std::unique_ptr<std::mutex>> old_mutexes = mutex_ptrs_;
      std::mutex *old_lock =
          old_mutexes[std::hash<T>()(elem) % old_mutexes.size()].get();
      old_lock->lock();
      current_owner = owner->get(&mark);
      if (!mark || (current_owner == this_thread)) {
        return;
      } else {
        old_lock->unlock();
      }
    }
  }

  // Release specific lock for elem
  void release(T elem) {
    mutex_ptrs_[std::hash<T>()(elem) & mutex_ptrs_.size()].get()->unlock();
  }

  // Checks all locks are unlocked before the table can be resized.
  void quiesce() {
    for (int i = 0; i < mutex_ptrs_.size(); i++) {
      std::mutex *mutex = mutex_ptrs_[i].get();
      // loops until mutex is unlocked ()
      while (!mutex->try_lock()) {
      }
      mutex->unlock();
    }
  }

  void Resize2() {
    /*bool mark = true;
    std::thread::id this_thread = std::this_thread::get_id();
    size_t old_size = table.size();

    if(owner->CompareAndSet(std::thread::id, this_thread, false, true)) {
      if (table.size() != old_size){
        return
      }

      quiesce();

      //copy table
      if (old_size == table.size()) {
      std::vector<std::vector<T>> old_table = table;
      table = std::vector<std::vector<T>>(old_table.size() * 2);
      for (auto &bucket : old_table) {
        for (auto &elem : bucket) {
          GetBucket(std::hash<T>()(elem)).push_back(elem);
        }
      }

      //copy locks and duplicate locks here too


    }

    } */
  }

  std::vector<T> &GetBucket(size_t hash) { return table[hash % table.size()]; }

  // returns the the mutex corresponding to the hash,
  std::mutex *GetLock(size_t hash) {
    return mutex_ptrs_[hash % mutex_ptrs_.size()].get();
  }

  // Average length of bucket is greater than 4
  bool Policy() { return set_size / table.size(); }

  // Doubles bucket vector and puts elements into new buckets
  void Resize() {
    size_t old_size = table.size();

    // Locks every mutex in a list of scopedlocks, ensures the set is not
    // modified during resizing.
    // These locks are unlocked once resising finishes (assumes no locks are
    // held)
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

#endif // HASH_SET_REFINABLE_H
