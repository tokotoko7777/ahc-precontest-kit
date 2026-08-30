#include <bits/stdc++.h>
using namespace std;

#ifndef AHC059_TIME_LIMIT
#define AHC059_TIME_LIMIT 1.85
#endif

#ifndef AHC059_CHAIN_TIME_LIMIT
#define AHC059_CHAIN_TIME_LIMIT 0.30
#endif

#ifndef AHC059_START_TEMPERATURE
#define AHC059_START_TEMPERATURE 0.2
#endif

#ifndef AHC059_END_TEMPERATURE
#define AHC059_END_TEMPERATURE 0.001
#endif

#ifndef AHC059_FAST_START_TEMPERATURE
#define AHC059_FAST_START_TEMPERATURE 1.0
#endif

#ifndef AHC059_FAST_END_TEMPERATURE
#define AHC059_FAST_END_TEMPERATURE 0.01
#endif

struct Position {
    int row;
    int col;
};

struct Event {
    int id;
    int endpoint;
};

int distance_between(Position a, Position b) {
    return abs(a.row - b.row) + abs(a.col - b.col);
}

struct Random {
    uint64_t state = 0x123456789abcdef0ULL;

    uint64_t next() {
        state ^= state << 7;
        state ^= state >> 9;
        return state;
    }

    int next_int(int upper) {
        return static_cast<int>(next() % static_cast<uint64_t>(upper));
    }

    double next_double() {
        return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
    }
};

struct Timer {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    double seconds() const {
        return chrono::duration<double>(chrono::steady_clock::now() - start).count();
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Timer timer;
    int N;
    cin >> N;
    const int pair_count = N * N / 2;

    vector<array<Position, 2>> card(pair_count);
    vector<int> appearances(pair_count, 0);
    for (int row = 0; row < N; ++row) {
        for (int col = 0; col < N; ++col) {
            int value;
            cin >> value;
            card[value][appearances[value]++] = {row, col};
        }
    }

    auto route_cost = [&](const vector<int>& order, vector<int>* answer_direction) {
        const int INF = 1 << 29;
        array<array<int, 2>, 200> dp;
        array<array<int, 2>, 200> previous;
        for (int index = 0; index < pair_count; ++index) {
            dp[index] = {INF, INF};
            previous[index] = {-1, -1};
        }
        const Position start{0, 0};

        for (int direction = 0; direction < 2; ++direction) {
            const int id = order[0];
            dp[0][direction] = distance_between(start, card[id][direction]);
        }

        for (int index = 1; index < pair_count; ++index) {
            const int id = order[index];
            const int previous_id = order[index - 1];
            for (int direction = 0; direction < 2; ++direction) {
                for (int previous_direction = 0; previous_direction < 2;
                     ++previous_direction) {
                    const int candidate = dp[index - 1][previous_direction]
                        + distance_between(
                            card[previous_id][previous_direction], card[id][direction]
                        )
                        + distance_between(
                            card[id][1 - direction],
                            card[previous_id][1 - previous_direction]
                        );
                    if (candidate < dp[index][direction]) {
                        dp[index][direction] = candidate;
                        previous[index][direction] = previous_direction;
                    }
                }
            }
        }

        const int last_id = order[pair_count - 1];
        const int last_inside = distance_between(card[last_id][0], card[last_id][1]);
        int direction = (dp[pair_count - 1][0] <= dp[pair_count - 1][1] ? 0 : 1);
        const int result = dp[pair_count - 1][direction] + last_inside;
        if (answer_direction != nullptr) {
            answer_direction->assign(pair_count, 0);
            for (int index = pair_count - 1; index >= 0; --index) {
                (*answer_direction)[index] = direction;
                direction = previous[index][direction];
            }
        }
        return result;
    };

