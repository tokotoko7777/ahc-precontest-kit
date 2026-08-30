#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace std;

struct Timer {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    double milliseconds() const {
        return chrono::duration<double, milli>(chrono::steady_clock::now() - start)
            .count();
    }
};

struct Random {
    uint64_t state;

    explicit Random(uint64_t seed) : state(seed) {}

    uint32_t next_u32() {
        state ^= state << 7;
        state ^= state >> 9;
        return static_cast<uint32_t>(state);
    }

    int next_int(int upper) {
        return static_cast<int>(next_u32() % static_cast<uint32_t>(upper));
    }

    double next_double() {
        return (next_u32() + 0.5) / 4294967296.0;
    }
};

struct Solver {
    static constexpr int BOARD_SIZE = 15;
    static constexpr int CELL_COUNT = BOARD_SIZE * BOARD_SIZE;
    static constexpr int LETTER_COUNT = 26;
    static constexpr int INF = numeric_limits<int>::max() / 4;

    int word_count = 0;
    int start_cell = 0;
    array<char, CELL_COUNT> letter{};
    vector<string> word;
    array<vector<int>, LETTER_COUNT> positions;
    array<array<int, CELL_COUNT>, CELL_COUNT> move_cost{};
    vector<vector<int>> overlap;
    vector<vector<int>> pair_cost;
    vector<int> start_cost;

    int exact_text_cost(const string& text) const {
        array<int, CELL_COUNT> previous{};
        array<int, CELL_COUNT> current{};
        int previous_letter = -1;

        for (int index = 0; index < static_cast<int>(text.size()); ++index) {
            const int current_letter = text[index] - 'A';
            const vector<int>& current_positions = positions[current_letter];
            if (index == 0) {
                for (int current_index = 0;
                     current_index < static_cast<int>(current_positions.size());
                     ++current_index) {
                    current[current_index] =
                        move_cost[start_cell][current_positions[current_index]];
                }
            } else {
                const vector<int>& previous_positions = positions[previous_letter];
                for (int current_index = 0;
                     current_index < static_cast<int>(current_positions.size());
                     ++current_index) {
                    int best = INF;
                    for (int previous_index = 0;
                         previous_index < static_cast<int>(previous_positions.size());
                         ++previous_index) {
                        best = min(best, previous[previous_index] +
                                             move_cost[previous_positions[previous_index]]
                                                      [current_positions[current_index]]);
                    }
                    current[current_index] = best;
                }
            }
            for (int current_index = 0;
                 current_index < static_cast<int>(current_positions.size());
                 ++current_index) {
                previous[current_index] = current[current_index];
            }
            previous_letter = current_letter;
        }

        int answer = INF;
        for (int index = 0; index < static_cast<int>(positions[previous_letter].size());
             ++index) {
            answer = min(answer, previous[index]);
        }
        return answer;
    }

    // The finger may start at any occurrence of previous_letter at no cost.
    // This gives a cheap pairwise estimate for ordering two words.
    int append_cost_after_letter(int previous_letter, string_view appended) const {
        array<int, CELL_COUNT> previous{};
        array<int, CELL_COUNT> current{};
        const vector<int>* previous_positions = &positions[previous_letter];
        for (int index = 0; index < static_cast<int>(previous_positions->size());
             ++index) {
            previous[index] = 0;
        }

        for (char character : appended) {
            const int current_letter = character - 'A';
            const vector<int>& current_positions = positions[current_letter];
            for (int current_index = 0;
                 current_index < static_cast<int>(current_positions.size());
                 ++current_index) {
                int best = INF;
                for (int previous_index = 0;
                     previous_index < static_cast<int>(previous_positions->size());
                     ++previous_index) {
                    best = min(best, previous[previous_index] +
                                         move_cost[(*previous_positions)[previous_index]]
                                                  [current_positions[current_index]]);
                }
                current[current_index] = best;
            }
            for (int current_index = 0;
                 current_index < static_cast<int>(current_positions.size());
                 ++current_index) {
                previous[current_index] = current[current_index];
            }
            previous_positions = &current_positions;
        }

        int answer = INF;
        for (int index = 0; index < static_cast<int>(previous_positions->size());
             ++index) {
            answer = min(answer, previous[index]);
        }
        return answer;
    }

