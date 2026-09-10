#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "library/tree-beam-search.hpp"

// AHC021 "Pyramid Sorting" の公式入力分布と公式得点を使う。
// https://atcoder.jp/contests/ahc021/tasks/ahc021_a
//
// 小さい番号から「親が全て確定済みのマス」へ運ぶ。
// 同じ距離なら大きい球を下へ押し下げる経路を高く評価し、
// 次の球をどの前線マスへ運ぶかをビームで比較する。
//
// 【問題に合わせて書き換える場所】
//   PyramidProblem の State / Move と5関数。
//   generate_moves / apply_move / revert_move / evaluate / make_key
//
// 【ライブラリが担当する場所】
//   Stateは1個だけ保持する。履歴木のDFS、apply/revertの呼び分け、
//   key重複除去、上位N個の選択、465世代のループはTreeBeamRunnerが行う。

constexpr int N = 30;
constexpr int CELL_COUNT = N * (N + 1) / 2;
constexpr int MAX_OPERATIONS = 10000;

struct PyramidProblem {
  // TODO: 【問題ごと】1手とundoに必要な情報をMoveへ書く。
  struct Move {
    // path[0]に対象の小さい球がいる。隣へ順番にswapし、
    // path.back()を今回の確定マスにする。
    std::vector<std::uint16_t> path;
    int rank_cost = 0;
  };

  // TODO: 【問題ごと】現在状態と、差分更新するscore・hash・cacheを書く。
  struct State {
    std::array<std::uint16_t, CELL_COUNT> value{};
    std::array<std::uint16_t, CELL_COUNT> position_of_value{};
    std::array<std::uint8_t, CELL_COUNT> fixed{};
    int next_value = 0;
    int operations = 0;
    long long rank_cost = 0;
    std::uint64_t hash = 0;
  };

  // TODO: 【問題ごと】候補順位の型を選ぶ。この例は小さい方が良い。
  using Score = long long;

  // TODO: 【問題ごと】全Stateで共通の入力・隣接表・事前計算を置く。
  std::array<int, CELL_COUNT> row{};
  std::array<int, CELL_COUNT> column{};
  std::array<std::vector<int>, CELL_COUNT> adjacent;

  PyramidProblem() {
    for (int r = 0; r < N; ++r) {
      for (int c = 0; c <= r; ++c) {
        const int vertex = id(r, c);
        row[vertex] = r;
        column[vertex] = c;
      }
    }
    for (int r = 0; r < N; ++r) {
      for (int c = 0; c <= r; ++c) {
        const int vertex = id(r, c);
        const auto add = [&](int next_row, int next_column) {
          if (next_row < 0 || next_row >= N || next_column < 0 ||
              next_column > next_row) {
            return;
          }
          adjacent[vertex].push_back(id(next_row, next_column));
        };
        add(r, c - 1);
        add(r, c + 1);
        add(r - 1, c - 1);
        add(r - 1, c);
        add(r + 1, c);
        add(r + 1, c + 1);
      }
    }
  }

  static int id(int row, int column) {
    return row * (row + 1) / 2 + column;
  }

