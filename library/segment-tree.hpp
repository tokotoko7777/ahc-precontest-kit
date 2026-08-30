#include <cassert>
#include <functional>
#include <utility>
#include <vector>

// 1点を変更し、区間の和・最小値・最大値などを求める Segment Tree。
// 使い方:
// SegmentTree<int> minimum(values, 1000000000,
//                          [](int a, int b) { return std::min(a, b); });
// minimum.set(index, new_value);
// int answer = minimum.query(left, right);  // [left, right)
template <class T>
struct SegmentTree {
  int n;
  int leaf_count;
  T identity;
  std::function<T(const T&, const T&)> operation;
  std::vector<T> data;

  SegmentTree(int size, T identity_element,
              std::function<T(const T&, const T&)> combine)
      : n(size),
        leaf_count(1),
        identity(std::move(identity_element)),
        operation(std::move(combine)) {
    assert(size >= 0);
    while (leaf_count < n) leaf_count *= 2;
    data.assign(leaf_count * 2, identity);
  }

  SegmentTree(const std::vector<T>& values, T identity_element,
              std::function<T(const T&, const T&)> combine)
      : SegmentTree(static_cast<int>(values.size()),
                    std::move(identity_element), std::move(combine)) {
    for (int i = 0; i < n; ++i) data[leaf_count + i] = values[i];
    for (int i = leaf_count - 1; i >= 1; --i) {
      data[i] = operation(data[i * 2], data[i * 2 + 1]);
    }
  }

  int size() const { return n; }

  void set(int index, const T& value) {
    assert(0 <= index && index < n);
    int node = leaf_count + index;
    data[node] = value;
    while (node > 1) {
      node /= 2;
      data[node] = operation(data[node * 2], data[node * 2 + 1]);
    }
  }

  const T& get(int index) const {
    assert(0 <= index && index < n);
    return data[leaf_count + index];
  }

  // values[left] から values[right - 1] を順番どおりにまとめる。
  T query(int left, int right) const {
    assert(0 <= left && left <= right && right <= n);
    T left_result = identity;
    T right_result = identity;
    left += leaf_count;
    right += leaf_count;

    while (left < right) {
      if (left & 1) left_result = operation(left_result, data[left++]);
      if (right & 1) right_result = operation(data[--right], right_result);
      left /= 2;
      right /= 2;
    }
    return operation(left_result, right_result);
  }

  T all() const { return data[1]; }
};
