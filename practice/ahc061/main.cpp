#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

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
    Random random{0x06120260213ULL};

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

        vector<Scenario> scenarios(SCENARIO_COUNT);
        for (int sample = 0; sample < SCENARIO_COUNT; ++sample) {
            for (int p = 1; p < players; ++p) {
                scenarios[sample].particle_index[p] =
                    sample_particle(posterior[p], random.uniform());
                for (int depth = 0; depth < LOOKAHEAD; ++depth) {
                    scenarios[sample].random_kind[depth][p] =
                        random.uniform();
                    scenarios[sample].random_cell[depth][p] =
                        random.uniform();
                }
            }
        }

        int answer = ranked.front().second;
        double best_average = -numeric_limits<double>::infinity();
        for (const auto& [quick, candidate] : ranked) {
            (void)quick;
            double sum = 0.0;
            for (const Scenario& scenario : scenarios) {
                array<int, MAX_PLAYERS> moves{};
                moves[0] = candidate;
                for (int p = 1; p < players; ++p) {
                    moves[p] = sample_opponent_action(
                        state, p, legal[p], scenario.particle_index[p],
                        scenario.random_kind[0][p],
                        scenario.random_cell[0][p]);
                }
                GameState rollout = simulate_turn(state, moves);

                const int steps = min(LOOKAHEAD, total_turns - turn);
                for (int depth = 1; depth < steps; ++depth) {
                    vector<vector<int>> next_legal(players);
                    for (int p = 0; p < players; ++p) {
                        next_legal[p] = legal_moves(rollout, p);
                    }
                    moves[0] = choose_rollout_move(
                        rollout, next_legal, representative_particle,
                        remaining - depth + 1);
                    for (int p = 1; p < players; ++p) {
                        moves[p] = sample_opponent_action(
                            rollout, p, next_legal[p],
                            scenario.particle_index[p],
                            scenario.random_kind[depth][p],
                            scenario.random_cell[depth][p]);
                    }
                    rollout = simulate_turn(rollout, moves);
                }
                sum += board_value(rollout);
            }
            const double average = sum / SCENARIO_COUNT;
            if (average > best_average + 1e-12 ||
                (abs(average - best_average) <= 1e-12 &&
                 candidate < answer)) {
                best_average = average;
                answer = candidate;
            }
        }
        return answer;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Solver solver;
    solver.run();
    return 0;
}