  static std::uint64_t hash_token(int position, int value) {
    std::uint64_t x = static_cast<std::uint64_t>(position) * CELL_COUNT +
                      value + 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  // TODO: 【問題ごと】必ず合法な初期Stateを作る。
  State initial_state(const std::array<int, CELL_COUNT>& permutation) const {
    State state;
    for (int position = 0; position < CELL_COUNT; ++position) {
      const int value = permutation[position];
      state.value[position] = static_cast<std::uint16_t>(value);
      state.position_of_value[value] =
          static_cast<std::uint16_t>(position);
      state.hash ^= hash_token(position, value);
    }
    return state;
  }

  bool parents_are_fixed(const State& state, int vertex) const {
    const int r = row[vertex];
    const int c = column[vertex];
    if (r == 0) return true;
    if (c > 0 && !state.fixed[id(r - 1, c - 1)]) return false;
    if (c < r && !state.fixed[id(r - 1, c)]) return false;
    return true;
  }

  // 小さい球が上へ動く時、交換相手の大きい球が下へ下がるほど嬉しい。
  int edge_cost(const State& state, int from, int to) const {
    if (row[to] < row[from]) return CELL_COUNT - state.value[to];
    return CELL_COUNT;
  }

  // TODO: 【問題ごと】現在Stateから試す合法Moveを全て返す。
  std::vector<Move> generate_moves(const State& state) const {
    if (state.next_value == CELL_COUNT) return {};
    const int start = state.position_of_value[state.next_value];
    constexpr int INF = std::numeric_limits<int>::max() / 4;
    std::array<int, CELL_COUNT> distance;
    std::array<int, CELL_COUNT> previous;
    distance.fill(INF);
    previous.fill(-1);
    using QueueEntry = std::pair<int, int>;
    std::priority_queue<QueueEntry,
                        std::vector<QueueEntry>,
                        std::greater<QueueEntry>>
        queue;
    distance[start] = 0;
    queue.push({0, start});
    while (!queue.empty()) {
      const auto [current_distance, vertex] = queue.top();
      queue.pop();
      if (distance[vertex] != current_distance) continue;
      for (int next : adjacent[vertex]) {
        if (state.fixed[next]) continue;
        const int next_distance =
            current_distance + edge_cost(state, vertex, next);
        if (next_distance < distance[next]) {
          distance[next] = next_distance;
          previous[next] = vertex;
          queue.push({next_distance, next});
        }
      }
    }

    std::vector<Move> moves;
    moves.reserve(N);
    for (int target = 0; target < CELL_COUNT; ++target) {
      if (state.fixed[target] || !parents_are_fixed(state, target) ||
          distance[target] == INF) {
        continue;
      }
      Move move;
      move.rank_cost = distance[target];
      for (int vertex = target; vertex != -1; vertex = previous[vertex]) {
        move.path.push_back(static_cast<std::uint16_t>(vertex));
        if (vertex == start) break;
      }
      if (move.path.empty() || move.path.back() != start) continue;
      std::reverse(move.path.begin(), move.path.end());
      moves.push_back(std::move(move));
    }
    std::sort(moves.begin(), moves.end(), [](const Move& left,
                                              const Move& right) {
      if (left.rank_cost != right.rank_cost) {
        return left.rank_cost < right.rank_cost;
      }
      return left.path.back() < right.path.back();
    });
    return moves;
  }

  void swap_vertices(State& state, int first, int second) const {
    const int first_value = state.value[first];
    const int second_value = state.value[second];
    state.hash ^= hash_token(first, first_value);
    state.hash ^= hash_token(second, second_value);
    state.hash ^= hash_token(first, second_value);
    state.hash ^= hash_token(second, first_value);
    std::swap(state.value[first], state.value[second]);
    state.position_of_value[first_value] =
        static_cast<std::uint16_t>(second);
    state.position_of_value[second_value] =
        static_cast<std::uint16_t>(first);
  }

  // TODO: 【問題ごと】盤面と全cacheをMove 1手分だけ差分更新する。
  void apply_move(State& state, Move& move) const {
    for (std::size_t i = 1; i < move.path.size(); ++i) {
      swap_vertices(state, move.path[i - 1], move.path[i]);
    }
    const int target = move.path.back();
    state.fixed[target] = 1;
    ++state.next_value;
    state.operations += static_cast<int>(move.path.size()) - 1;
    state.rank_cost += move.rank_cost;
  }

  // TODO: 【問題ごと】apply直前と完全に同じStateへ戻す。
  void revert_move(State& state, const Move& move) const {
    const int target = move.path.back();
    state.rank_cost -= move.rank_cost;
    state.operations -= static_cast<int>(move.path.size()) - 1;
    --state.next_value;
    state.fixed[target] = 0;
    for (std::size_t i = move.path.size(); i > 1; --i) {
      swap_vertices(state, move.path[i - 1], move.path[i - 2]);
    }
  }

  // TODO: 【問題ごと】現在Stateの順位値そのものを返す。差分値ではない。
  Score evaluate(const State& state) const { return state.rank_cost; }
  // TODO: 【必要な問題だけ】同じ未来を持つ局面が同じになるkeyを返す。
  std::uint64_t make_key(const State& state) const { return state.hash; }

  int count_errors(const State& state) const {
    int errors = 0;
    for (int r = 0; r + 1 < N; ++r) {
      for (int c = 0; c <= r; ++c) {
        const int upper = id(r, c);
        errors += state.value[upper] > state.value[id(r + 1, c)];
        errors += state.value[upper] > state.value[id(r + 1, c + 1)];
      }
    }
    return errors;
  }

  int official_score(const State& state) const {
    const int errors = count_errors(state);
    if (errors == 0) return 100000 - 5 * state.operations;
    return 50000 - 50 * errors;
  }

  void validate(const State& state) const {
    if (state.next_value != CELL_COUNT || state.operations > MAX_OPERATIONS ||
        count_errors(state) != 0) {
      throw std::runtime_error("constructed pyramid is not a legal solution");
    }
    std::uint64_t expected_hash = 0;
    for (int position = 0; position < CELL_COUNT; ++position) {
      const int value = state.value[position];
      if (state.position_of_value[value] != position ||
          !state.fixed[position]) {
        throw std::runtime_error("position or fixed cache is inconsistent");
      }
      expected_hash ^= hash_token(position, value);
    }
    if (expected_hash != state.hash) {
      throw std::runtime_error("incremental hash is inconsistent");
    }
  }
};

std::array<int, CELL_COUNT> make_case(std::uint64_t seed) {
  std::array<int, CELL_COUNT> permutation{};
  std::iota(permutation.begin(), permutation.end(), 0);
  std::mt19937_64 engine(seed);
  std::shuffle(permutation.begin(), permutation.end(), engine);
  return permutation;
}

struct BenchmarkResult {
  int score = 0;
  int operations = 0;
  double milliseconds = 0.0;
};

BenchmarkResult solve_case(const std::array<int, CELL_COUNT>& input,
                           int width) {
  PyramidProblem problem;
  const PyramidProblem::State initial = problem.initial_state(input);
  TreeBeamRunner<PyramidProblem> beam(
      problem, initial, problem.evaluate(initial), width, false);
  beam.reserve_nodes(1 + static_cast<std::size_t>(width) * CELL_COUNT);
  beam.reserve_candidates(static_cast<std::size_t>(width) * N);

  const auto start = std::chrono::steady_clock::now();
  const int advanced = beam.run_with_key(CELL_COUNT);
  const auto finish = std::chrono::steady_clock::now();
  if (advanced != CELL_COUNT) {
    throw std::runtime_error("beam stopped before placing every value");
  }

  const std::vector<PyramidProblem::Move> answer = beam.restore();
  PyramidProblem::State check = initial;
  for (PyramidProblem::Move move : answer) problem.apply_move(check, move);
  problem.validate(check);
  if (problem.evaluate(check) != beam.best_score()) {
    throw std::runtime_error("restored answer and rank score disagree");
  }
  return {problem.official_score(check),
          check.operations,
          std::chrono::duration<double, std::milli>(finish - start).count()};
}

int main(int argc, char** argv) {
  int case_count = argc >= 2 ? std::stoi(argv[1]) : 5;
  if (case_count <= 0) {
    throw std::invalid_argument("case_count must be positive");
  }
  std::vector<int> widths{1, 10, 40};
  if (argc >= 3) {
    widths.clear();
    for (int i = 2; i < argc; ++i) {
      const int width = std::stoi(argv[i]);
      if (width <= 0) throw std::invalid_argument("width must be positive");
      widths.push_back(width);
    }
  }

  std::vector<long long> total_score(widths.size());
  std::vector<long long> total_operations(widths.size());
  std::vector<double> total_milliseconds(widths.size());
  std::cout << "AHC021 apply/revert tree beam score benchmark\n";
  std::cout << "usage: ahc021_tree_beam_score_benchmark "
               "[case_count [width ...]]\n";
  std::cout << "seed width score operations milliseconds\n";
  for (int seed = 0; seed < case_count; ++seed) {
    const auto input = make_case(static_cast<std::uint64_t>(seed));
    for (std::size_t index = 0; index < widths.size(); ++index) {
      const BenchmarkResult result = solve_case(input, widths[index]);
      total_score[index] += result.score;
      total_operations[index] += result.operations;
      total_milliseconds[index] += result.milliseconds;
      std::cout << seed << ' ' << widths[index] << ' ' << result.score << ' '
                << result.operations << ' ' << std::fixed
                << std::setprecision(3) << result.milliseconds << '\n';
    }
  }
  std::cout << "summary_width cases score_sum average_score "
               "average_operations milliseconds\n";
  for (std::size_t index = 0; index < widths.size(); ++index) {
    std::cout << widths[index] << ' ' << case_count << ' '
              << total_score[index] << ' ' << std::fixed
              << std::setprecision(3)
              << static_cast<double>(total_score[index]) / case_count << ' '
              << static_cast<double>(total_operations[index]) / case_count
              << ' ' << total_milliseconds[index] << '\n';
  }
  std::cout << "reference_reported_official_150_cases early_top_score "
               "13426585 average 89510.567\n";
  std::cout << "reference_note: generated shuffles differ; compare only as a "
               "rough distribution-level guide\n";
}
