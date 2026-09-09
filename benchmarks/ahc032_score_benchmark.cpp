#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "library/action-beam-search.hpp"

// AHC032 "Mod Stamp" と同じ大きさ・入力分布・得点規則を使う、
// ActionBeamRunnerの固定seedベンチマーク。
// 実行時間だけでなく、探索で得た問題本来の得点を比較する。
//
// 公式generatorそのものではないため、AtCoderの提出得点とは比較しない。
// 全値を[0, 998244352]から一様生成し、同じseedなら常に同じ問題を作る。

constexpr int BOARD_SIZE = 9;
constexpr int STAMP_SIZE = 3;
constexpr int STAMP_COUNT = 20;
constexpr int OPERATION_LIMIT = 81;
constexpr int PLACEMENTS_PER_AXIS = BOARD_SIZE - STAMP_SIZE + 1;
constexpr int PLACEMENT_COUNT =
    PLACEMENTS_PER_AXIS * PLACEMENTS_PER_AXIS;
constexpr std::uint32_t MODULO = 998244353U;

struct SplitMix64 {
  std::uint64_t state;

  std::uint64_t next() {
    std::uint64_t value = (state += 0x9e3779b97f4a7c15ULL);
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
  }

  std::uint32_t below(std::uint32_t bound) {
    // 2^64がboundで割り切れない時の剰余biasをrejectionで除く。
    const std::uint64_t threshold = (std::uint64_t{0} - bound) % bound;
    while (true) {
      const std::uint64_t value = next();
      if (value >= threshold) {
        return static_cast<std::uint32_t>(value % bound);
      }
    }
  }
};

struct ModStampInstance {
  std::array<std::uint32_t, BOARD_SIZE * BOARD_SIZE> board{};
  std::array<std::array<std::uint32_t, STAMP_SIZE * STAMP_SIZE>,
             STAMP_COUNT>
      stamps{};
};

ModStampInstance make_instance(std::uint64_t seed) {
  SplitMix64 random{seed};
  ModStampInstance instance;
  for (std::uint32_t& value : instance.board) {
    value = random.below(MODULO);
  }
  for (auto& stamp : instance.stamps) {
    for (std::uint32_t& value : stamp) {
      value = random.below(MODULO);
    }
  }
  return instance;
}

long long board_score(
    const std::array<std::uint32_t, BOARD_SIZE * BOARD_SIZE>& board) {
  long long score = 0;
  for (std::uint32_t value : board) score += value;
  return score;
}

struct ModStampProblem {
  struct State {
    std::array<std::uint32_t, BOARD_SIZE * BOARD_SIZE> board{};
    // choices[position]は、その置き場所で選んだComboの番号。
    std::array<std::uint16_t, PLACEMENT_COUNT> choices{};
    int position = 0;
    int operations = 0;
    long long finalized_score = 0;
  };

  // Actionには3x3加算値を持たせず、Problem側のComboを引く番号だけを置く。
  // 全候補ぶん保存されるデータを2 byteに抑える。
  using Action = std::uint16_t;
  using Score = long long;

  struct Combo {
    std::array<std::uint32_t, STAMP_SIZE * STAMP_SIZE> add{};
    std::array<std::uint8_t, 3> stamp_ids{};
    std::uint8_t count = 0;
  };

  explicit ModStampProblem(ModStampInstance input)
      : instance(std::move(input)) {
    build_combinations();
  }

  State initial_state() const {
    State state;
    state.board = instance.board;
    return state;
  }

  Score initial_score() const {
    // まだ確定したマスがないので順位値は0。
    return 0;
  }

  // 残り操作回数に収まる、0～3個のスタンプの組合せを返す。
  // allowed_actionsはProblemが保持するので、毎Stateでvectorを作らず参照を返せる。
  const std::vector<Action>& generate_actions(const State& state) const {
    const int remaining = OPERATION_LIMIT - state.operations;
    return allowed_actions[static_cast<std::size_t>(
        remaining < 3 ? remaining : 3)];
  }

