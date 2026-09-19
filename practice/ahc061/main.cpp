// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

// BEGIN LIBRARY: 通常は編集しない。更新元URLはincludes直後。
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
// 空関数を配置済みの雛形: template/search/monte-carlo/rollout.cpp
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
    for (std::size_t action = 0; action < actions_.size(); ++action) {
      for (const Scenario& scenario : scenarios_) {
        average_scores_[action] += static_cast<Average>(
            problem_.evaluate_action(state, actions_[action], scenario));
      }
      average_scores_[action] /= static_cast<Average>(sample_count);
    }

    std::size_t best = 0;
    for (std::size_t action = 1; action < actions_.size(); ++action) {
      if (better(actions_[action], average_scores_[action],
                 actions_[best], average_scores_[best])) best = action;
    }
    return actions_[best];
  }

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
// END LIBRARY: ここから下が問題依存部分。
// Pre-contest public solver source (created with generative AI):
// https://github.com/tokotoko7777/ahc-precontest-kit/blob/main/examples/search/ahc061_common_rollout.cpp
// Official problem: https://atcoder.jp/contests/ahc061/tasks/ahc061_a

using namespace std;

// AHC061: Multi-Player Territory Game
//
// The program has three small building blocks.
//   1. Make every legal move by BFS.
//   2. Learn each opponent's hidden parameters from the moves we observe.
//   3. Try likely opponent moves, simulate one turn exactly, and choose the
//      move with the best average resulting board.
//
// Compile with -DSIMPLE_BASELINE to use only a legal immediate-gain greedy.

constexpr int MAX_N = 10;
constexpr int MAX_CELLS = MAX_N * MAX_N;
constexpr int MAX_PLAYERS = 8;
constexpr int PARTICLE_COUNT = 512;

#ifndef AHC061_SCENARIOS
#define AHC061_SCENARIOS 40
#endif

#ifndef AHC061_CANDIDATES
#define AHC061_CANDIDATES 18
#endif

constexpr int SCENARIO_COUNT = AHC061_SCENARIOS;
constexpr int FINAL_CANDIDATES = AHC061_CANDIDATES;

#ifndef AHC061_LOOKAHEAD
#define AHC061_LOOKAHEAD 3
#endif

constexpr int LOOKAHEAD = AHC061_LOOKAHEAD;
static_assert(LOOKAHEAD >= 1 && SCENARIO_COUNT >= 1 && FINAL_CANDIDATES >= 1,
              "rollout counts must be positive");

const int DR[4] = {-1, 1, 0, 0};
const int DC[4] = {0, 0, -1, 1};

struct Random {
    uint64_t state;

    explicit Random(uint64_t seed) : state(seed) {}

    uint64_t next_u64() {
        state += 0x9e3779b97f4a7c15ULL;
        uint64_t z = state;
        z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31U);
    }

    double uniform() {
        return static_cast<double>(next_u64() >> 11U) *
               (1.0 / 9007199254740992.0);
    }
};

struct GameState {
    int n = 0;
    int players = 0;
    int level_limit = 0;
    array<int, MAX_CELLS> owner{};
    array<int, MAX_CELLS> level{};
    array<int, MAX_PLAYERS> position{};
};

struct Particle {
    // weight[0..3] corresponds to empty, own-upgrade, enemy-level-1,
    // and enemy-level-2-or-more.
    array<double, 4> weight{};
    double epsilon = 0.3;
    double log_probability = 0.0;
};

struct Scenario {
    array<int, MAX_PLAYERS> particle_index{};
    array<array<double, MAX_PLAYERS>, LOOKAHEAD> random_kind{};
    array<array<double, MAX_PLAYERS>, LOOKAHEAD> random_cell{};
};

