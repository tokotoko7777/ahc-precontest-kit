#include <algorithm>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace std;

struct Box {
    int row;
    int col;
    long long weight;
    long long durability;
};

struct Plan {
    vector<vector<int>> trips;
    long long moves = 0;
};

int grid_size;
vector<Box> boxes;

int distance_between(int first, int second) {
    return abs(boxes[first].row - boxes[second].row) +
           abs(boxes[first].col - boxes[second].col);
}

int distance_from_exit(int id) {
    return boxes[id].row + boxes[id].col;
}

long long trip_length(const vector<int>& trip) {
    if (trip.empty()) return 0;
    long long result = distance_from_exit(trip.front());
    for (int i = 1; i < static_cast<int>(trip.size()); ++i) {
        result += distance_between(trip[i - 1], trip[i]);
    }
    result += distance_from_exit(trip.back());
    return result;
}

// Checks all durability changes for one trip.  The vector order is also the
// order in which boxes are picked up (bottom of the stack first).
bool is_safe(const vector<int>& trip) {
    vector<long long> remaining;
    int previous = -1;  // -1 means the exit (0, 0).

    for (int id : trip) {
        const int steps = (previous == -1) ? distance_from_exit(id)
                                            : distance_between(previous, id);
        long long weight_above = 0;
        for (int i = static_cast<int>(remaining.size()) - 1; i >= 0; --i) {
            remaining[i] -= weight_above * steps;
            if (remaining[i] <= 0) return false;
            weight_above += boxes[trip[i]].weight;
        }
        remaining.push_back(boxes[id].durability);
        previous = id;
    }

    const int steps_home = trip.empty() ? 0 : distance_from_exit(trip.back());
    long long weight_above = 0;
    for (int i = static_cast<int>(remaining.size()) - 1; i >= 0; --i) {
        remaining[i] -= weight_above * steps_home;
        if (remaining[i] <= 0) return false;
        weight_above += boxes[trip[i]].weight;
    }
    return true;
}

long long seed_priority(int id, int mode) {
    const long long dist = distance_from_exit(id);
    const long long weight = boxes[id].weight;
    const long long durability = boxes[id].durability;
    if (mode == 0) return dist * 1000000000LL + weight * 100000LL + durability;
    if (mode == 1) return weight * 1000000000LL + dist * 1000000LL + durability;
    if (mode == 2) return (dist * 80LL + weight) * 10000000LL + durability;
    if (mode == 3) return durability * 100000LL + dist * 1000LL + weight;
    if (mode == 4) return dist * 1000000000LL + durability * 1000LL + weight;
    return dist * weight * 1000000LL + durability * 100LL + weight;
}

long long next_priority(int id, int saving, int mode) {
    const long long weight = boxes[id].weight;
    const long long durability = boxes[id].durability;
    const long long ratio = durability * 1000LL / weight;
    if (mode == 0) return saving * 10000000000LL - weight * 100000LL + durability;
    if (mode == 1) return saving * 10000000000LL + durability * 10000LL - weight;
    if (mode == 2) return saving * 10000000000LL + ratio * 100000LL - weight;
    if (mode == 3) return saving * 100000000LL - weight * 100000LL + durability * 100LL;
    if (mode == 4) return saving * 100000000LL + durability * 100LL - weight * 10000LL;
    if (mode == 5) return saving * 100000000LL + ratio * 100000LL + durability;
    if (mode == 6) return saving * 1000000LL + durability * 100LL - weight * 1000LL;
    return saving * 1000000LL - weight * 10000LL + ratio;
}

// Greedily creates safe trips.  A candidate is accepted only when all boxes
// would still survive an immediate shortest return to the exit.
Plan make_plan(int seed_mode, int next_mode) {
    const int count = static_cast<int>(boxes.size());
    vector<char> unused(count, true);
    unused[0] = false;
    int boxes_left = count - 1;
    Plan plan;

    while (boxes_left > 0) {
        int first = -1;
        long long best_seed = numeric_limits<long long>::min();
        for (int id = 1; id < count; ++id) {
            if (!unused[id]) continue;
            const long long value = seed_priority(id, seed_mode);
            if (value > best_seed) {
                best_seed = value;
                first = id;
            }
        }

        vector<int> trip{first};
        vector<long long> remaining{boxes[first].durability};
        unused[first] = false;
        --boxes_left;
        int current = first;

        while (true) {
            int chosen = -1;
            long long best_value = numeric_limits<long long>::min();

            for (int id = 1; id < count; ++id) {
                if (!unused[id]) continue;
                const int travel = distance_between(current, id);
                const int return_distance = distance_from_exit(id);
                const int saving = distance_from_exit(current) + return_distance - travel;
                if (saving <= 0) continue;

                bool safe = true;
                long long weight_above = 0;
                for (int i = static_cast<int>(trip.size()) - 1; i >= 0; --i) {
                    const long long damage =
                        weight_above * travel +
                        (weight_above + boxes[id].weight) * return_distance;
                    if (remaining[i] - damage <= 0) {
                        safe = false;
                        break;
                    }
                    weight_above += boxes[trip[i]].weight;
                }
                if (!safe) continue;

                const long long value = next_priority(id, saving, next_mode);
                if (value > best_value) {
                    best_value = value;
                    chosen = id;
                }
            }

            if (chosen == -1) break;

            const int travel = distance_between(current, chosen);
            long long weight_above = 0;
            for (int i = static_cast<int>(trip.size()) - 1; i >= 0; --i) {
                remaining[i] -= weight_above * travel;
                weight_above += boxes[trip[i]].weight;
            }
            trip.push_back(chosen);
            remaining.push_back(boxes[chosen].durability);
            unused[chosen] = false;
            --boxes_left;
            current = chosen;
        }

        plan.moves += trip_length(trip);
        plan.trips.push_back(move(trip));
    }
    return plan;
}