    // Make one nested chain.  Opening endpoints are visited forward, while
    // matching endpoints are visited in reverse order.
    // Nearest-neighbor is sensitive to its first pair.  All 400 choices of
    // first pair and orientation are cheap enough to try.
    vector<int> best_order;
    int best_cost = 1 << 29;
    for (int first_id = 0; first_id < pair_count; ++first_id) {
        for (int first_direction = 0; first_direction < 2; ++first_direction) {
            vector<int> candidate_order;
            candidate_order.reserve(pair_count);
            vector<char> used(pair_count, false);
            used[first_id] = true;
            candidate_order.push_back(first_id);
            Position current_entry = card[first_id][first_direction];
            Position current_exit = card[first_id][1 - first_direction];

            for (int step = 1; step < pair_count; ++step) {
                int next_id = -1;
                int next_direction = -1;
                int best_distance = 1 << 29;
                for (int id = 0; id < pair_count; ++id) {
                    if (used[id]) continue;
                    for (int direction = 0; direction < 2; ++direction) {
                        const int d = distance_between(current_entry, card[id][direction])
                                    + distance_between(
                                          card[id][1 - direction], current_exit
                                      );
                        if (d < best_distance) {
                            best_distance = d;
                            next_id = id;
                            next_direction = direction;
                        }
                    }
                }
                used[next_id] = true;
                candidate_order.push_back(next_id);
                current_entry = card[next_id][next_direction];
                current_exit = card[next_id][1 - next_direction];
            }

            const int candidate_cost = route_cost(candidate_order, nullptr);
            if (candidate_cost < best_cost) {
                best_cost = candidate_cost;
                best_order = candidate_order;
            }
        }
    }

    vector<int> current_order = best_order;
    int current_cost = best_cost;
    Random random;

    long long iterations = 0;
    double temperature = AHC059_START_TEMPERATURE;
    const double chain_deadline = min(
        static_cast<double>(AHC059_CHAIN_TIME_LIMIT),
        static_cast<double>(AHC059_TIME_LIMIT)
    );
    while (true) {
        if ((iterations & 255LL) == 0) {
            const double elapsed = timer.seconds();
            if (elapsed >= chain_deadline) break;
            const double progress = min(1.0, elapsed / chain_deadline);
            temperature = AHC059_START_TEMPERATURE
                        * pow(AHC059_END_TEMPERATURE / AHC059_START_TEMPERATURE,
                              progress);
        }
        ++iterations;

        const int operation = random.next_int(3);
        int left = random.next_int(pair_count);
        int right = random.next_int(pair_count);
        if (left == right) continue;
        if (left > right) swap(left, right);

        if (operation == 0) {
            swap(current_order[left], current_order[right]);
        } else if (operation == 1) {
            reverse(current_order.begin() + left, current_order.begin() + right + 1);
        } else {
            const int value = current_order[right];
            current_order.erase(current_order.begin() + right);
            current_order.insert(current_order.begin() + left, value);
        }

        const int next_cost = route_cost(current_order, nullptr);
        const int difference = next_cost - current_cost;
        const bool accept = difference <= 0
                         || random.next_double() < exp(-difference / temperature);
        if (accept) {
            current_cost = next_cost;
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best_order = current_order;
            }
        } else if (operation == 0) {
            swap(current_order[left], current_order[right]);
        } else if (operation == 1) {
            reverse(current_order.begin() + left, current_order.begin() + right + 1);
        } else {
            const int value = current_order[left];
            current_order.erase(current_order.begin() + left);
            current_order.insert(current_order.begin() + right, value);
        }
    }

    vector<int> direction;
    route_cost(best_order, &direction);

#ifdef AHC059_DEBUG
    cerr << "slow_iterations=" << iterations << " moves=" << best_cost << '\n';
#endif

    // In the second phase, orientations are explicit.  A flip, swap, or
    // interval reverse changes only a few boundary terms, so its score
    // difference can be calculated without scanning all 200 pairs.
    current_order = best_order;
    current_cost = best_cost;
    vector<int> best_direction = direction;

    auto component_cost = [&](int component) {
        if (component == -1) {
            return distance_between(
                {0, 0}, card[current_order[0]][direction[0]]
            );
        }
        if (component == pair_count - 1) {
            const int id = current_order[pair_count - 1];
            return distance_between(card[id][0], card[id][1]);
        }
        const int left_id = current_order[component];
        const int right_id = current_order[component + 1];
        return distance_between(
                   card[left_id][direction[component]],
                   card[right_id][direction[component + 1]]
               )
             + distance_between(
                   card[left_id][1 - direction[component]],
                   card[right_id][1 - direction[component + 1]]
               );
    };

    temperature = AHC059_FAST_START_TEMPERATURE;
    long long fast_iterations = 0;