class Solver {
public:
    void run() {
        read_initial_input();

#ifndef SIMPLE_BASELINE
        initialize_models();
        runner.reserve(FINAL_CANDIDATES, SCENARIO_COUNT);
#endif

        for (int turn = 0; turn < total_turns; ++turn) {
            vector<vector<int>> legal(players);
            for (int p = 0; p < players; ++p) {
                legal[p] = legal_moves(state, p);
            }

            int my_move = state.position[0];

#ifdef SIMPLE_BASELINE
            my_move = choose_baseline(legal[0]);
#else
            vector<array<double, MAX_CELLS>> opponent_probability(players);
            for (int p = 1; p < players; ++p) {
                opponent_probability[p] = predict_moves(p, legal[p]);
            }
            my_move = choose_move(turn, legal, opponent_probability);
#endif

            cout << my_move / n << ' ' << my_move % n << '\n' << flush;

            vector<int> selected(players);
            for (int p = 0; p < players; ++p) {
                int r = 0;
                int c = 0;
                if (!(cin >> r >> c)) {
                    return;
                }
                selected[p] = r * n + c;
            }

#ifndef SIMPLE_BASELINE
            // The selected moves were decided from the board at the beginning
            // of this turn, so update the beliefs before replacing that board.
            for (int p = 1; p < players; ++p) {
                observe_move(p, legal[p], selected[p]);
            }
#endif

            for (int p = 0; p < players; ++p) {
                int r = 0;
                int c = 0;
                cin >> r >> c;
                state.position[p] = r * n + c;
            }
            for (int cell = 0; cell < n * n; ++cell) {
                cin >> state.owner[cell];
            }
            for (int cell = 0; cell < n * n; ++cell) {
                cin >> state.level[cell];
            }
        }
    }

private:
    int n = 0;
    int players = 0;
    int total_turns = 0;
    int level_limit = 0;
    array<int, MAX_CELLS> value{};
    GameState state;
    vector<vector<Particle>> model;

    bool inside(int r, int c) const {
        return 0 <= r && r < n && 0 <= c && c < n;
    }

    void read_initial_input() {
        cin >> n >> players >> total_turns >> level_limit;
        for (int cell = 0; cell < n * n; ++cell) {
            cin >> value[cell];
        }

        state.n = n;
        state.players = players;
        state.level_limit = level_limit;
        state.owner.fill(-1);
        state.level.fill(0);
        state.position.fill(0);

        for (int p = 0; p < players; ++p) {
            int r = 0;
            int c = 0;
            cin >> r >> c;
            const int cell = r * n + c;
            state.position[p] = cell;
            state.owner[cell] = p;
            state.level[cell] = 1;
        }
    }

    vector<int> legal_moves(const GameState& board, int player) const {
        array<char, MAX_CELLS> reached{};
        array<char, MAX_CELLS> candidate{};
        queue<int> bfs;

        const int start = board.position[player];
        reached[start] = 1;
        candidate[start] = 1;
        bfs.push(start);

        while (!bfs.empty()) {
            const int cell = bfs.front();
            bfs.pop();
            const int r = cell / n;
            const int c = cell % n;

            for (int d = 0; d < 4; ++d) {
                const int nr = r + DR[d];
                const int nc = c + DC[d];
                if (!inside(nr, nc)) {
                    continue;
                }
                const int next = nr * n + nc;
                candidate[next] = 1;
                if (!reached[next] && board.owner[next] == player) {
                    reached[next] = 1;
                    bfs.push(next);
                }
            }
        }

        // A player may not choose a cell currently occupied by somebody else.
        for (int other = 0; other < players; ++other) {
            if (other != player) {
                candidate[board.position[other]] = 0;
            }
        }

        vector<int> result;
        result.reserve(n * n);
        for (int cell = 0; cell < n * n; ++cell) {
            if (candidate[cell]) {
                result.push_back(cell);
            }
        }
        return result;
    }

    int action_type(const GameState& board, int player, int cell) const {
        if (board.owner[cell] == -1) {
            return 0;
        }
        if (board.owner[cell] == player) {
            return board.level[cell] < level_limit ? 1 : 4;
        }
        return board.level[cell] == 1 ? 2 : 3;
    }

    double action_value(const Particle& particle, const GameState& board,
                        int player, int cell) const {
        const int type = action_type(board, player, cell);
        if (type == 4) {
            return 0.0;
        }
        return static_cast<double>(value[cell]) * particle.weight[type];
    }