    string make_text(const vector<int>& order) const {
        string text = word[order[0]];
        text.reserve(word_count * 5);
        for (int index = 1; index < word_count; ++index) {
            const int previous_word = order[index - 1];
            const int current_word = order[index];
            text += word[current_word].substr(overlap[previous_word][current_word]);
        }
        return text;
    }

    int exact_order_cost(const vector<int>& order) const {
        return exact_text_cost(make_text(order));
    }

    int approximate_order_cost(const vector<int>& order) const {
        int cost = start_cost[order[0]];
        for (int index = 1; index < word_count; ++index) {
            cost += pair_cost[order[index - 1]][order[index]];
        }
        return cost;
    }

    vector<int> nearest_neighbor_order(int first_word) const {
        vector<int> order;
        order.reserve(word_count);
        vector<char> used(word_count, false);
        order.push_back(first_word);
        used[first_word] = true;

        while (static_cast<int>(order.size()) < word_count) {
            const int previous_word = order.back();
            int next_word = -1;
            int best_cost = INF;
            for (int candidate = 0; candidate < word_count; ++candidate) {
                if (used[candidate]) continue;
                if (pair_cost[previous_word][candidate] < best_cost) {
                    best_cost = pair_cost[previous_word][candidate];
                    next_word = candidate;
                }
            }
            used[next_word] = true;
            order.push_back(next_word);
        }
        return order;
    }

    vector<int> randomized_neighbor_order(int first_word, Random& random) const {
        vector<int> order;
        order.reserve(word_count);
        vector<char> used(word_count, false);
        order.push_back(first_word);
        used[first_word] = true;

        while (static_cast<int>(order.size()) < word_count) {
            const int previous_word = order.back();
            array<pair<int, int>, 4> best{};
            int best_count = 0;
            for (int candidate = 0; candidate < word_count; ++candidate) {
                if (used[candidate]) continue;
                const pair<int, int> value = {pair_cost[previous_word][candidate],
                                              candidate};
                int place;
                if (best_count < static_cast<int>(best.size())) {
                    place = best_count;
                    best[best_count++] = value;
                } else if (value < best.back()) {
                    place = best_count - 1;
                    best.back() = value;
                } else {
                    continue;
                }
                while (place > 0 && best[place] < best[place - 1]) {
                    swap(best[place], best[place - 1]);
                    --place;
                }
            }
            const int choice_limit = min(3, best_count);
            const int next_word = best[random.next_int(choice_limit)].second;
            used[next_word] = true;
            order.push_back(next_word);
        }
        return order;
    }

    vector<int> insertion_order(int first_word, Random& random) const {
        vector<int> order = {first_word};
        vector<char> used(word_count, false);
        used[first_word] = true;

        while (static_cast<int>(order.size()) < word_count) {
            int chosen_word = -1;
            int chosen_position = -1;
            int best_value = INF;
            for (int candidate = 0; candidate < word_count; ++candidate) {
                if (used[candidate]) continue;
                for (int position = 0;
                     position <= static_cast<int>(order.size()); ++position) {
                    int difference;
                    if (position == 0) {
                        difference = start_cost[candidate] +
                                     pair_cost[candidate][order[0]] -
                                     start_cost[order[0]];
                    } else if (position == static_cast<int>(order.size())) {
                        difference = pair_cost[order.back()][candidate];
                    } else {
                        const int left = order[position - 1];
                        const int right = order[position];
                        difference = pair_cost[left][candidate] +
                                     pair_cost[candidate][right] - pair_cost[left][right];
                    }
                    // Only break exact ties randomly; the main criterion stays
                    // easy to understand and deterministic in scale.
                    const int value = difference * 8 + random.next_int(8);
                    if (value < best_value) {
                        best_value = value;
                        chosen_word = candidate;
                        chosen_position = position;
                    }
                }
            }
            order.insert(order.begin() + chosen_position, chosen_word);
            used[chosen_word] = true;
        }
        return order;
    }