  // 次の置き場所を処理すると、そこより右下のスタンプでは二度と変更できない
  // マスが生じる。その「新しく確定するマス」の合計を順位値へ加える。
  // 最終世代では全81マスが確定するため、この順位値が問題本来の得点になる。
  Score evaluate_action(const State& state, const Action& action) const {
    const int row = state.position / PLACEMENTS_PER_AXIS;
    const int column = state.position % PLACEMENTS_PER_AXIS;
    const int finalized_rows =
        row == PLACEMENTS_PER_AXIS - 1 ? STAMP_SIZE : 1;
    const int finalized_columns =
        column == PLACEMENTS_PER_AXIS - 1 ? STAMP_SIZE : 1;
    const Combo& combo = combinations[action];

    Score score = state.finalized_score;
    for (int di = 0; di < finalized_rows; ++di) {
      for (int dj = 0; dj < finalized_columns; ++dj) {
        const std::size_t board_index = static_cast<std::size_t>(
            (row + di) * BOARD_SIZE + column + dj);
        const std::size_t stamp_index =
            static_cast<std::size_t>(di * STAMP_SIZE + dj);
        score += add_mod(state.board[board_index], combo.add[stamp_index]);
      }
    }
    return score;
  }

  // 選ばれたComboだけを、コピー済みの子Stateへ反映する。
  void apply_action(State& state, Action& action) const {
    const int row = state.position / PLACEMENTS_PER_AXIS;
    const int column = state.position % PLACEMENTS_PER_AXIS;
    const Combo& combo = combinations[action];

    for (int di = 0; di < STAMP_SIZE; ++di) {
      for (int dj = 0; dj < STAMP_SIZE; ++dj) {
        const std::size_t board_index = static_cast<std::size_t>(
            (row + di) * BOARD_SIZE + column + dj);
        const std::size_t stamp_index =
            static_cast<std::size_t>(di * STAMP_SIZE + dj);
        state.board[board_index] =
            add_mod(state.board[board_index], combo.add[stamp_index]);
      }
    }

    // evaluate_actionが返した値と同じ順位値をStateへ保存する。
    const int finalized_rows =
        row == PLACEMENTS_PER_AXIS - 1 ? STAMP_SIZE : 1;
    const int finalized_columns =
        column == PLACEMENTS_PER_AXIS - 1 ? STAMP_SIZE : 1;
    for (int di = 0; di < finalized_rows; ++di) {
      for (int dj = 0; dj < finalized_columns; ++dj) {
        state.finalized_score += state.board[static_cast<std::size_t>(
            (row + di) * BOARD_SIZE + column + dj)];
      }
    }

    state.choices[static_cast<std::size_t>(state.position)] = action;
    state.operations += combo.count;
    ++state.position;
  }

  // 操作上限、盤面更新、最終得点を探索本体とは別経路で再計算する。
  // benchmarkが高得点を表示していても、ここに失敗すれば不正解として停止する。
  void validate(const State& answer) const {
    if (answer.position != PLACEMENT_COUNT) {
      throw std::runtime_error("beam did not reach the final placement");
    }

    auto board = instance.board;
    int operations = 0;
    for (int position = 0; position < PLACEMENT_COUNT; ++position) {
      const int row = position / PLACEMENTS_PER_AXIS;
      const int column = position % PLACEMENTS_PER_AXIS;
      const Combo& combo =
          combinations[answer.choices[static_cast<std::size_t>(position)]];
      operations += combo.count;
      for (int di = 0; di < STAMP_SIZE; ++di) {
        for (int dj = 0; dj < STAMP_SIZE; ++dj) {
          const std::size_t board_index = static_cast<std::size_t>(
              (row + di) * BOARD_SIZE + column + dj);
          const std::size_t stamp_index =
              static_cast<std::size_t>(di * STAMP_SIZE + dj);
          board[board_index] =
              add_mod(board[board_index], combo.add[stamp_index]);
        }
      }
    }

    if (operations > OPERATION_LIMIT || operations != answer.operations) {
      throw std::runtime_error("illegal operation count");
    }
    if (board != answer.board || board_score(board) != answer.finalized_score) {
      throw std::runtime_error("incremental board or score is inconsistent");
    }
  }

  ModStampInstance instance;
  std::vector<Combo> combinations;
  std::array<std::vector<Action>, 4> allowed_actions;

 private:
  static std::uint32_t add_mod(std::uint32_t a, std::uint32_t b) {
    std::uint32_t result = a + b;
    if (result >= MODULO) result -= MODULO;
    return result;
  }

