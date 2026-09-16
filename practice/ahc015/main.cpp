#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// BEGIN LIBRARY: common-scenario-average.hpp
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/common-scenario-average.hpp

// 全ての候補を「同じ未来シナリオ集合」で評価し、候補ごとの平均値を返す。
// 候補ごとに別乱数を使うより、候補差と偶然差を区別しやすい。
//
// 使い方:
// auto average = common_scenario_average(
//     actions, scenarios,
//     [](const Action& action, const Scenario& future) {
//       return simulate(action, future);
//     });
template <class Action, class Scenario, class Evaluate>
std::vector<long double> common_scenario_average(
    const std::vector<Action>& actions,
    const std::vector<Scenario>& scenarios,
    Evaluate evaluate
) {
  assert(!actions.empty());
  assert(!scenarios.empty());

  std::vector<long double> average(actions.size(), 0.0L);
  for (int action = 0; action < static_cast<int>(actions.size()); ++action) {
    for (const Scenario& scenario : scenarios) {
      average[action] += static_cast<long double>(
          evaluate(actions[action], scenario));
    }
    average[action] /= static_cast<long double>(scenarios.size());
  }
  return average;
}

// 問題依存部分をProblemへ集める、共通シナリオrolloutの薄いRunner。
//
// 【使う人がmain.cpp側へ書く場所】
// 次のTODOだけを自分の問題に合わせる。Runner本体は通常変更しない。
// 空関数を配置済みの雛形: template/search/monte-carlo-rollout.cpp
//
//   TODO: 【問題ごと】現在情報、今の1手、未知の未来、評価値の型を書く。
//   using State, Action, Scenario, Score
//   TODO: 【問題ごと】今比較したい合法Actionを全て返す。
//   generate_actions(const State&) -> 比較する最初のAction一覧。
//   TODO: 【問題ごと】自分では決められない未知情報だけをsampleする。
//   generate_scenario(const State&, mt19937_64&)
//     -> 未知の未来を1本生成する。
//   TODO: 【問題ごと】最初のAction後を終端まで進め、評価値を返す。
//   evaluate_action(const State&, const Action&, const Scenario&)
//     -> その最初のActionからシナリオを辿った評価値。
//
// Runnerは全Actionを同じScenario集合で比較し、平均が最良の1手を返す。
// 省略可能: 第2テンプレート引数は乱数器、第3引数は平均計算の型。
// 既存解の乱数列・丸めを保つ例: Runner<Problem, MyRandom, double>。
// MyRandomはuint64_tのseedから構築でき、generate_scenarioで使えればよい。
// 通常は既定のmt19937_64 / long doubleのままでよい。
// choose_action(state, samples, better)なら同点処理も指定できる。
// better(action, average, best_action, best_average) -> bool:
//   TODO: 前2引数の候補を後2引数の暫定最良より優先する時だけtrueを返す。
//   呼び出し順はgenerate_actions順。既定では完全同点なら先の候補を保つ。
// Stateの更新や出力は行わない。選んだActionの反映は呼び出し側が行う。
// ↓↓↓ ここから下はライブラリ本体。通常は編集しない。↓↓↓
template <class Problem, class Engine = std::mt19937_64,
          class Average = long double>
struct CommonScenarioRolloutRunner {
  using State = typename Problem::State;
  using Action = typename Problem::Action;
  using Scenario = typename Problem::Scenario;
  using Score = typename Problem::Score;

  explicit CommonScenarioRolloutRunner(Problem& problem,
                                       std::uint64_t seed = 0,
                                       bool maximize = true)
      : problem_(problem), engine_(seed), maximize_(maximize) {}

  Action choose_action(const State& state, int sample_count) {
    return choose_action(state, sample_count,
        [this](const Action&, Average score, const Action&, Average best) {
          return maximize_ ? best < score : score < best;
        });
  }

  template <class Better>
  Action choose_action(const State& state, int sample_count, Better better) {
    prepare(state, sample_count);

    for (std::size_t action = 0; action < actions_.size(); ++action) {
      for (const Scenario& scenario : scenarios_) {
        average_scores_[action] += static_cast<Average>(
            problem_.evaluate_action(state, actions_[action], scenario));
      }
      average_scores_[action] /= static_cast<Average>(sample_count);
    }
    return select(better);
  }