    while (true) {
        if ((fast_iterations & 1023LL) == 0) {
            const double elapsed = timer.seconds();
            if (elapsed >= AHC059_TIME_LIMIT) break;
            const double progress = max(0.0, min(
                1.0,
                (elapsed - chain_deadline)
                    / (AHC059_TIME_LIMIT - chain_deadline)
            ));
            temperature = AHC059_FAST_START_TEMPERATURE
                        * pow(AHC059_FAST_END_TEMPERATURE
                                  / AHC059_FAST_START_TEMPERATURE,
                              progress);
        }
        ++fast_iterations;

        if ((fast_iterations & ((1LL << 18) - 1)) == 0) {
            vector<int> exact_direction;
            const int exact_cost = route_cost(current_order, &exact_direction);
            if (exact_cost < current_cost) {
                current_cost = exact_cost;
                direction.swap(exact_direction);
                if (current_cost < best_cost) {
                    best_cost = current_cost;
                    best_order = current_order;
                    best_direction = direction;
                }
            }
        }

        const int operation = random.next_int(3);
        int left = random.next_int(pair_count);
        int right = random.next_int(pair_count);
        if (operation != 0) {
            if (left > right) swap(left, right);
            if (left == right) continue;
        }

        array<int, 4> affected{};
        int affected_count = 0;
        auto add_component = [&](int component) {
            if (component < -1 || component >= pair_count) return;
            for (int k = 0; k < affected_count; ++k) {
                if (affected[k] == component) return;
            }
            affected[affected_count++] = component;
        };

        if (operation == 0) {
            add_component(left - 1);
            add_component(left);
        } else if (operation == 1) {
            add_component(left - 1);
            add_component(left);
            add_component(right - 1);
            add_component(right);
        } else {
            add_component(left - 1);
            add_component(right);
        }

        int old_part = 0;
        for (int k = 0; k < affected_count; ++k) {
            old_part += component_cost(affected[k]);
        }

        if (operation == 0) {
            direction[left] ^= 1;
        } else if (operation == 1) {
            swap(current_order[left], current_order[right]);
            swap(direction[left], direction[right]);
        } else {
            reverse(current_order.begin() + left, current_order.begin() + right + 1);
            reverse(direction.begin() + left, direction.begin() + right + 1);
        }

        int new_part = 0;
        for (int k = 0; k < affected_count; ++k) {
            new_part += component_cost(affected[k]);
        }

        const int difference = new_part - old_part;
        const bool accept = difference <= 0
                         || random.next_double() < exp(-difference / temperature);
        if (accept) {
            current_cost += difference;
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best_order = current_order;
                best_direction = direction;
            }
        } else if (operation == 0) {
            direction[left] ^= 1;
        } else if (operation == 1) {
            swap(current_order[left], current_order[right]);
            swap(direction[left], direction[right]);
        } else {
            reverse(current_order.begin() + left, current_order.begin() + right + 1);
            reverse(direction.begin() + left, direction.begin() + right + 1);
        }
    }

    // One final 2-state DP can only improve the orientations of the best order.
    vector<int> optimized_direction;
    const int optimized_cost = route_cost(best_order, &optimized_direction);
    if (optimized_cost <= best_cost) {
        best_cost = optimized_cost;
        best_direction = optimized_direction;
    }

#ifdef AHC059_DEBUG
    cerr << "fast_iterations=" << fast_iterations << " moves=" << best_cost << '\n';
#endif

    vector<Event> best_events;
    best_events.reserve(N * N);
    for (int index = 0; index < pair_count; ++index) {
        best_events.push_back({best_order[index], best_direction[index]});
    }
    for (int index = pair_count - 1; index >= 0; --index) {
        best_events.push_back({best_order[index], 1 - best_direction[index]});
    }

    Position position{0, 0};
    auto move_to = [&](Position goal) {
        while (position.row < goal.row) {
            cout << "D\n";
            ++position.row;
        }
        while (position.row > goal.row) {
            cout << "U\n";
            --position.row;
        }
        while (position.col < goal.col) {
            cout << "R\n";
            ++position.col;
        }
        while (position.col > goal.col) {
            cout << "L\n";
            --position.col;
        }
    };

    for (Event event : best_events) {
        move_to(card[event.id][event.endpoint]);
        cout << "Z\n";
    }
}