    void apply_random_move(vector<int>& order, Random& random, int& move_type,
                           int& first, int& second) const {
        move_type = random.next_int(3);
        first = random.next_int(word_count);
        second = random.next_int(word_count);
        if (first == second) second = (second + 1) % word_count;

        if (move_type == 0) {
            swap(order[first], order[second]);
        } else if (move_type == 1) {
            const int value = order[first];
            order.erase(order.begin() + first);
            order.insert(order.begin() + second, value);
        } else {
            if (first > second) swap(first, second);
            reverse(order.begin() + first, order.begin() + second + 1);
        }
    }

    void undo_random_move(vector<int>& order, int move_type, int first,
                          int second) const {
        if (move_type == 0) {
            swap(order[first], order[second]);
        } else if (move_type == 1) {
            const int value = order[second];
            order.erase(order.begin() + second);
            order.insert(order.begin() + first, value);
        } else {
            reverse(order.begin() + first, order.begin() + second + 1);
        }
    }

    void apply_guided_move(vector<int>& order, Random& random, int& move_type,
                           int& first, int& second) const {
        // Remove one word and reinsert it at one of the three best places
        // according to the fast pairwise estimate.
        move_type = 1;
        first = random.next_int(word_count);
        const int value = order[first];
        order.erase(order.begin() + first);

        array<pair<int, int>, 3> best{};
        int best_count = 0;
        for (int position = 0; position < word_count; ++position) {
            if (position == first) continue;
            int difference;
            if (position == 0) {
                difference = start_cost[value] + pair_cost[value][order[0]] -
                             start_cost[order[0]];
            } else if (position == word_count - 1) {
                difference = pair_cost[order.back()][value];
            } else {
                difference = pair_cost[order[position - 1]][value] +
                             pair_cost[value][order[position]] -
                             pair_cost[order[position - 1]][order[position]];
            }
            const pair<int, int> candidate = {difference, position};
            int place;
            if (best_count < static_cast<int>(best.size())) {
                place = best_count;
                best[best_count++] = candidate;
            } else if (candidate < best.back()) {
                place = best_count - 1;
                best.back() = candidate;
            } else {
                continue;
            }
            while (place > 0 && best[place] < best[place - 1]) {
                swap(best[place], best[place - 1]);
                --place;
            }
        }
        const int random_value = random.next_int(6);
        const int chosen = min(best_count - 1,
                               random_value < 3 ? 0 : (random_value < 5 ? 1 : 2));
        second = best[chosen].second;
        order.insert(order.begin() + second, value);
    }