  // 【任意】同じScenarioの全Actionを一括評価する。共通の途中計算を共有したい時だけ使う。
  // evaluate_batch(const State&, const vector<Action>&, const Scenario&)
  //   -> Action列と同じ長さ・順序のscore列（vectorやarray、借用参照も可）。
  // TODO: callbackへ全候補の評価を書く。CoalescedRolloutで同じ状態以降を共有できる。
  // 各Actionの加算順・乱数列・同点処理は通常版と同じ。評価関数の呼び出し順は異なるので、
  // 外部乱数や評価の副作用に依存しないこと。Problemの必須関数は増やさない。
  template <class EvaluateBatch>
  Action choose_action_batched(const State& state, int sample_count,
                               EvaluateBatch&& evaluate_batch) {
    return choose_action_batched(state, sample_count, evaluate_batch,
        [this](const Action&, Average score, const Action&, Average best) {
          return maximize_ ? best < score : score < best;
        });
  }

  template <class EvaluateBatch, class Better>
  Action choose_action_batched(const State& state, int sample_count,
                               EvaluateBatch&& evaluate_batch, Better better) {
    prepare(state, sample_count);
    for (const Scenario& scenario : scenarios_) {
      auto&& scores = evaluate_batch(state, actions_, scenario);
      if (scores.size() != actions_.size()) {
        throw std::invalid_argument("batch scores must match actions");
      }
      for (std::size_t action = 0; action < actions_.size(); ++action) {
        average_scores_[action] += static_cast<Average>(scores[action]);
      }
    }
    for (Average& score : average_scores_) score /= static_cast<Average>(sample_count);
    return select(better);
  }

 private:
  void prepare(const State& state, int sample_count) {
    if (sample_count <= 0) {
      throw std::invalid_argument("sample_count must be positive");
    }

    actions_.clear();
    auto&& generated_actions = problem_.generate_actions(state);
    for (const auto& action : generated_actions) actions_.push_back(action);
    if (actions_.empty()) {
      throw std::runtime_error("generate_actions returned no action");
    }

    scenarios_.clear();
    scenarios_.reserve(static_cast<std::size_t>(sample_count));
    for (int sample = 0; sample < sample_count; ++sample) {
      scenarios_.push_back(problem_.generate_scenario(state, engine_));
    }

    average_scores_.assign(actions_.size(), Average{});
  }

  template <class Better>
  Action select(Better& better) {
    std::size_t best = 0;
    for (std::size_t action = 1; action < actions_.size(); ++action) {
      if (better(actions_[action], average_scores_[action],
                 actions_[best], average_scores_[best])) best = action;
    }
    return actions_[best];
  }

 public:
  void reserve(int action_count, int sample_count) {
    if (action_count < 0 || sample_count < 0) {
      throw std::invalid_argument("reserve counts must be non-negative");
    }
    actions_.reserve(static_cast<std::size_t>(action_count));
    scenarios_.reserve(static_cast<std::size_t>(sample_count));
    average_scores_.reserve(static_cast<std::size_t>(action_count));
  }

  const std::vector<Action>& last_actions() const { return actions_; }
  const std::vector<Scenario>& last_scenarios() const { return scenarios_; }
  const std::vector<Average>& last_average_scores() const {
    return average_scores_;
  }
  Engine& engine() { return engine_; }

 private:
  Problem& problem_;
  Engine engine_;
  bool maximize_;
  std::vector<Action> actions_;
  std::vector<Scenario> scenarios_;
  std::vector<Average> average_scores_;
};
// END LIBRARY: common-scenario-average.hpp
// BEGIN LIBRARY: coalesced-rollout.hpp
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <vector>
// Pre-contest public source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/library/coalesced-rollout.hpp

// 同じ未来を辿る少数候補を同時に進め、同じ途中状態になったら残りを共有する。
// hashによる近似ではない。等価なStateなら以後の遷移と最終評価も必ず同じ、が条件。
// 時刻は全候補で同じstep引数を使う。将来に効く履歴・乱数状態はState/固定Scenarioへ
// 全て入れる。最初のActionが違うだけで未来の方策も変わるなら、その情報もStateへ入れる。
// callbackは外部状態を変更しないこと（呼び出し回数は省略される）。
//
// TODO: Stateはsimulationの全状態、Scoreは最終評価の型を書く。
// TODO: initialに、比較したい各Actionを1回反映したStateを列挙順で入れる。
// TODO: advance(State&, int step)へ、共通Scenarioのstep番を使う1遷移を書く。
// TODO: evaluate(const State&)は最後の評価値を返す。
// TODO: State::operator==は「同じ未来を持つ」時だけtrue。等価判定を第5引数にも渡せる。
//
// CoalescedRollout<MyState, long long> cache;
// const auto& scores = cache.evaluate(initial, steps, advance, evaluate);
// scores[i]はinitial[i]を最後まで実行した正確な評価。次のevaluateまで有効。
// evaluate<false>(...)は共有OFFの比較用。同じ入力なら全scoreが一致する。
// Stateはコピー構築・代入可能、Scoreはコピー構築可能にする（default構築は不要）。
// 等価判定は1段O(候補数^2)。数個の候補向けで、大きなビームには使わない。
template <class State, class Score>
class CoalescedRollout {
 public:
  void reserve(std::size_t count) {
    states_.reserve(count);
    parent_.reserve(count);
    active_.reserve(count);
    scores_.reserve(count);
  }

