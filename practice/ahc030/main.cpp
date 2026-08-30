#include <bits/stdc++.h>
using namespace std;

// Compile with -DAHC030_FULL_DRILL_BASELINE for the statement's baseline.
// Compile with -DAHC030_STATIC_ORDER to disable updates after each drilling.
// Compile with -DAHC030_NO_SURVEY to omit the initial row/column surveys.

#ifndef AHC030_SURVEY_WEIGHT
#define AHC030_SURVEY_WEIGHT 0.8
#endif

#ifndef AHC030_SURVEY_MIN_FIELDS
#define AHC030_SURVEY_MIN_FIELDS 8
#endif

#ifndef AHC030_ENTROPY_WEIGHT
#define AHC030_ENTROPY_WEIGHT 4.0
#endif

#ifndef AHC030_POSITIVE_WEIGHT
#define AHC030_POSITIVE_WEIGHT 0.0
#endif

constexpr int MAX_CELLS = 400;

struct Placement {
    bitset<MAX_CELLS> cells;
};

struct OilField {
    vector<Placement> placements;
    vector<char> active;
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N, M;
    double epsilon;
    cin >> N >> M >> epsilon;
    (void)epsilon;

    vector<vector<pair<int, int>>> shapes(M);
    int total_oil_units = 0;
    for (int field = 0; field < M; ++field) {
        int area;
        cin >> area;
        total_oil_units += area;
        shapes[field].resize(area);
        for (auto& [row, column] : shapes[field]) cin >> row >> column;
    }

    const int cell_count = N * N;
    vector<int> drilled_value(cell_count, -1);
    int operation_count = 0;

    auto drill = [&](int cell) {
        cout << "q 1 " << cell / N << ' ' << cell % N << endl;
        int value;
        cin >> value;
        ++operation_count;
        drilled_value[cell] = value;
        return value;
    };

    auto answer = [&](const vector<int>& cells) {
        cout << "a " << cells.size();
        for (int cell : cells) cout << ' ' << cell / N << ' ' << cell % N;
        cout << endl;
        int result;
        cin >> result;
        ++operation_count;
        return result;
    };

    [[maybe_unused]] auto survey = [&](const vector<int>& cells) {
        cout << "q " << cells.size();
        for (int cell : cells) cout << ' ' << cell / N << ' ' << cell % N;
        cout << endl;
        int value;
        cin >> value;
        ++operation_count;
        return value;
    };

#ifdef AHC030_FULL_DRILL_BASELINE
    vector<int> baseline_oil_cells;
    for (int cell = 0; cell < cell_count; ++cell) {
        if (drill(cell) > 0) baseline_oil_cells.push_back(cell);
    }
    answer(baseline_oil_cells);
    return 0;
#endif

    vector<OilField> fields(M);
    for (int field = 0; field < M; ++field) {
        int maximum_row = 0;
        int maximum_column = 0;
        for (const auto& [row, column] : shapes[field]) {
            maximum_row = max(maximum_row, row);
            maximum_column = max(maximum_column, column);
        }
        for (int top = 0; top + maximum_row < N; ++top) {
            for (int left = 0; left + maximum_column < N; ++left) {
                Placement placement;
                for (const auto& [row, column] : shapes[field]) {
                    placement.cells.set((top + row) * N + left + column);
                }
                fields[field].placements.push_back(placement);
            }
        }
        fields[field].active.assign(fields[field].placements.size(), true);
    }

    vector<pair<int, int>> observations;
    vector<pair<int, int>> valid_pairs;
    vector<double> survey_hint(cell_count, 0.0);

#if !defined(AHC030_NO_SURVEY) && !defined(AHC030_STATIC_ORDER)
    // Four large groups cost only about 8/N in total.  Their noisy values are
    // used as a weak ordering hint, never as proof for the final answer.
    if (M >= AHC030_SURVEY_MIN_FIELDS) {
        for (int block_row = 0; block_row < 2; ++block_row) {
            for (int block_column = 0; block_column < 2; ++block_column) {
                vector<int> cells;
                for (int row = 0; row < N; ++row) {
                    for (int column = 0; column < N; ++column) {
                        if (row * 2 / N == block_row
                            && column * 2 / N == block_column) {
                            cells.push_back(row * N + column);
                        }
                    }
                }
                const int response = survey(cells);
                const double estimated_sum = max(
                    0.0,
                    (response - static_cast<double>(cells.size()) * epsilon)
                    / (1.0 - 2.0 * epsilon));
                const double density =
                    estimated_sum / static_cast<double>(cells.size());
                for (int cell : cells) survey_hint[cell] = density;
            }
        }
    }
#endif