    vector<pair<int, int>> restore_path(const string& text) const {
        vector<vector<int>> parent(text.size());
        array<int, CELL_COUNT> previous{};
        array<int, CELL_COUNT> current{};
        int previous_letter = -1;

        for (int index = 0; index < static_cast<int>(text.size()); ++index) {
            const int current_letter = text[index] - 'A';
            const vector<int>& current_positions = positions[current_letter];
            parent[index].assign(current_positions.size(), -1);
            if (index == 0) {
                for (int current_index = 0;
                     current_index < static_cast<int>(current_positions.size());
                     ++current_index) {
                    current[current_index] =
                        move_cost[start_cell][current_positions[current_index]];
                }
            } else {
                const vector<int>& previous_positions = positions[previous_letter];
                for (int current_index = 0;
                     current_index < static_cast<int>(current_positions.size());
                     ++current_index) {
                    current[current_index] = INF;
                    for (int previous_index = 0;
                         previous_index < static_cast<int>(previous_positions.size());
                         ++previous_index) {
                        const int candidate =
                            previous[previous_index] +
                            move_cost[previous_positions[previous_index]]
                                     [current_positions[current_index]];
                        if (candidate < current[current_index]) {
                            current[current_index] = candidate;
                            parent[index][current_index] = previous_index;
                        }
                    }
                }
            }
            for (int current_index = 0;
                 current_index < static_cast<int>(current_positions.size());
                 ++current_index) {
                previous[current_index] = current[current_index];
            }
            previous_letter = current_letter;
        }

        int chosen_index = 0;
        for (int index = 1; index < static_cast<int>(positions[previous_letter].size());
             ++index) {
            if (previous[index] < previous[chosen_index]) chosen_index = index;
        }

        vector<pair<int, int>> answer(text.size());
        for (int index = static_cast<int>(text.size()) - 1; index >= 0; --index) {
            const int current_letter = text[index] - 'A';
            const int cell = positions[current_letter][chosen_index];
            answer[index] = {cell / BOARD_SIZE, cell % BOARD_SIZE};
            chosen_index = parent[index][chosen_index];
        }
        return answer;
    }