  template <bool Merge = true, class Advance, class Evaluate,
            class Equal = std::equal_to<State>>
  const std::vector<Score>& evaluate(const std::vector<State>& initial,
                                    int steps, Advance&& advance,
                                    Evaluate&& evaluate_final, Equal equal = {}) {
    if (steps < 0) throw std::invalid_argument("steps must be non-negative");
    states_ = initial;
    parent_.resize(initial.size());
    active_.resize(initial.size());
    std::iota(parent_.begin(), parent_.end(), std::size_t{0});
    std::iota(active_.begin(), active_.end(), std::size_t{0});
    last_transitions_ = 0;
    if constexpr (Merge) merge_equal(equal);
    for (int step = 0; step < steps; ++step) {
      for (std::size_t index : active_) advance(states_[index], step);
      last_transitions_ += active_.size();
      if constexpr (Merge) merge_equal(equal);
    }
    scores_.clear();
    for (std::size_t index = 0; index < initial.size(); ++index) {
      std::size_t root = index;
      while (parent_[root] != root) root = parent_[root];
      // 代表は常に先の候補なので、代表の最終評価は既に計算済み。
      if (root == index) scores_.push_back(evaluate_final(states_[index]));
      else scores_.push_back(scores_[root]);
    }
    return scores_;
  }

  std::uint64_t last_transitions() const { return last_transitions_; }

 private:
  template <class Equal>
  void merge_equal(Equal& equal) {
    std::size_t kept = 0;
    for (std::size_t index : active_) {
      std::size_t earlier = 0;
      while (earlier < kept && !equal(states_[active_[earlier]], states_[index])) ++earlier;
      if (earlier == kept) active_[kept++] = index;
      else parent_[index] = active_[earlier];
    }
    active_.resize(kept);
  }

  std::vector<State> states_;
  std::vector<std::size_t> parent_, active_;
  std::vector<Score> scores_;
  std::uint64_t last_transitions_ = 0;
};
// END LIBRARY: coalesced-rollout.hpp

// AHC015 "Halloween Candy" の公式入力分布と公式得点を使う。
// https://atcoder.jp/contests/ahc015/tasks/ahc015_a
//
// 【問題に合わせて書き換える場所】
//   CandyRolloutProblem の State / Action / Scenario と3関数。
//   - generate_actions: 今比較する最初の手
//   - generate_scenario: 自分で決められない未来
//   - evaluate_action: その未来で最初の手を採点するplayout
//
// 【ライブラリが担当する場所】
//   同じ未来sampleを全Actionで使う、平均する、最良の1手を選ぶ。
//
// 未来の配置だけをMonte Carloでsampleし、自分の未来操作は
// 「今の色×次の色」の軽い問題固有方策で最後まで進める。

constexpr int BOARD_SIZE = 10;
constexpr int CELL_COUNT = BOARD_SIZE * BOARD_SIZE;
using Board = std::array<std::uint8_t, CELL_COUNT>;

int cell_id(int row, int column) { return row * BOARD_SIZE + column; }

void place_by_rank(Board& board, int rank, int flavor) {
  for (std::uint8_t& cell : board) {
    if (cell != 0) continue;
    if (--rank == 0) {
      cell = static_cast<std::uint8_t>(flavor);
      return;
    }
  }
  throw std::runtime_error("placement rank exceeds empty cells");
}

Board tilt_board(const Board& board, int direction) {
  Board result{};
  if (direction <= 1) {
    for (int column = 0; column < BOARD_SIZE; ++column) {
      int write = direction == 0 ? 0 : BOARD_SIZE - 1;
      const int step = direction == 0 ? 1 : -1;
      for (int k = 0; k < BOARD_SIZE; ++k) {
        const int row = direction == 0 ? k : BOARD_SIZE - 1 - k;
        const std::uint8_t candy = board[cell_id(row, column)];
        if (candy == 0) continue;
        result[cell_id(write, column)] = candy;
        write += step;
      }
    }
  } else {
    for (int row = 0; row < BOARD_SIZE; ++row) {
      int write = direction == 2 ? 0 : BOARD_SIZE - 1;
      const int step = direction == 2 ? 1 : -1;
      for (int k = 0; k < BOARD_SIZE; ++k) {
        const int column = direction == 2 ? k : BOARD_SIZE - 1 - k;
        const std::uint8_t candy = board[cell_id(row, column)];
        if (candy == 0) continue;
        result[cell_id(row, write)] = candy;
        write += step;
      }
    }
  }
  return result;
}

