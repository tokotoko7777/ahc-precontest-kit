#include <cassert>
#include <utility>
#include <vector>

// 一部を書き換えて試し、採用しなければ元へ戻すための配列。
// 使い方:
// RollbackArray<int> values(initial_values);
// int snapshot = values.snapshot();
// values.set(index, new_value);
// values.rollback(snapshot);
template <class T>
struct RollbackArray {
  std::vector<T> values;
  std::vector<std::pair<int, T>> history;

  explicit RollbackArray(std::vector<T> values)
      : values(std::move(values)) {}

  int size() const { return static_cast<int>(values.size()); }

  const T& operator[](int index) const {
    assert(0 <= index && index < size());
    return values[index];
  }

  void set(int index, const T& value) {
    assert(0 <= index && index < size());
    history.emplace_back(index, values[index]);
    values[index] = value;
  }

  int snapshot() const { return static_cast<int>(history.size()); }

  void rollback(int snapshot) {
    assert(0 <= snapshot && snapshot <= static_cast<int>(history.size()));
    while (static_cast<int>(history.size()) > snapshot) {
      const auto& [index, old_value] = history.back();
      values[index] = old_value;
      history.pop_back();
    }
  }
};
