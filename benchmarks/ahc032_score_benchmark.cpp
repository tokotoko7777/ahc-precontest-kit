#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
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
// 0.06 * MOD * (1-progress) * remaining_operations を整数だけで比較する。
// rank = finalized_score * 4900
//      + 6 * MOD * remaining_placements * remaining_operations
constexpr long long RANK_SCALE = 4900;

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
  struct Placement {
    std::uint8_t row = 0;
    std::uint8_t column = 0;
    std::uint8_t max_actions = 0;
    std::uint8_t cumulative_limit = 0;
  };

  // TODO: 【問題ごと】探索途中の解と差分評価用cacheをStateへ書く。
  struct State {
    std::array<std::uint32_t, BOARD_SIZE * BOARD_SIZE> board{};
    // choices[position]は、その置き場所で選んだComboの番号。
    std::array<std::uint16_t, PLACEMENT_COUNT> choices{};
    int position = 0;
    int operations = 0;
    long long finalized_score = 0;
  };

  // TODO: 【問題ごと】次の1手を表す軽いActionと、候補順位Scoreを選ぶ。
  // Actionには3x3加算値を持たせず、Problem側のComboを引く番号だけを置く。
  // 全候補ぶん保存されるデータを2 byteに抑える。
  using Action = std::uint16_t;
  using Score = long long;

  struct Combo {
    std::array<std::uint32_t, STAMP_SIZE * STAMP_SIZE> add{};
    std::array<std::uint8_t, 4> stamp_ids{};
    std::uint8_t count = 0;
  };

  explicit ModStampProblem(ModStampInstance input)
      : instance(std::move(input)) {
    build_placements();
    build_combinations();
  }

  // TODO: 【問題ごと】初期Stateと、その候補順位値を作る。
  State initial_state() const {
    State state;
    state.board = instance.board;
    return state;
  }

  Score initial_score() const {
    return 6LL * MODULO * PLACEMENT_COUNT * OPERATION_LIMIT;
  }

  // TODO: 【問題ごと】現在Stateから試す合法Actionを全て返す。
  // 青マスは最大2回、端の緑マスは最大3回、最後の赤マスは最大4回。
  // さらに累積上限を設け、81回を序盤だけで使い切らないよう終盤へ手数を残す。
  // allowed_actionsはProblemが保持するので、毎Stateでvectorを作らず参照を返せる。
  const std::vector<Action>& generate_actions(const State& state) const {
    const Placement& placement =
        placements[static_cast<std::size_t>(state.position)];
    const int until_cumulative_limit =
        static_cast<int>(placement.cumulative_limit) - state.operations;
    const int maximum = std::max(
        0, std::min(static_cast<int>(placement.max_actions),
                    until_cumulative_limit));
    return allowed_actions[static_cast<std::size_t>(maximum)];
  }

  // TODO: 【問題ごと】Action適用後の順位値そのものを差分計算する。
  // 次の置き場所を処理すると、そこより右下のスタンプでは二度と変更できない
  // マスが生じる。その「新しく確定するマス」の合計を順位値へ加える。
  // 最終世代では全81マスが確定するため、この順位値が問題本来の得点になる。
  Score evaluate_action(const State& state, const Action& action) const {
    const Placement& placement =
        placements[static_cast<std::size_t>(state.position)];
    const int row = placement.row;
    const int column = placement.column;
    const int finalized_rows =
        row == PLACEMENTS_PER_AXIS - 1 ? STAMP_SIZE : 1;
    const int finalized_columns =
        column == PLACEMENTS_PER_AXIS - 1 ? STAMP_SIZE : 1;
    const Combo& combo = combinations[action];

    Score finalized_score = state.finalized_score;
    for (int di = 0; di < finalized_rows; ++di) {
      for (int dj = 0; dj < finalized_columns; ++dj) {
        const std::size_t board_index = static_cast<std::size_t>(
            (row + di) * BOARD_SIZE + column + dj);
        const std::size_t stamp_index =
            static_cast<std::size_t>(di * STAMP_SIZE + dj);
        finalized_score +=
            add_mod(state.board[board_index], combo.add[stamp_index]);
      }
    }
    const int next_position = state.position + 1;
    const int next_operations = state.operations + combo.count;
    const int remaining_placements = PLACEMENT_COUNT - next_position;
    const int remaining_operations = OPERATION_LIMIT - next_operations;
    return finalized_score * RANK_SCALE +
           6LL * MODULO * remaining_placements * remaining_operations;
  }

  // TODO: 【問題ごと】採用されたActionだけをコピー済みStateへ反映する。
  // 選ばれたComboだけを、コピー済みの子Stateへ反映する。
  void apply_action(State& state, Action& action) const {
    const Placement& placement =
        placements[static_cast<std::size_t>(state.position)];
    const int row = placement.row;
    const int column = placement.column;
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
      const Placement& placement =
          placements[static_cast<std::size_t>(position)];
      const int row = placement.row;
      const int column = placement.column;
      const Combo& combo =
          combinations[answer.choices[static_cast<std::size_t>(position)]];
      operations += combo.count;
      if (combo.count > placement.max_actions ||
          operations > placement.cumulative_limit) {
        throw std::runtime_error("operation schedule limit exceeded");
      }
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
  std::array<Placement, PLACEMENT_COUNT> placements{};
  std::vector<Combo> combinations;
  std::array<std::vector<Action>, 5> allowed_actions;

 private:
  static std::uint32_t add_mod(std::uint32_t a, std::uint32_t b) {
    std::uint32_t result = a + b;
    if (result >= MODULO) result -= MODULO;
    return result;
  }

  void add_combination(const std::array<int, 4>& ids, int count) {
    Combo combo;
    combo.count = static_cast<std::uint8_t>(count);
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

  void enumerate_combinations(int count,
                              int depth,
                              int minimum_stamp,
                              std::array<int, 4>& ids) {
    if (depth == count) {
      add_combination(ids, count);
      return;
    }
    for (int stamp = minimum_stamp; stamp < STAMP_COUNT; ++stamp) {
      ids[static_cast<std::size_t>(depth)] = stamp;
      enumerate_combinations(count, depth + 1, stamp, ids);
    }
  }

  void build_combinations() {
    // 同じ場所で押す0～4個のスタンプを、順序を区別しない多重集合で列挙する。
    std::array<int, 4> ids{};
    for (int count = 0; count <= 4; ++count) {
      enumerate_combinations(count, 0, 0, ids);
    }

    if (combinations.size() >
        static_cast<std::size_t>(std::numeric_limits<Action>::max())) {
      throw std::runtime_error("too many combinations for Action");
    }
    for (std::size_t id = 0; id < combinations.size(); ++id) {
      for (int limit = combinations[id].count; limit <= 4; ++limit) {
        allowed_actions[static_cast<std::size_t>(limit)].push_back(
            static_cast<Action>(id));
      }
    }
  }

  void build_placements() {
    // 未処理領域の上辺→左辺を交互に確定する。
    // 行優先だけにすると難しい3マス確定が各行末へ偏るため、
    // 横向きと縦向きの3マス確定を交互に出して多様性低下を分散する。
    int position = 0;
    int cumulative_twice = 0;
    const auto add = [&](int row, int column) {
      const bool bottom = row == PLACEMENTS_PER_AXIS - 1;
      const bool right = column == PLACEMENTS_PER_AXIS - 1;
      int maximum = 2;
      int budget_increase_twice = 3;  // 通常マスは累積上限を+1.5。
      if (bottom && right) {
        maximum = 4;
        budget_increase_twice = 6;  // 最後の3x3は+3。
      } else if (bottom || right) {
        maximum = 3;
        budget_increase_twice = 4;  // 端の3マスは+2。
      }
      cumulative_twice += budget_increase_twice;
      placements[static_cast<std::size_t>(position++)] = {
          static_cast<std::uint8_t>(row),
          static_cast<std::uint8_t>(column),
          static_cast<std::uint8_t>(maximum),
          static_cast<std::uint8_t>((cumulative_twice + 1) / 2)};
    };

    for (int layer = 0; layer < PLACEMENTS_PER_AXIS; ++layer) {
      for (int column = layer; column < PLACEMENTS_PER_AXIS; ++column) {
        add(layer, column);
      }
      for (int row = layer + 1; row < PLACEMENTS_PER_AXIS; ++row) {
        add(row, layer);
      }
    }
    if (position != PLACEMENT_COUNT ||
        placements.back().cumulative_limit != OPERATION_LIMIT) {
      throw std::runtime_error("invalid placement schedule");
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

int main(int argc, char** argv) {
  int case_count = 5;
  if (argc >= 2) case_count = std::stoi(argv[1]);
  if (case_count <= 0) {
    throw std::invalid_argument("case_count must be positive");
  }

  std::vector<int> widths{1, 100, 1000, 3000, 10000};
  if (argc >= 3) {
    widths.clear();
    for (int i = 2; i < argc; ++i) {
      const int width = std::stoi(argv[i]);
      if (width <= 0) {
        throw std::invalid_argument("beam width must be positive");
      }
      widths.push_back(width);
    }
  }

  std::vector<long long> total_scores(widths.size());
  std::vector<double> total_milliseconds(widths.size());
  long long initial_total = 0;

  std::cout << "AHC032-like Mod Stamp score benchmark\n";
  std::cout << "usage: ahc032_score_benchmark [case_count [width ...]]\n";
  std::cout << "seed initial_score width score gain operations milliseconds\n";
  for (int seed = 0; seed < case_count; ++seed) {
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
      static_cast<long double>(case_count) * BOARD_SIZE * BOARD_SIZE *
      (MODULO - 1U);
  std::cout << "summary_width cases score_sum scaled_to_150 "
               "gain_vs_initial percent_of_cell_max milliseconds\n";
  for (std::size_t i = 0; i < widths.size(); ++i) {
    const long double percentage =
        100.0L * total_scores[i] / maximum_total;
    const long double scaled_to_150 =
        static_cast<long double>(total_scores[i]) * 150.0L / case_count;
    std::cout << widths[i] << ' ' << case_count << ' ' << total_scores[i]
              << ' ' << std::fixed << std::setprecision(0) << scaled_to_150
              << ' ' << total_scores[i] - initial_total << ' '
              << std::setprecision(6) << static_cast<double>(percentage) << ' '
              << std::setprecision(3) << total_milliseconds[i] << '\n';
  }
  std::cout << "reference_official_inputs terry_u16_second 11845290951426\n";
  std::cout << "reference_official_inputs editorial_first_equivalent "
               "11920072299359\n";
  std::cout << "reference_note: generated cases differ; scaled_to_150 is only "
               "a distribution-level estimate\n";
}