int component_square_sum(const Board& board) {
  std::array<std::uint8_t, CELL_COUNT> visited{};
  std::array<int, CELL_COUNT> stack{};
  constexpr std::array<int, 4> DR{{-1, 1, 0, 0}};
  constexpr std::array<int, 4> DC{{0, 0, -1, 1}};
  int answer = 0;
  for (int start = 0; start < CELL_COUNT; ++start) {
    if (board[start] == 0 || visited[start]) continue;
    const std::uint8_t flavor = board[start];
    int size = 0;
    int stack_size = 1;
    stack[0] = start;
    visited[start] = 1;
    while (stack_size > 0) {
      const int cell = stack[--stack_size];
      ++size;
      const int row = cell / BOARD_SIZE;
      const int column = cell % BOARD_SIZE;
      for (int direction = 0; direction < 4; ++direction) {
        const int next_row = row + DR[direction];
        const int next_column = column + DC[direction];
        if (next_row < 0 || next_row >= BOARD_SIZE || next_column < 0 ||
            next_column >= BOARD_SIZE) {
          continue;
        }
        const int next = cell_id(next_row, next_column);
        if (!visited[next] && board[next] == flavor) {
          visited[next] = 1;
          stack[stack_size++] = next;
        }
      }
    }
    answer += size * size;
  }
  return answer;
}

struct CandyCase {
  std::array<int, CELL_COUNT> flavor{};
  std::array<int, CELL_COUNT> placement_rank{};
};

CandyCase make_case(std::uint64_t seed) {
  std::mt19937_64 engine(seed);
  CandyCase result;
  for (int turn = 0; turn < CELL_COUNT; ++turn) {
    result.flavor[turn] =
        std::uniform_int_distribution<int>(1, 3)(engine);
    result.placement_rank[turn] =
        std::uniform_int_distribution<int>(1, CELL_COUNT - turn)(engine);
  }
  return result;
}

long long official_score(const Board& board, const CandyCase& input) {
  std::array<int, 4> count{};
  for (int flavor : input.flavor) ++count[flavor];
  long long denominator = 0;
  for (int flavor = 1; flavor <= 3; ++flavor) {
    denominator += 1LL * count[flavor] * count[flavor];
  }
  const long long numerator = component_square_sum(board);
  return std::llround(1'000'000.0L * numerator / denominator);
}

// direction: 0=F, 1=B, 2=L, 3=R。
int rule_direction(int current_flavor, int next_flavor) {
  // 色1を上下の一方、色2・3を左右へ分ける。
  // 「現在の色×次に来る色」なので、別問題ではここが主な編集点。
  constexpr int RULE[3][3] = {
      {0, 1, 1},
      {0, 2, 3},
      {0, 2, 3},
  };
  return RULE[current_flavor - 1][next_flavor - 1];
}

struct CandyRolloutProblem {
  // TODO: 【問題ごと】現在までに確定している情報をStateへ書く。
  struct State {
    Board board{};
    int turn = 0;  // このturnの飴は配置済み、傾ける前。
  };
  // TODO: 【問題ごと】今選ぶ1手の型を書く。全候補ぶん持つので小さくする。
  using Action = int;
  // TODO: 【問題ごと】自分で決められない未知の未来1本を表す型を書く。
  struct Scenario {
    std::array<std::uint8_t, CELL_COUNT> rank{};
    int length = 0;
  };
  // TODO: 【問題ごと】1 rolloutの評価値の型を書く。
  using Score = int;

  // TODO: 【問題ごと】共通入力・事前計算と、今選べるAction表を置く。
  const CandyCase& input;
  std::array<Action, 4> actions{{0, 1, 2, 3}};

  // TODO: 【問題ごと】今比較する合法Actionを全て返す。
  const std::array<Action, 4>& generate_actions(const State&) const {
    return actions;
  }

  // TODO: 【問題ごと】自分では決められない未知情報だけをsampleする。
  // 未知なのは「次以降の飴が何番目の空きマスへ来るか」だけ。
  Scenario generate_scenario(const State& state,
                             std::mt19937_64& engine) const {
    Scenario scenario;
    scenario.length = CELL_COUNT - 1 - state.turn;
    for (int step = 0; step < scenario.length; ++step) {
      const int empty_count = CELL_COUNT - 1 - state.turn - step;
      scenario.rank[step] = static_cast<std::uint8_t>(
          std::uniform_int_distribution<int>(1, empty_count)(engine));
    }
    return scenario;
  }

