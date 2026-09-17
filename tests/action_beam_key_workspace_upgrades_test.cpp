#include <array>
#include <cassert>
#include <map>
#include <random>
#include <string>
#include "library/action-beam-search.hpp"

struct KeyState { int rank = 0; std::vector<int> path; };
struct KeyExpected { KeyState state; double score; std::size_t order; };

struct CollisionHash {
  std::size_t salt;
  std::size_t operator()(std::uint64_t) const { return salt; }
};

template <class Hash>
void compare_keyed(Hash hash, bool maximize, bool same_score) {
  ActionBeamSearch<KeyState, int, double> beam({}, 0, 23, maximize);
  std::vector<KeyExpected> reference{{{}, 0, 0}};
  std::mt19937 random(731);
  auto better = [=](const KeyExpected& a, const KeyExpected& b) {
    const bool an = std::isnan(a.score), bn = std::isnan(b.score);
    if (an != bn) return !an;
    if (!an && a.score != b.score) return maximize ? a.score > b.score : a.score < b.score;
    return a.order < b.order;
  };
  for (int depth = 0; depth < 12; ++depth) {
    // Resize/clear/release and different collision rates across generations.
    const int branches = depth % 3 == 0 ? 67 : 9;
    const int groups = depth % 4 == 0 ? 127 : 13;
    std::vector<std::vector<double>> scores(reference.size());
    std::vector<std::vector<std::uint64_t>> keys(reference.size());
    std::map<std::uint64_t, KeyExpected> best;
    std::size_t order = 0;
    for (std::size_t p = 0; p < reference.size(); ++p) {
      for (int a = 0; a < branches; ++a) {
        double score = same_score ? 0.0 : double(int(random() % 17) - 8);
        if (!same_score && a % 19 == 0) score = std::numeric_limits<double>::quiet_NaN();
        // Identical low 40 bits must not be treated as identical keys.
        const std::uint64_t key = std::uint64_t(random() % groups) << 40;
        scores[p].push_back(score);
        keys[p].push_back(key);
        auto state = reference[p].state;
        state.path.push_back(a);
        KeyExpected candidate{std::move(state), score, order++};
        auto found = best.find(key);
        if (found == best.end()) best.emplace(key, std::move(candidate));
        else if (better(candidate, found->second)) found->second = std::move(candidate);
      }
    }
    std::vector<KeyExpected> next;
    for (auto& entry : best) next.push_back(std::move(entry.second));
    std::sort(next.begin(), next.end(), better);
    if (next.size() > std::size_t(beam.width())) next.resize(beam.width());
    auto expand = [=](const KeyState&) {
      std::vector<int> result(branches);
      std::iota(result.begin(), result.end(), 0);
      return result;
    };
    int rank = 0, evaluated = 0, keyed = 0;
    assert(beam.step_with_key(expand,
        [&](const KeyState& s, int a) { ++evaluated; return scores[s.rank][a]; },
        [&](const KeyState& s, int a) { ++keyed; return keys[s.rank][a]; },
        hash, std::equal_to<std::uint64_t>{},
        [&](KeyState& s, int a) { s.path.push_back(a); s.rank = rank++; }));
    assert(evaluated == int(order) && keyed == int(order));
    assert(beam.last_generated_count() == order);
    assert(beam.last_unique_count() == best.size());
    assert(beam.last_kept_count() == next.size());
    for (std::size_t i = 0; i < next.size(); ++i) {
      next[i].state.rank = int(i);
      assert(beam.states()[i].path == next[i].state.path);
      assert(beam.scores()[i] == next[i].score ||
             (std::isnan(beam.scores()[i]) && std::isnan(next[i].score)));
    }
    reference = std::move(next);
    if (depth == 3) beam.release_memory();
    if (depth == 6) {
      beam.reset({}, 0);
      reference = {{{}, 0, 0}};
      beam.set_width(41);
    }
  }
}

void test_key_type_switch() {
  ActionBeamSearch<int, int, int> beam(0, 0, 4);
  auto expand = [](int) { return std::array<int, 4>{{1, 2, 3, 4}}; };
  auto eval = [](int, int a) { return a; };
  auto apply = [](int& s, int a) { s = a; };
  assert(beam.step_with_key(expand, eval, [](int, int a) { return std::to_string(a % 2); }, apply));
  assert(beam.states() == std::vector<int>({4, 3}));
  assert(beam.step_with_key(expand, eval, [](int, int) { return 0U; }, apply));
  assert(beam.states() == std::vector<int>({4}));
  assert(!beam.step_with_key([](int) { return std::vector<int>{}; }, eval,
                            [](int, int a) { return a; }, apply));
  assert(beam.last_unique_count() == 0);
  assert(beam.step(expand, eval, apply));
  assert(beam.states() == std::vector<int>({4, 3, 2, 1}));
}

int main() {
  test_key_type_switch();
  for (bool maximize : {false, true}) for (bool ties : {false, true}) {
    compare_keyed(std::hash<std::uint64_t>{}, maximize, ties);
    compare_keyed(CollisionHash{7}, maximize, ties);
  }
}