  void add_combination(int first, int second, int third, int count) {
    Combo combo;
    combo.count = static_cast<std::uint8_t>(count);
    const std::array<int, 3> ids{first, second, third};
    for (int i = 0; i < count; ++i) {
      combo.stamp_ids[static_cast<std::size_t>(i)] =
          static_cast<std::uint8_t>(ids[static_cast<std::size_t>(i)]);
      for (std::size_t cell = 0; cell < combo.add.size(); ++cell) {
        combo.add[cell] = add_mod(
            combo.add[cell],
            instance.stamps[static_cast<std::size_t>(
                ids[static_cast<std::size_t>(i)])][cell]);
      }
    }
    combinations.push_back(combo);
  }

  void build_combinations() {
    // 同じ場所で押す0～3個のスタンプを、順序を区別しない多重集合で列挙する。
    add_combination(0, 0, 0, 0);
    for (int first = 0; first < STAMP_COUNT; ++first) {
      add_combination(first, 0, 0, 1);
    }
    for (int first = 0; first < STAMP_COUNT; ++first) {
      for (int second = first; second < STAMP_COUNT; ++second) {
        add_combination(first, second, 0, 2);
      }
    }
    for (int first = 0; first < STAMP_COUNT; ++first) {
      for (int second = first; second < STAMP_COUNT; ++second) {
        for (int third = second; third < STAMP_COUNT; ++third) {
          add_combination(first, second, third, 3);
        }
      }
    }

    if (combinations.size() >
        static_cast<std::size_t>(std::numeric_limits<Action>::max())) {
      throw std::runtime_error("too many combinations for Action");
    }
    for (std::size_t id = 0; id < combinations.size(); ++id) {
      for (int limit = combinations[id].count; limit <= 3; ++limit) {
        allowed_actions[static_cast<std::size_t>(limit)].push_back(
            static_cast<Action>(id));
      }
    }
  }
};

struct BenchmarkResult {
  long long score = 0;
  int operations = 0;
  double milliseconds = 0.0;
};

BenchmarkResult run_beam(ModStampProblem& problem, int width) {
  ActionBeamRunner<ModStampProblem> beam(
      problem, problem.initial_state(), problem.initial_score(), width);
  const auto start = std::chrono::steady_clock::now();
  const int advanced = beam.run(PLACEMENT_COUNT);
  const auto finish = std::chrono::steady_clock::now();
  if (advanced != PLACEMENT_COUNT) {
    throw std::runtime_error("beam stopped before completing the board");
  }
  problem.validate(beam.best());
  return {
      beam.best().finalized_score,
      beam.best().operations,
      std::chrono::duration<double, std::milli>(finish - start).count()};
}

int main() {
  constexpr std::array<std::uint64_t, 5> seeds{0, 1, 2, 3, 4};
  constexpr std::array<int, 3> widths{1, 20, 100};
  std::array<long long, widths.size()> total_scores{};
  std::array<double, widths.size()> total_milliseconds{};
  long long initial_total = 0;

  std::cout << "AHC032-like Mod Stamp score benchmark\n";
  std::cout << "seed initial_score width score gain operations milliseconds\n";
  for (std::uint64_t seed : seeds) {
    ModStampProblem problem(make_instance(seed));
    const long long initial = board_score(problem.instance.board);
    initial_total += initial;
    for (std::size_t i = 0; i < widths.size(); ++i) {
      const BenchmarkResult result = run_beam(problem, widths[i]);
      total_scores[i] += result.score;
      total_milliseconds[i] += result.milliseconds;
      std::cout << seed << ' ' << initial << ' ' << widths[i] << ' '
                << result.score << ' ' << result.score - initial << ' '
                << result.operations << ' ' << std::fixed
                << std::setprecision(3) << result.milliseconds << '\n';
    }
  }

  const long double maximum_total =
      static_cast<long double>(seeds.size()) * BOARD_SIZE * BOARD_SIZE *
      (MODULO - 1U);
  std::cout << "summary_width score_sum gain_vs_initial percent_of_cell_max "
               "milliseconds\n";
  for (std::size_t i = 0; i < widths.size(); ++i) {
    const long double percentage =
        100.0L * total_scores[i] / maximum_total;
    std::cout << widths[i] << ' ' << total_scores[i] << ' '
              << total_scores[i] - initial_total << ' ' << std::fixed
              << std::setprecision(6) << static_cast<double>(percentage) << ' '
              << std::setprecision(3) << total_milliseconds[i] << '\n';
  }
}