  // TODO: 【問題ごと】最初のAction後を終端まで進め、最終評価を返す。
  // 最初の1手だけactionを使い、以後は軽い固定方策で終局まで進める。
  // 返値は公式得点の分子。全actionで分母が同じなので順位は一致する。
  Score evaluate_action(const State& state,
                        const Action& action,
                        const Scenario& scenario) const {
    Board board = tilt_board(state.board, action);
    for (int step = 0; step < scenario.length; ++step) {
      const int turn = state.turn + 1 + step;
      place_by_rank(board, scenario.rank[step], input.flavor[turn]);
      if (turn + 1 < CELL_COUNT) {
        board = tilt_board(
            board, rule_direction(input.flavor[turn], input.flavor[turn + 1]));
      }
    }
    return component_square_sum(board);
  }
};


#ifndef AHC015_SAMPLES
#define AHC015_SAMPLES 448 // TODO: 制限時間内の実問題scoreで調整する。
#endif
#ifndef AHC015_MERGE
#define AHC015_MERGE 1
#endif
#ifndef AHC015_CLASSIC
#define AHC015_CLASSIC 0
#endif
#ifndef AHC015_ENGINE_SEED
#define AHC015_ENGINE_SEED 15015
#endif
#ifndef AHC015_TURN_SEED
#define AHC015_TURN_SEED 1
#endif
// TODO(AHC015): 既知の味だけ最初に読む。配置順位は各ターンの直前に1個だけ読む。
int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  CandyCase input;
  for (int& flavor : input.flavor) {
    if (!(std::cin >> flavor) || flavor < 1 || flavor > 3) return 1;
  }
  CandyRolloutProblem problem{input};
  CommonScenarioRolloutRunner<CandyRolloutProblem> runner(problem, AHC015_ENGINE_SEED);
  runner.reserve(4, AHC015_SAMPLES);
  CoalescedRollout<Board, int> shared;
  shared.reserve(4);
  std::vector<Board> initial_boards;
  initial_boards.reserve(4);
  std::uint64_t transitions = 0;
  Board board{};
  constexpr char directions[] = "FBLR";
  for (int turn = 0; turn < CELL_COUNT; ++turn) {
    int rank;
    if (!(std::cin >> rank) || rank < 1 || rank > CELL_COUNT - turn) return 1;
    place_by_rank(board, rank, input.flavor[turn]);
    int action = 0;
    if (turn + 1 < CELL_COUNT) {
      const CandyRolloutProblem::State state{board, turn};
#if AHC015_TURN_SEED
      // サンプル数を変えても各手番の未来列の先頭を揃える。未来の実入力は使わない。
      runner.engine().seed(AHC015_ENGINE_SEED + 0x9e3779b97f4a7c15ULL * (turn + 1));
#endif
#if AHC015_CLASSIC
      action = runner.choose_action(state, AHC015_SAMPLES);
      transitions += std::uint64_t{4} * AHC015_SAMPLES * (CELL_COUNT - 1 - turn);
#else
      // TODO: 今の各Actionを1回だけ反映した仮状態。Scenarioに依存しない部分は先に作る。
      initial_boards.clear();
      for (int first_action : problem.generate_actions(state)) {
        initial_boards.push_back(tilt_board(board, first_action));
      }
      action = runner.choose_action_batched(state, AHC015_SAMPLES,
          [&](const auto&, const auto&, const CandyRolloutProblem::Scenario& scenario) -> const std::vector<int>& {
        // TODO: 共通の未来を1段進める。元の方策・配置・最終評価は通常版と同じ。
        const auto advance = [&](Board& future, int step) {
          const int future_turn = turn + 1 + step;
          place_by_rank(future, scenario.rank[step], input.flavor[future_turn]);
          if (future_turn + 1 < CELL_COUNT) {
            future = tilt_board(future, rule_direction(input.flavor[future_turn], input.flavor[future_turn + 1]));
          }
        };
        const auto& scores = shared.evaluate<AHC015_MERGE != 0>(
            initial_boards, scenario.length, advance, component_square_sum);
        transitions += shared.last_transitions();
        return scores;
      });
#endif
    }
    board = tilt_board(board, action);
    std::cout << directions[action] << std::endl;
  }
  std::cerr << "samples=" << AHC015_SAMPLES << " transitions=" << transitions << '\n';
  return 0;
}