// The greedy construction already gives safe trips.  This small local search
// only accepts a swap/reversal when it shortens the route and stays safe.
void improve_one_trip(vector<int>& trip) {
    bool changed = true;
    while (changed) {
        changed = false;
        long long current_length = trip_length(trip);
        vector<int> best_trip = trip;
        long long best_length = current_length;
        const int size = static_cast<int>(trip.size());

        for (int left = 0; left < size; ++left) {
            for (int right = left + 1; right < size; ++right) {
                vector<int> candidate = trip;
                swap(candidate[left], candidate[right]);
                const long long length = trip_length(candidate);
                if (length < best_length && is_safe(candidate)) {
                    best_length = length;
                    best_trip = move(candidate);
                }

                candidate = trip;
                reverse(candidate.begin() + left, candidate.begin() + right + 1);
                const long long reversed_length = trip_length(candidate);
                if (reversed_length < best_length && is_safe(candidate)) {
                    best_length = reversed_length;
                    best_trip = move(candidate);
                }
            }
        }

        if (best_length < current_length) {
            trip = move(best_trip);
            changed = true;
        }
    }
}

// Move one box to another trip, or swap boxes between two trips.  This repairs
// boundaries made by the greedy construction.  Every trial is checked by the
// same complete durability simulation as the final answer.
void improve_between_trips(Plan& plan) {
    for (int iteration = 0; iteration < 50; ++iteration) {
        vector<long long> lengths(plan.trips.size());
        for (int i = 0; i < static_cast<int>(plan.trips.size()); ++i) {
            lengths[i] = trip_length(plan.trips[i]);
        }

        // type 1: relocate, type 2: swap
        int best_type = 0;
        int best_a = -1;
        int best_b = -1;
        int best_pos_a = -1;
        int best_pos_b = -1;
        long long best_gain = 0;

        const int trip_count = static_cast<int>(plan.trips.size());
        for (int a = 0; a < trip_count; ++a) {
            for (int pos_a = 0; pos_a < static_cast<int>(plan.trips[a].size()); ++pos_a) {
                vector<int> shortened = plan.trips[a];
                const int moved_box = shortened[pos_a];
                shortened.erase(shortened.begin() + pos_a);
                const long long shortened_length = trip_length(shortened);

                for (int b = 0; b < trip_count; ++b) {
                    if (a == b) continue;
                    for (int pos_b = 0; pos_b <= static_cast<int>(plan.trips[b].size()); ++pos_b) {
                        vector<int> extended = plan.trips[b];
                        extended.insert(extended.begin() + pos_b, moved_box);
                        const long long new_length = shortened_length + trip_length(extended);
                        const long long gain = lengths[a] + lengths[b] - new_length;
                        if (gain > best_gain && is_safe(extended)) {
                            best_gain = gain;
                            best_type = 1;
                            best_a = a;
                            best_b = b;
                            best_pos_a = pos_a;
                            best_pos_b = pos_b;
                        }
                    }
                }
            }
        }

        for (int a = 0; a < trip_count; ++a) {
            for (int b = a + 1; b < trip_count; ++b) {
                for (int pos_a = 0; pos_a < static_cast<int>(plan.trips[a].size()); ++pos_a) {
                    for (int pos_b = 0; pos_b < static_cast<int>(plan.trips[b].size()); ++pos_b) {
                        vector<int> changed_a = plan.trips[a];
                        vector<int> changed_b = plan.trips[b];
                        swap(changed_a[pos_a], changed_b[pos_b]);
                        const long long new_length = trip_length(changed_a) + trip_length(changed_b);
                        const long long gain = lengths[a] + lengths[b] - new_length;
                        if (gain > best_gain && is_safe(changed_a) && is_safe(changed_b)) {
                            best_gain = gain;
                            best_type = 2;
                            best_a = a;
                            best_b = b;
                            best_pos_a = pos_a;
                            best_pos_b = pos_b;
                        }
                    }
                }
            }
        }

        if (best_type == 0) break;
        if (best_type == 1) {
            const int moved_box = plan.trips[best_a][best_pos_a];
            plan.trips[best_a].erase(plan.trips[best_a].begin() + best_pos_a);
            plan.trips[best_b].insert(plan.trips[best_b].begin() + best_pos_b, moved_box);
            if (plan.trips[best_a].empty()) plan.trips.erase(plan.trips.begin() + best_a);
        } else {
            swap(plan.trips[best_a][best_pos_a], plan.trips[best_b][best_pos_b]);
        }
    }
}