    pair<double, int> best_value_and_count(const Particle& particle,
                                           const GameState& board, int player,
                                           const vector<int>& legal) const {
        double best = -numeric_limits<double>::infinity();
        int count = 0;
        for (int cell : legal) {
            const double score = action_value(particle, board, player, cell);
            if (count == 0) {
                best = score;
                count = 1;
                continue;
            }
            const double tolerance = 1e-9 * max(1.0, abs(best));
            if (score > best + tolerance) {
                best = score;
                count = 1;
            } else if (score >= best - tolerance) {
                ++count;
            }
        }
        return {best, count};
    }

    void initialize_models() {
        model.assign(players, {});
        for (int p = 1; p < players; ++p) {
            model[p].reserve(PARTICLE_COUNT);
            Random generator(0xa5a5a5a500000000ULL +
                             static_cast<uint64_t>(p) * 0x123456789ULL);
            for (int i = 0; i < PARTICLE_COUNT; ++i) {
                Particle particle;
                for (double& w : particle.weight) {
                    w = 0.3 + 0.7 * generator.uniform();
                }
                particle.epsilon = 0.1 + 0.4 * generator.uniform();
                model[p].push_back(particle);
            }
        }
    }

    vector<double> normalized_particle_weights(int player) const {
        vector<double> probability(model[player].size());
        double largest = -numeric_limits<double>::infinity();
        for (const Particle& particle : model[player]) {
            largest = max(largest, particle.log_probability);
        }

        double sum = 0.0;
        for (size_t i = 0; i < model[player].size(); ++i) {
            probability[i] = exp(model[player][i].log_probability - largest);
            sum += probability[i];
        }
        for (double& probability_value : probability) {
            probability_value /= sum;
        }
        return probability;
    }

    array<double, MAX_CELLS> predict_moves(int player,
                                           const vector<int>& legal) const {
        array<double, MAX_CELLS> result{};
        const vector<double> posterior = normalized_particle_weights(player);
        double random_mass = 0.0;

        for (size_t i = 0; i < model[player].size(); ++i) {
            const Particle& particle = model[player][i];
            const double mass = posterior[i];
            const auto [best, best_count] =
                best_value_and_count(particle, state, player, legal);
            random_mass += mass * particle.epsilon;

            for (int cell : legal) {
                const double score = action_value(particle, state, player, cell);
                const double tolerance = 1e-9 * max(1.0, abs(best));
                if (score >= best - tolerance) {
                    result[cell] += mass * (1.0 - particle.epsilon) /
                                    static_cast<double>(best_count);
                }
            }
        }

        const double uniform_mass = random_mass / static_cast<double>(legal.size());
        for (int cell : legal) {
            result[cell] += uniform_mass;
        }
        return result;
    }

    void observe_move(int player, const vector<int>& legal, int selected) {
        double largest = -numeric_limits<double>::infinity();
        for (Particle& particle : model[player]) {
            const auto [best, best_count] =
                best_value_and_count(particle, state, player, legal);
            const double selected_value =
                action_value(particle, state, player, selected);
            const double tolerance = 1e-9 * max(1.0, abs(best));

            double likelihood = particle.epsilon /
                                static_cast<double>(legal.size());
            if (selected_value >= best - tolerance) {
                likelihood += (1.0 - particle.epsilon) /
                              static_cast<double>(best_count);
            }
            particle.log_probability += log(max(likelihood, 1e-300));
            largest = max(largest, particle.log_probability);
        }

        // Subtracting one common value changes no posterior probability and
        // prevents exp/log values from drifting to extreme magnitudes.
        for (Particle& particle : model[player]) {
            particle.log_probability -= largest;
        }
    }

    array<long long, MAX_PLAYERS> player_scores(const GameState& board) const {
        array<long long, MAX_PLAYERS> score{};
        for (int cell = 0; cell < n * n; ++cell) {
            const int p = board.owner[cell];
            if (p >= 0) {
                score[p] += static_cast<long long>(value[cell]) *
                            board.level[cell];
            }
        }
        return score;
    }