    // This is arc consistency for constraints
    //   cover_0(cell) + ... + cover_(M-1)(cell) = observed value.
    // It only removes a placement when even the best choices of the other
    // fields cannot satisfy an exact drilling result.
    auto propagate = [&]() {
#ifdef AHC030_STATIC_ORDER
        return;
#else
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& [cell, observed] : observations) {
                vector<char> can_avoid(M, false);
                vector<char> can_cover(M, false);
                int forced_cover = 0;
                int possible_cover = 0;

                for (int field = 0; field < M; ++field) {
                    for (int index = 0;
                         index < static_cast<int>(fields[field].placements.size());
                         ++index) {
                        if (!fields[field].active[index]) continue;
                        if (fields[field].placements[index].cells.test(cell)) {
                            can_cover[field] = true;
                        } else {
                            can_avoid[field] = true;
                        }
                    }
                    if (can_cover[field] && !can_avoid[field]) ++forced_cover;
                    if (can_cover[field]) ++possible_cover;
                }

                for (int field = 0; field < M; ++field) {
                    const int other_forced =
                        forced_cover - (can_cover[field] && !can_avoid[field]);
                    const int other_possible = possible_cover - can_cover[field];
                    for (int index = 0;
                         index < static_cast<int>(fields[field].placements.size());
                         ++index) {
                        if (!fields[field].active[index]) continue;
                        const int here =
                            fields[field].placements[index].cells.test(cell) ? 1 : 0;
                        if (other_forced > observed - here
                            || observed - here > other_possible) {
                            fields[field].active[index] = false;
                            changed = true;
                        }
                    }
                }
            }
        }

        // With two fields, checking every pair of translations is still small
        // enough.  This enforces all observations together, not one at a time.
        if (M == 2) {
            valid_pairs.clear();
            vector<char> used_first(fields[0].placements.size(), false);
            vector<char> used_second(fields[1].placements.size(), false);
            for (int first = 0;
                 first < static_cast<int>(fields[0].placements.size()); ++first) {
                if (!fields[0].active[first]) continue;
                for (int second = 0;
                     second < static_cast<int>(fields[1].placements.size()); ++second) {
                    if (!fields[1].active[second]) continue;
                    bool valid = true;
                    for (const auto& [cell, observed] : observations) {
                        const int predicted =
                            static_cast<int>(fields[0].placements[first].cells.test(cell))
                            + static_cast<int>(
                                fields[1].placements[second].cells.test(cell));
                        if (predicted != observed) {
                            valid = false;
                            break;
                        }
                    }
                    if (valid) {
                        valid_pairs.push_back({first, second});
                        used_first[first] = true;
                        used_second[second] = true;
                    }
                }
            }
            for (int index = 0;
                 index < static_cast<int>(fields[0].active.size()); ++index) {
                fields[0].active[index] &= used_first[index];
            }
            for (int index = 0;
                 index < static_cast<int>(fields[1].active.size()); ++index) {
                fields[1].active[index] &= used_second[index];
            }
        }