void improve_plan(Plan& plan) {
    for (vector<int>& trip : plan.trips) improve_one_trip(trip);
    improve_between_trips(plan);
    for (vector<int>& trip : plan.trips) improve_one_trip(trip);

    // Sometimes two independently constructed trips can safely be joined.
    while (true) {
        int best_first = -1;
        int best_second = -1;
        long long best_gain = 0;
        vector<int> best_joined;

        const int trip_count = static_cast<int>(plan.trips.size());
        for (int first = 0; first < trip_count; ++first) {
            for (int second = 0; second < trip_count; ++second) {
                if (first == second) continue;
                vector<int> joined = plan.trips[first];
                joined.insert(joined.end(), plan.trips[second].begin(), plan.trips[second].end());
                const long long gain = trip_length(plan.trips[first]) +
                                       trip_length(plan.trips[second]) - trip_length(joined);
                if (gain > best_gain && is_safe(joined)) {
                    best_gain = gain;
                    best_first = first;
                    best_second = second;
                    best_joined = move(joined);
                }
            }
        }

        if (best_first == -1) break;
        plan.trips[best_first] = move(best_joined);
        plan.trips.erase(plan.trips.begin() + best_second);
        improve_one_trip(plan.trips[best_first > best_second ? best_first - 1 : best_first]);
    }

    improve_between_trips(plan);
    for (vector<int>& trip : plan.trips) improve_one_trip(trip);

    plan.moves = 0;
    for (const vector<int>& trip : plan.trips) plan.moves += trip_length(trip);
}

void add_repeat(string& answer, char operation, int count) {
    answer.append(static_cast<size_t>(count), operation);
}

void move_to_box(string& answer, int& row, int& col, const Box& target) {
    // Between two non-exit cells, this order never passes through (0, 0).
    if (row == 0) {
        if (target.row > row) add_repeat(answer, 'D', target.row - row);
        if (target.row < row) add_repeat(answer, 'U', row - target.row);
        if (target.col > col) add_repeat(answer, 'R', target.col - col);
        if (target.col < col) add_repeat(answer, 'L', col - target.col);
    } else {
        if (target.col > col) add_repeat(answer, 'R', target.col - col);
        if (target.col < col) add_repeat(answer, 'L', col - target.col);
        if (target.row > row) add_repeat(answer, 'D', target.row - row);
        if (target.row < row) add_repeat(answer, 'U', row - target.row);
    }
    row = target.row;
    col = target.col;
}

void return_to_exit(string& answer, int& row, int& col) {
    add_repeat(answer, 'L', col);
    col = 0;
    add_repeat(answer, 'U', row);
    row = 0;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cin >> grid_size;
    boxes.resize(grid_size * grid_size);
    for (int row = 0; row < grid_size; ++row) {
        for (int col = 0; col < grid_size; ++col) {
            boxes[row * grid_size + col].row = row;
            boxes[row * grid_size + col].col = col;
            cin >> boxes[row * grid_size + col].weight;
        }
    }
    for (int row = 0; row < grid_size; ++row) {
        for (int col = 0; col < grid_size; ++col) {
            cin >> boxes[row * grid_size + col].durability;
        }
    }

    Plan best_plan;
    best_plan.moves = numeric_limits<long long>::max();

#ifdef BASELINE
    for (int id = 1; id < static_cast<int>(boxes.size()); ++id) {
        best_plan.trips.push_back(vector<int>{id});
    }
    best_plan.moves = 0;
    for (const vector<int>& trip : best_plan.trips) best_plan.moves += trip_length(trip);
#else
    vector<Plan> candidate_plans;
    for (int seed_mode = 0; seed_mode < 6; ++seed_mode) {
        for (int next_mode = 0; next_mode < 8; ++next_mode) {
            Plan candidate = make_plan(seed_mode, next_mode);
            candidate_plans.push_back(move(candidate));
            sort(candidate_plans.begin(), candidate_plans.end(),
                 [](const Plan& left, const Plan& right) { return left.moves < right.moves; });
            if (candidate_plans.size() > 2) candidate_plans.resize(2);
        }
    }
    for (Plan& candidate : candidate_plans) {
        improve_plan(candidate);
        if (candidate.moves < best_plan.moves) best_plan = move(candidate);
    }
#endif

    string answer;
    answer.reserve(static_cast<size_t>(best_plan.moves + grid_size * grid_size));
    int row = 0;
    int col = 0;
    for (const vector<int>& trip : best_plan.trips) {
        for (int id : trip) {
            move_to_box(answer, row, col, boxes[id]);
            answer.push_back('1');
        }
        return_to_exit(answer, row, col);
    }

    for (char operation : answer) cout << operation << '\n';
    return 0;
}