    int choose_baseline(const vector<int>& legal) const {
        int answer = legal.front();
        long long best = -1;
        for (int cell : legal) {
            long long gain = 0;
            if (state.owner[cell] == -1) {
                gain = value[cell];
            } else if (state.owner[cell] == 0 &&
                       state.level[cell] < level_limit) {
                gain = value[cell];
            } else if (state.owner[cell] > 0 && state.level[cell] == 1) {
                gain = value[cell];
            }
            if (gain > best || (gain == best && cell < answer)) {
                best = gain;
                answer = cell;
            }
        }
        return answer;
    }

    double quick_score(const GameState& board, int cell,
                       const vector<array<double, MAX_CELLS>>& probability,
                       const array<long long, MAX_PLAYERS>& scores,
                       int remaining) const {
        double survive = 1.0;
        if (board.owner[cell] != 0) {
            for (int p = 1; p < players; ++p) {
                survive *= 1.0 - probability[p][cell];
            }
        }

        double own_gain = 0.0;
        double enemy_loss = 0.0;
        if (board.owner[cell] == -1) {
            own_gain = value[cell];
        } else if (board.owner[cell] == 0) {
            if (board.level[cell] < level_limit) {
                own_gain = value[cell];
            }
        } else {
            enemy_loss = value[cell];
            if (board.level[cell] == 1) {
                own_gain = value[cell];
            }
        }

        long long strongest = 0;
        for (int p = 1; p < players; ++p) {
            strongest = max(strongest, scores[p]);
        }
        double denial_weight = 0.25;
        if (board.owner[cell] > 0 &&
            scores[board.owner[cell]] == strongest) {
            denial_weight = 0.8;
        }

        // Capturing a border cell can reveal another useful border. This
        // small bonus prevents a purely myopic policy from getting stuck.
        double expansion = 0.0;
        if (board.owner[cell] == -1 ||
            (board.owner[cell] > 0 && board.level[cell] == 1)) {
            array<int, 4> neighboring_values{};
            int count = 0;
            const int r = cell / n;
            const int c = cell % n;
            for (int d = 0; d < 4; ++d) {
                const int nr = r + DR[d];
                const int nc = c + DC[d];
                if (inside(nr, nc)) {
                    const int next = nr * n + nc;
                    if (board.owner[next] != 0) {
                        neighboring_values[count++] = value[next];
                    }
                }
            }
            sort(neighboring_values.begin(), neighboring_values.end(),
                 greater<int>());
            if (count > 0) {
                expansion += 0.20 * neighboring_values[0];
            }
            if (count > 1) {
                expansion += 0.08 * neighboring_values[1];
            }
            expansion *= static_cast<double>(remaining) /
                         static_cast<double>(max(1, total_turns));
        }

        return survive *
               (own_gain + denial_weight * enemy_loss + expansion);
    }

    int sample_particle(const vector<double>& probability, double draw) const {
        double cumulative = 0.0;
        for (size_t i = 0; i < probability.size(); ++i) {
            cumulative += probability[i];
            if (draw < cumulative) {
                return static_cast<int>(i);
            }
        }
        return static_cast<int>(probability.size()) - 1;
    }

    int sample_opponent_action(const GameState& board, int player,
                               const vector<int>& legal, int particle_index,
                               double random_kind,
                               double random_cell) const {
        const Particle& particle = model[player][particle_index];
        if (random_kind < particle.epsilon) {
            int index = static_cast<int>(
                random_cell * static_cast<double>(legal.size()));
            index = min(index, static_cast<int>(legal.size()) - 1);
            return legal[index];
        }

        const auto [best, best_count] =
            best_value_and_count(particle, board, player, legal);
        int selected_rank = static_cast<int>(
            random_cell * static_cast<double>(best_count));
        selected_rank = min(selected_rank, best_count - 1);
        for (int cell : legal) {
            const double score = action_value(particle, board, player, cell);
            const double tolerance = 1e-9 * max(1.0, abs(best));
            if (score >= best - tolerance) {
                if (selected_rank == 0) {
                    return cell;
                }
                --selected_rank;
            }
        }
        return legal.back();
    }