#endif
    };

    // Returns true only when the independent candidate sets themselves prove
    // the oil/non-oil status of every cell.  Correlations are not guessed.
    auto find_proven_answer = [&](vector<int>& oil_cells) {
        oil_cells.clear();
        if (M == 2 && !valid_pairs.empty()) {
            bitset<MAX_CELLS> possibly_oil;
            bitset<MAX_CELLS> certainly_oil;
            certainly_oil.set();
            for (const auto& [first, second] : valid_pairs) {
                const bitset<MAX_CELLS> occupied =
                    fields[0].placements[first].cells
                    | fields[1].placements[second].cells;
                possibly_oil |= occupied;
                certainly_oil &= occupied;
            }
            if (possibly_oil == certainly_oil) {
                for (int cell = 0; cell < cell_count; ++cell) {
                    if (certainly_oil.test(cell)) oil_cells.push_back(cell);
                }
                return true;
            }
        }

        for (int cell = 0; cell < cell_count; ++cell) {
            bool possibly_oil = false;
            bool certainly_oil = false;
            for (int field = 0; field < M; ++field) {
                bool any_cover = false;
                bool all_cover = true;
                int active_count = 0;
                for (int index = 0;
                     index < static_cast<int>(fields[field].placements.size());
                     ++index) {
                    if (!fields[field].active[index]) continue;
                    ++active_count;
                    const bool covers =
                        fields[field].placements[index].cells.test(cell);
                    any_cover |= covers;
                    all_cover &= covers;
                }
                if (active_count == 0) return false;
                possibly_oil |= any_cover;
                certainly_oil |= all_cover;
            }
            if (certainly_oil) {
                oil_cells.push_back(cell);
            } else if (possibly_oil) {
                return false;
            }
        }
        return true;
    };

    // Probability and entropy are only used to choose the next exact query.
    // They never decide the final answer, so an inaccurate independence
    // approximation cannot make the output wrong.
    auto choose_next_cell = [&]() {
        vector<array<int, 3>> exact_count;
        if (M == 2 && !valid_pairs.empty()
            && valid_pairs.size() <= 30000) {
            exact_count.assign(cell_count, {0, 0, 0});
            for (const auto& [first, second] : valid_pairs) {
                for (int cell = 0; cell < cell_count; ++cell) {
                    const int count =
                        static_cast<int>(fields[0].placements[first].cells.test(cell))
                        + static_cast<int>(
                            fields[1].placements[second].cells.test(cell));
                    ++exact_count[cell][count];
                }
            }
        }

        int best_cell = -1;
        double best_priority = -1.0;
        for (int cell = 0; cell < cell_count; ++cell) {
            if (drilled_value[cell] != -1) continue;

            vector<double> count_probability(1, 1.0);
            double expected_oil = 0.0;
            bool possibly_oil = false;
            if (!exact_count.empty()) {
                count_probability.assign(3, 0.0);
                for (int count = 0; count <= 2; ++count) {
                    count_probability[count] =
                        static_cast<double>(exact_count[cell][count])
                        / static_cast<double>(valid_pairs.size());
                    expected_oil += count * count_probability[count];
                }
                possibly_oil = exact_count[cell][1] + exact_count[cell][2] > 0;
            } else {
                for (int field = 0; field < M; ++field) {
                    int active_count = 0;
                    int cover_count = 0;
                    for (int index = 0;
                         index < static_cast<int>(fields[field].placements.size());
                         ++index) {
                        if (!fields[field].active[index]) continue;
                        ++active_count;
                        cover_count +=
                            fields[field].placements[index].cells.test(cell);
                    }
                    const double cover_probability =
                        static_cast<double>(cover_count) / active_count;
                    expected_oil += cover_probability;
                    possibly_oil |= cover_count > 0;

                    vector<double> next(count_probability.size() + 1, 0.0);
                    for (int count = 0;
                         count < static_cast<int>(count_probability.size());
                         ++count) {
                        next[count] += count_probability[count]
                                     * (1.0 - cover_probability);
                        next[count + 1] += count_probability[count]
                                         * cover_probability;
                    }
                    count_probability = move(next);
                }
            }
            if (!possibly_oil) continue;

            double entropy = 0.0;
            for (double probability : count_probability) {
                if (probability > 1e-15) {
                    entropy -= probability * log(probability);
                }
            }
            const double positive_probability = 1.0 - count_probability[0];
            const double priority = expected_oil
                                  + AHC030_ENTROPY_WEIGHT * entropy
                                  + AHC030_POSITIVE_WEIGHT * positive_probability
                                  + AHC030_SURVEY_WEIGHT * survey_hint[cell];
            if (priority > best_priority) {
                best_priority = priority;
                best_cell = cell;
            }
        }
        return best_cell;
    };

    int found_oil_units = 0;
    bool proof_answer_rejected = false;
    while (operation_count + 1 < 2 * N * N) {
        vector<int> proven_oil_cells;
        if (!proof_answer_rejected
            && find_proven_answer(proven_oil_cells)) {
            if (answer(proven_oil_cells) == 1) return 0;
            proof_answer_rejected = true;
        }

        if (found_oil_units == total_oil_units) {
            vector<int> oil_cells;
            for (int cell = 0; cell < cell_count; ++cell) {
                if (drilled_value[cell] > 0) oil_cells.push_back(cell);
            }
            if (answer(oil_cells) == 1) return 0;
            break;
        }

        const int cell = choose_next_cell();
        if (cell == -1) break;
        const int value = drill(cell);
        found_oil_units += value;
        observations.push_back({cell, value});
        propagate();
    }

    // Defensive fallback.  In normal operation the known total is reached
    // after at most N*N drillings, well before the 2*N*N operation limit.
    for (int cell = 0; cell < cell_count; ++cell) {
        if (drilled_value[cell] == -1) drill(cell);
    }
    vector<int> oil_cells;
    for (int cell = 0; cell < cell_count; ++cell) {
        if (drilled_value[cell] > 0) oil_cells.push_back(cell);
    }
    answer(oil_cells);
}
