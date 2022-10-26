#ifndef HASH_SET_SEQUENTIAL_H
#define HASH_SET_SEQUENTIAL_H

#include <cassert>
#include <vector>

#include "src/hash_set_base.h"

template <typename T>
class HashSetSequential : public HashSetBase<T>
{
public:
  explicit HashSetSequential(size_t initial_capacity)
  {
    table = std::vector<std::vector<T>>(initial_capacity);
    set_size_ = 0;
  }

  // Finds the bucket corresponding to the elems hash and inserts the element to that bucket
  bool Add(T elem) final
  {
    std::vector<T> &bucket = GetBucket(elem);
    if (VectorContains(bucket, elem))
    {
      return false;
    }
    else
    {
      bucket.push_back(elem);
      set_size_++;
      if (Policy())
      {
        Resize();
      }
      return true;
    }
  }

  // Finds the bucket corresponding to the elems hash and removes the element from that bucket
  bool Remove(T elem) final
  {
    std::vector<T> &bucket = GetBucket(elem);
    for (auto it = bucket.begin(); it != bucket.end(); it++)
    {
      if (*it == elem)
      {
        bucket.erase(it);
        set_size_--;
        return true;
      }
    }
    return false;
  }

  // Returns true iffthe element is contained in the HashSet and false otherwise
  [[nodiscard]] bool Contains(T elem) final
  {
    std::vector<T> &bucket = GetBucket(elem);
    return VectorContains(bucket, elem);
  }

  // Returns the total amount of elements in hashset
  [[nodiscard]] size_t Size() const final { return set_size_; }

private:
  size_t set_size_;
  std::vector<std::vector<T>> table;

  // Helper to Contains returning true iff an element is contained in a bucket
  bool VectorContains(std::vector<T> &v, const T &elem)
  {
    for (auto it = v.begin(); it != v.end(); it++)
    {
      if (*it == elem)
      {
        return true;
      }
    }
    return false;
  }

  // Returns corresponding bucket for elem based on it's hash
  std::vector<T> &GetBucket(T elem)
  {
    return table[std::hash<T>()(elem) % table.size()];
  }

  // Average length of bucket is greater than 4
  bool Policy() { return set_size_ / table.size() > 4; }

  // Doubles bucket vector and puts elements into new buckets
  void Resize()
  {
    std::vector<std::vector<T>> old_table = table;
    table = std::vector<std::vector<T>>(old_table.size() * 2);
    for (auto &bucket : old_table)
    {
      for (auto &elem : bucket)
      {
        GetBucket(elem).push_back(elem);
      }
    }
  }
};

#endif // HASH_SET_SEQUENTIAL_H