    array<double, MAX_CELLS> particle_move_probability(
        const GameState& board, int player, const vector<int>& legal,
        int particle_index) const {
        array<double, MAX_CELLS> probability{};
        const Particle& particle = model[player][particle_index];
        const auto [best, best_count] =
            best_value_and_count(particle, board, player, legal);
        const double random_part =
            particle.epsilon / static_cast<double>(legal.size());
        const double greedy_part =
            (1.0 - particle.epsilon) / static_cast<double>(best_count);
        for (int cell : legal) {
            probability[cell] = random_part;
            const double score = action_value(particle, board, player, cell);
            const double tolerance = 1e-9 * max(1.0, abs(best));
            if (score >= best - tolerance) {
                probability[cell] += greedy_part;
            }
        }
        return probability;
    }

    GameState simulate_turn(const GameState& before,
                            const array<int, MAX_PLAYERS>& move) const {
        GameState after = before;
        array<int, MAX_CELLS> count{};
        array<char, MAX_PLAYERS> collected{};

        for (int p = 0; p < players; ++p) {
            ++count[move[p]];
        }
        for (int p = 0; p < players; ++p) {
            const int target = move[p];
            if (count[target] >= 2 && before.owner[target] != p) {
                collected[p] = 1;
            }
        }

        for (int p = 0; p < players; ++p) {
            if (collected[p]) {
                continue;
            }
            const int target = move[p];
            const int old_owner = before.owner[target];
            const int old_level = before.level[target];

            if (old_owner == -1) {
                after.owner[target] = p;
                after.level[target] = 1;
                after.position[p] = target;
            } else if (old_owner == p) {
                after.level[target] = min(level_limit, old_level + 1);
                after.position[p] = target;
            } else if (old_level == 1) {
                after.owner[target] = p;
                after.level[target] = 1;
                after.position[p] = target;
            } else {
                after.level[target] = old_level - 1;
                // A failed attack sends the piece back to its old position.
            }
        }
        return after;
    }

    double board_value(const GameState& board) const {
        const auto scores = player_scores(board);
        long long strongest_opponent = 1;
        for (int p = 1; p < players; ++p) {
            strongest_opponent = max(strongest_opponent, scores[p]);
        }
        return log2(1.0 + static_cast<double>(scores[0]) /
                              static_cast<double>(strongest_opponent));
    }

    int choose_rollout_move(
        const GameState& board, const vector<vector<int>>& legal,
        const array<int, MAX_PLAYERS>& representative_particle,
        int remaining) const {
        const auto scores = player_scores(board);
        vector<array<double, MAX_CELLS>> probability(players);
        for (int p = 1; p < players; ++p) {
            probability[p] = particle_move_probability(
                board, p, legal[p], representative_particle[p]);
        }

        int answer = legal[0].front();
        double best = -numeric_limits<double>::infinity();
        for (int cell : legal[0]) {
            const double score =
                quick_score(board, cell, probability, scores, remaining);
            if (score > best || (score == best && cell < answer)) {
                best = score;
                answer = cell;
            }
        }
        return answer;
    }

    // =====================================================================
    // TODO(AHC061): ここから問題依存のrollout接口。Runner本体は編集不要。
    // Solver内のBFS・衝突処理・相手推定も、別問題ではここから呼ぶものを差し替える。
    // =====================================================================
    struct RolloutProblem {
        // TODO: 現在の盤面と、1回の候補比較中は変わらない前計算を置く。
        // 参照先はchoose_actionが終わるまで有効。未来の観測値は入れない。
        struct State {
            const GameState& board;
            int turn;
            const vector<vector<int>>& legal;
            const vector<vector<double>>& posterior;
            const array<int, MAX_PLAYERS>& representative_particle;
            const vector<int>& actions;
        };
        using Action = int;          // TODO: 今選ぶマス番号を返す。
        using Scenario = ::Scenario; // TODO: 相手パラメータと各手の乱数を置く。
        using Score = double;        // TODO: 大きいほど良い盤面評価を返す。
        const Solver& solver;

        // TODO: 最初の1手の合法候補一覧を返す。ここでは安い評価で18個へ絞ったもの。
        const vector<Action>& generate_actions(const State& current) const {
            return current.actions;
        }