    vector<pair<int, int>> greedy_path(const string& text) const {
        vector<pair<int, int>> answer;
        answer.reserve(text.size());
        int current_cell = start_cell;
        for (char character : text) {
            const vector<int>& candidates = positions[character - 'A'];
            int chosen = candidates[0];
            for (int candidate : candidates) {
                if (move_cost[current_cell][candidate] < move_cost[current_cell][chosen]) {
                    chosen = candidate;
                }
            }
            answer.push_back({chosen / BOARD_SIZE, chosen % BOARD_SIZE});
            current_cell = chosen;
        }
        return answer;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m, start_row, start_column;
    cin >> n >> m >> start_row >> start_column;
    Solver solver;
    solver.word_count = m;
    solver.start_cell = start_row * n + start_column;

    uint64_t input_hash = 1469598103934665603ULL;
    for (int row = 0; row < n; ++row) {
        string line;
        cin >> line;
        for (int column = 0; column < n; ++column) {
            const int cell = row * n + column;
            solver.letter[cell] = line[column];
            solver.positions[line[column] - 'A'].push_back(cell);
            input_hash ^= static_cast<uint64_t>(line[column] + cell * 257);
            input_hash *= 1099511628211ULL;
        }
    }
    solver.word.resize(m);
    for (string& text : solver.word) {
        cin >> text;
        for (char character : text) {
            input_hash ^= static_cast<uint64_t>(character);
            input_hash *= 1099511628211ULL;
        }
    }

    for (int from = 0; from < Solver::CELL_COUNT; ++from) {
        for (int to = 0; to < Solver::CELL_COUNT; ++to) {
            solver.move_cost[from][to] = abs(from / n - to / n) +
                                         abs(from % n - to % n) + 1;
        }
    }

    solver.overlap.assign(m, vector<int>(m, 0));
    solver.pair_cost.assign(m, vector<int>(m, 0));
    solver.start_cost.resize(m);
    for (int first = 0; first < m; ++first) {
        solver.start_cost[first] = solver.exact_text_cost(solver.word[first]);
        for (int second = 0; second < m; ++second) {
            if (first == second) continue;
            for (int length = 4; length >= 1; --length) {
                if (solver.word[first].substr(5 - length) ==
                    solver.word[second].substr(0, length)) {
                    solver.overlap[first][second] = length;
                    break;
                }
            }
            const int length = solver.overlap[first][second];
            const string_view appended(solver.word[second].data() + length,
                                       solver.word[second].size() - length);
            solver.pair_cost[first][second] = solver.append_cost_after_letter(
                solver.word[first].back() - 'A', appended);
        }
    }

#ifdef AHC028_BASELINE
    string baseline_text;
    baseline_text.reserve(m * 5);
    for (const string& text : solver.word) baseline_text += text;
    const vector<pair<int, int>> baseline_answer = solver.greedy_path(baseline_text);
    for (auto [row, column] : baseline_answer) cout << row << ' ' << column << '\n';
    return 0;
#endif

    Timer timer;
    Random random(input_hash ^ 0x9e3779b97f4a7c15ULL);
#ifdef LOCAL_SHORT_TIME
    constexpr double DEADLINE_MS = 90.0;
#else
    constexpr double DEADLINE_MS = 1850.0;
#endif
    const double approximate_phase_end = DEADLINE_MS * 0.18;

    vector<int> best_order(m);
    iota(best_order.begin(), best_order.end(), 0);
    int best_exact_cost = solver.exact_order_cost(best_order);

#ifdef AHC028_INPUT_ORDER_DP
    const string input_order_text = solver.make_text(best_order);
    const vector<pair<int, int>> input_order_answer =
        solver.restore_path(input_order_text);
    for (auto [row, column] : input_order_answer) cout << row << ' ' << column << '\n';
    return 0;
#endif

    auto consider = [&](const vector<int>& candidate) {
        const int cost = solver.exact_order_cost(candidate);
        if (cost < best_exact_cost) {
            best_exact_cost = cost;
            best_order = candidate;
        }
    };

    for (int first = 0; first < m; ++first) {
        consider(solver.nearest_neighbor_order(first));
    }
    for (int trial = 0; trial < 12; ++trial) {
        consider(solver.insertion_order(random.next_int(m), random));
    }
    for (int trial = 0; trial < 160; ++trial) {
        consider(solver.randomized_neighbor_order(random.next_int(m), random));
    }

    vector<int> current_order = best_order;
    int current_approximate_cost = solver.approximate_order_cost(current_order);
    int iteration = 0;
    double current_time = timer.milliseconds();
    while (current_time < approximate_phase_end) {
        if ((iteration & 255) == 0) current_time = timer.milliseconds();
        ++iteration;
        int move_type, first, second;
        solver.apply_random_move(current_order, random, move_type, first, second);
        const int next_cost = solver.approximate_order_cost(current_order);
        const int difference = next_cost - current_approximate_cost;
        const double progress = min(1.0, current_time / approximate_phase_end);
        const double temperature = 5.0 * (1.0 - progress) + 0.15 * progress;
        if (difference <= 0 ||
            random.next_double() < exp(-difference / temperature)) {
            current_approximate_cost = next_cost;
            if ((iteration & 2047) == 0) consider(current_order);
        } else {
            solver.undo_random_move(current_order, move_type, first, second);
        }
    }
    consider(current_order);

    // The final phase uses the exact position-DP cost.  It is more expensive,
    // but it corrects cases where good string overlap causes awkward movement.
    current_order = best_order;
    int current_exact_cost = best_exact_cost;
    iteration = 0;
    current_time = timer.milliseconds();
    while (current_time < DEADLINE_MS) {
        if ((iteration & 7) == 0) current_time = timer.milliseconds();
        ++iteration;
        int move_type, first, second;
        solver.apply_guided_move(current_order, random, move_type, first, second);
        const int next_cost = solver.exact_order_cost(current_order);
        const int difference = next_cost - current_exact_cost;
        const double progress = max(0.0, min(1.0,
            (current_time - approximate_phase_end) /
            max(1.0, DEADLINE_MS - approximate_phase_end)));
        const double temperature = 2.0 * (1.0 - progress) + 0.05 * progress;
        if (difference <= 0 ||
            random.next_double() < exp(-difference / temperature)) {
            current_exact_cost = next_cost;
            if (current_exact_cost < best_exact_cost) {
                best_exact_cost = current_exact_cost;
                best_order = current_order;
            }
        } else {
            solver.undo_random_move(current_order, move_type, first, second);
        }
    }

    const string answer_text = solver.make_text(best_order);
    const vector<pair<int, int>> answer = solver.restore_path(answer_text);
    for (auto [row, column] : answer) cout << row << ' ' << column << '\n';
    return 0;
}