        // TODO: 未知の相手行動だけをsampleする。全候補が同じScenarioを使う。
        // 相手パラメータは1シナリオ内では固定し、実際の観測で得た事後分布から引く。
        Scenario generate_scenario(const State& current, Random& random) const {
            Scenario scenario;
            for (int p = 1; p < solver.players; ++p) {
                scenario.particle_index[p] =
                    solver.sample_particle(current.posterior[p], random.uniform());
                for (int depth = 0; depth < LOOKAHEAD; ++depth) {
                    scenario.random_kind[depth][p] = random.uniform();
                    scenario.random_cell[depth][p] = random.uniform();
                }
            }
            return scenario;
        }

        // TODO: candidateを最初に選び、同じ未来乱数で規定手数だけ仮実行する。
        // 戻り値は「自分の点 / 最強の相手の点」に基づくlog評価。
        // 元盤面・粒子の学習結果・Scenarioは変更しない。学習は本物の観測時だけ。
        Score evaluate_action(const State& current, Action candidate,
                              const Scenario& scenario) const {
            array<int, MAX_PLAYERS> moves{};
            moves[0] = candidate;
            for (int p = 1; p < solver.players; ++p) {
                moves[p] = solver.sample_opponent_action(
                    current.board, p, current.legal[p], scenario.particle_index[p],
                    scenario.random_kind[0][p], scenario.random_cell[0][p]);
            }
            GameState rollout = solver.simulate_turn(current.board, moves);
            const int steps = min(LOOKAHEAD, solver.total_turns - current.turn);
            for (int depth = 1; depth < steps; ++depth) {
                vector<vector<int>> legal(solver.players);
                for (int p = 0; p < solver.players; ++p) {
                    legal[p] = solver.legal_moves(rollout, p);
                }
                moves[0] = solver.choose_rollout_move(
                    rollout, legal, current.representative_particle,
                    solver.total_turns - current.turn - depth);
                for (int p = 1; p < solver.players; ++p) {
                    moves[p] = solver.sample_opponent_action(
                        rollout, p, legal[p], scenario.particle_index[p],
                        scenario.random_kind[depth][p], scenario.random_cell[depth][p]);
                }
                rollout = solver.simulate_turn(rollout, moves);
            }
            return solver.board_value(rollout);
        }
    };

    RolloutProblem rollout_problem{*this};
    // 旧版と乱数列、doubleの加算順、近似同点の扱いを揃える。
    // Runnerを毎ターン作り直すとseedが戻るのでSolverと同じ寿命にする。
    CommonScenarioRolloutRunner<RolloutProblem, Random, double> runner{
        rollout_problem, 0x06120260213ULL};

    int choose_move(
        int turn, const vector<vector<int>>& legal,
        const vector<array<double, MAX_CELLS>>& opponent_probability) {
        const int remaining = total_turns - turn - 1;
        const auto scores = player_scores(state);

        vector<pair<double, int>> ranked;
        ranked.reserve(legal[0].size());
        for (int cell : legal[0]) {
            ranked.push_back(
                {quick_score(state, cell, opponent_probability, scores,
                             remaining),
                 cell});
        }
        sort(ranked.begin(), ranked.end(),
             [](const auto& left, const auto& right) {
                 if (left.first != right.first) {
                     return left.first > right.first;
                 }
                 return left.second < right.second;
             });
        if (static_cast<int>(ranked.size()) > FINAL_CANDIDATES) {
            ranked.resize(FINAL_CANDIDATES);
        }

        vector<vector<double>> posterior(players);
        array<int, MAX_PLAYERS> representative_particle{};
        for (int p = 1; p < players; ++p) {
            posterior[p] = normalized_particle_weights(p);
            representative_particle[p] = static_cast<int>(
                max_element(posterior[p].begin(), posterior[p].end()) -
                posterior[p].begin());
        }

        vector<int> actions;
        actions.reserve(ranked.size());
        for (const auto& entry : ranked) actions.push_back(entry.second);
        const RolloutProblem::State current{
            state, turn, legal, posterior, representative_particle, actions};
        return runner.choose_action(current, SCENARIO_COUNT,
            [](int candidate, double average, int best, double best_average) {
                // TODO: 誤差1e-12以内なら小さいマス番号を優先する。
                return average > best_average + 1e-12 ||
                       (abs(average - best_average) <= 1e-12 && candidate < best);
            });
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Solver solver;
    solver.run();
    return 0;
}
