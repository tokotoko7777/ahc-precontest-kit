// AHC064: move contiguous blocks of cars with a beam search.
// This file is self-contained: paste it into AtCoder as main.cpp.
#pragma GCC optimize("O3,unroll-loops")

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <unordered_set>
#include <vector>

using namespace std;
using namespace std::chrono;

constexpr int R = 10;
constexpr int N = 100;
constexpr int DEP_CAP = 15;
constexpr int SID_CAP = 20;
constexpr int MAX_TURNS = 100;

constexpr int BEAM_WIDTH = 100;
constexpr int NORMAL_CHILDREN = 10;
constexpr int FIRST_CHILDREN = 80;
constexpr double BEAM_TIME_LIMIT_SEC = 1.85;

constexpr int REACH = 500;
constexpr int ONE_SIDE_EXPOSED = 1400;
constexpr int UNCONNECTED = 1500;
constexpr int V_SAME_FORWARD = 60;
constexpr int V_REVERSE = 800;
constexpr int V_OTHER = 30;
constexpr int TOP_DEP_V = 60;
constexpr int TOP_SID_V = 100;
constexpr int H_WEIGHT = 3;
constexpr int REVERSE_EXPOSED_BONUS = 120;
constexpr int SAME_LINE_KIND_BONUS = 80;
constexpr int ANCHOR_LINE_BONUS = 120;

struct Action {
    int type = 0;
    int i = 0;
    int j = 0;
    int k = 0;
};

struct TurnMove {
    vector<Action> actions;
};

struct Location {
    int kind = -1;  // 0: departure, 1: siding
    int line = -1;
    int pos = -1;
};

struct State {
    array<vector<int>, R> dep;
    array<vector<int>, R> sid;
    array<Location, N> locs;
    array<int, R> top_cost {};
    array<array<int, R - 1>, R> pair_cost {};
    long long penalty = numeric_limits<long long>::max();
    int turn = 0;
    vector<TurnMove> operations;
};

struct Candidate {
    Action action;
    long long raw_gain = numeric_limits<long long>::min();
    double weighted_gain = -1e100;
};

struct RawCandidate {
    Action action;
    long long raw_gain = numeric_limits<long long>::min();
};

struct XorShift {
    uint64_t x = 88172645463325252ULL;

    uint64_t next_u64() {
        x ^= x << 7;
        x ^= x >> 9;
        return x;
    }

    double next01() {
        return static_cast<double>(next_u64() >> 11) * (1.0 / 9007199254740992.0);
    }

    double multiplier() {
        return 0.3 + 0.7 * next01();
    }
};

// `static` keeps helper functions local to this one file without a custom namespace.
static uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static uint64_t hash_state(const State& s) {
    uint64_t h = 0x9d39247e33776d41ULL;
    for (int i = 0; i < R; ++i) {
        h = splitmix64(h ^ static_cast<uint64_t>(i * 1009 + s.dep[i].size() * 17 + s.sid[i].size() * 31));
        for (const int car : s.dep[i]) {
            h = splitmix64(h ^ static_cast<uint64_t>(car + 1 + i * 127));
        }
        for (const int car : s.sid[i]) {
            h = splitmix64(h ^ static_cast<uint64_t>(car + 101 + i * 257));
        }
    }
    return h;
}

static array<Location, N> build_locations(const State& s) {
    array<Location, N> locs;
    for (int i = 0; i < R; ++i) {
        for (int p = 0; p < static_cast<int>(s.dep[i].size()); ++p) {
            locs[s.dep[i][p]] = Location{0, i, p};
        }
        for (int p = 0; p < static_cast<int>(s.sid[i].size()); ++p) {
            locs[s.sid[i][p]] = Location{1, i, p};
        }
    }
    return locs;
}

static bool same_line_kind(const Location& a, const Location& b) {
    return a.kind == b.kind && a.line == b.line;
}

static bool adjacent_connected(const Location& a, const Location& b) {
    return same_line_kind(a, b) && a.pos + 1 == b.pos;
}

static bool sid_head(const Location& loc) {
    return loc.kind == 1 && loc.pos == 0;
}

static bool dep_tail_with_sizes(const Location& loc, const array<int, R>& dep_size) {
    return loc.kind == 0 && loc.pos + 1 == dep_size[loc.line];
}

static bool dep_head(const Location& loc) {
    return loc.kind == 0 && loc.pos == 0;
}

static bool sid_tail_with_sizes(const Location& loc, const array<int, R>& sid_size) {
    return loc.kind == 1 && loc.pos + 1 == sid_size[loc.line];
}

// Cost of breaking one target adjacency: car a should be immediately before car b.
static int pair_penalty_with_sizes(const array<int, R>& dep_size, const array<int, R>& sid_size, const Location& a, const Location& b) {
    if (adjacent_connected(a, b)) {
        return 0;
    }

    const bool left_exposed = dep_tail_with_sizes(a, dep_size);
    const bool right_exposed = sid_head(b);
    const bool reverse_exposed = sid_tail_with_sizes(a, sid_size) || dep_head(b);
    int penalty = 0;
    if (left_exposed && right_exposed) {
        penalty += REACH;
    } else if (left_exposed || right_exposed) {
        penalty += ONE_SIDE_EXPOSED;
    } else {
        penalty += UNCONNECTED;
    }
    if (reverse_exposed) {
        penalty -= REVERSE_EXPOSED_BONUS;
    }

    if (same_line_kind(a, b)) {
        penalty += (a.pos < b.pos ? V_SAME_FORWARD : V_REVERSE);
    } else if (a.kind == b.kind || a.line == b.line) {
        penalty += V_OTHER;
        penalty -= SAME_LINE_KIND_BONUS;
    } else {
        penalty += V_OTHER;
    }
    penalty += abs(a.line - b.line) * H_WEIGHT;
    return penalty;
}

static int top_penalty_with_sizes(const array<int, R>& dep_size, const array<Location, N>& locs, const int r) {
    const Location& loc = locs[10 * r];
    if (loc.kind == 0 && loc.line == r && loc.pos == 0) {
        return 0;
    }

    const bool anchor_exposed = dep_size[r] == 0;
    const bool car_exposed = sid_head(loc);
    int penalty = 0;
    if (anchor_exposed && car_exposed) {
        penalty += REACH;
    } else if (anchor_exposed || car_exposed) {
        penalty += ONE_SIDE_EXPOSED;
    } else {
        penalty += UNCONNECTED;
    }

    penalty += (loc.kind == 0 ? TOP_DEP_V : TOP_SID_V);
    if (loc.line == r) {
        penalty -= ANCHOR_LINE_BONUS;
    }
    penalty += abs(r - loc.line) * H_WEIGHT;
    return penalty;
}

static void refresh_state(State& s) {
    s.locs = build_locations(s);
    array<int, R> dep_size;
    array<int, R> sid_size;
    for (int i = 0; i < R; ++i) {
        dep_size[i] = static_cast<int>(s.dep[i].size());
        sid_size[i] = static_cast<int>(s.sid[i].size());
    }

    s.penalty = 0;
    for (int r = 0; r < R; ++r) {
        s.top_cost[r] = top_penalty_with_sizes(dep_size, s.locs, r);
        s.penalty += s.top_cost[r];
        for (int c = 0; c + 1 < R; ++c) {
            const int a = 10 * r + c;
            s.pair_cost[r][c] = pair_penalty_with_sizes(dep_size, sid_size, s.locs[a], s.locs[a + 1]);
            s.penalty += s.pair_cost[r][c];
        }
    }
}

static void apply_action_to_board(State& s, const Action& action) {
    if (action.type == 0) {
        vector<int>& from = s.dep[action.i];
        vector<int>& to = s.sid[action.j];
        vector<int> block(from.end() - action.k, from.end());
        from.erase(from.end() - action.k, from.end());
        to.insert(to.begin(), block.begin(), block.end());
    } else {
        vector<int>& from = s.sid[action.j];
        vector<int>& to = s.dep[action.i];
        vector<int> block(from.begin(), from.begin() + action.k);
        from.erase(from.begin(), from.begin() + action.k);
        to.insert(to.end(), block.begin(), block.end());
    }
}

static void apply_turn(State& s, const TurnMove& turn) {
    for (const Action& action : turn.actions) {
        apply_action_to_board(s, action);
    }
    if (!turn.actions.empty()) {
        s.operations.push_back(turn);
        ++s.turn;
        refresh_state(s);
    }
}

static void mark_car_components(const int car, array<bool, R>& top_used, array<array<bool, R - 1>, R>& pair_used) {
    const int r = car / 10;
    const int c = car % 10;
    if (c == 0) {
        top_used[r] = true;
    }
    if (c > 0) {
        pair_used[r][c - 1] = true;
    }
    if (c + 1 < R) {
        pair_used[r][c] = true;
    }
}

static void mark_line_components(const vector<int>& line, array<bool, R>& top_used, array<array<bool, R - 1>, R>& pair_used) {
    for (const int car : line) {
        mark_car_components(car, top_used, pair_used);
    }
}

static long long single_action_gain(const State& s, const Action& action) {
    array<Location, N> next_locs = s.locs;
    array<int, R> next_dep_size;
    array<int, R> next_sid_size;
    for (int i = 0; i < R; ++i) {
        next_dep_size[i] = static_cast<int>(s.dep[i].size());
        next_sid_size[i] = static_cast<int>(s.sid[i].size());
    }

    array<bool, R> top_used {};
    array<array<bool, R - 1>, R> pair_used {};
    for (int r = 0; r < R; ++r) {
        pair_used[r].fill(false);
    }

    top_used[action.i] = true;
    mark_line_components(s.dep[action.i], top_used, pair_used);
    mark_line_components(s.sid[action.j], top_used, pair_used);

    const int dep_size = static_cast<int>(s.dep[action.i].size());
    const int sid_size = static_cast<int>(s.sid[action.j].size());
    if (action.type == 0) {
        next_dep_size[action.i] -= action.k;
        next_sid_size[action.j] += action.k;
        const int offset = dep_size - action.k;
        for (int p = offset; p < dep_size; ++p) {
            next_locs[s.dep[action.i][p]] = Location{1, action.j, p - offset};
        }
        for (int p = 0; p < sid_size; ++p) {
            next_locs[s.sid[action.j][p]] = Location{1, action.j, p + action.k};
        }
    } else {
        next_dep_size[action.i] += action.k;
        next_sid_size[action.j] -= action.k;
        for (int p = 0; p < action.k; ++p) {
            next_locs[s.sid[action.j][p]] = Location{0, action.i, dep_size + p};
        }
        for (int p = action.k; p < sid_size; ++p) {
            next_locs[s.sid[action.j][p]] = Location{1, action.j, p - action.k};
        }
    }

    long long old_part = 0;
    long long new_part = 0;
    for (int r = 0; r < R; ++r) {
        if (top_used[r]) {
            old_part += s.top_cost[r];
            new_part += top_penalty_with_sizes(next_dep_size, next_locs, r);
        }
        for (int c = 0; c + 1 < R; ++c) {
            if (!pair_used[r][c]) {
                continue;
            }
            old_part += s.pair_cost[r][c];
            const int a = 10 * r + c;
            new_part += pair_penalty_with_sizes(next_dep_size, next_sid_size, next_locs[a], next_locs[a + 1]);
        }
    }
    return old_part - new_part;
}

static void update_best_candidate(Candidate& best, const Action& action, const long long raw_gain, const double weighted_gain) {
    if (weighted_gain > best.weighted_gain ||
        (weighted_gain == best.weighted_gain && raw_gain > best.raw_gain) ||
        (weighted_gain == best.weighted_gain && raw_gain == best.raw_gain && action.k > best.action.k)) {
        best.action = action;
        best.raw_gain = raw_gain;
        best.weighted_gain = weighted_gain;
    }
}

using RawCandidateGrid = array<array<vector<RawCandidate>, R>, R>;

// Try every legal contiguous block length for every departure/siding pair.
static RawCandidateGrid build_raw_pair_candidates(const State& s) {
    RawCandidateGrid raw;
    for (int i = 0; i < R; ++i) {
        for (int j = 0; j < R; ++j) {
            const int dep_size = static_cast<int>(s.dep[i].size());
            const int sid_size = static_cast<int>(s.sid[j].size());

            const int max_type0 = min(dep_size, SID_CAP - sid_size);
            for (int k = 1; k <= max_type0; ++k) {
                const Action action{0, i, j, k};
                const long long raw_gain = single_action_gain(s, action);
                raw[i][j].push_back(RawCandidate{action, raw_gain});
            }

            const int max_type1 = min(sid_size, DEP_CAP - dep_size);
            for (int k = 1; k <= max_type1; ++k) {
                const Action action{1, i, j, k};
                const long long raw_gain = single_action_gain(s, action);
                raw[i][j].push_back(RawCandidate{action, raw_gain});
            }
        }
    }
    return raw;
}

static array<array<Candidate, R>, R> choose_weighted_pair_candidates(const RawCandidateGrid& raw, XorShift& rng) {
    array<array<Candidate, R>, R> best;
    for (int i = 0; i < R; ++i) {
        for (int j = 0; j < R; ++j) {
            for (const RawCandidate& cand : raw[i][j]) {
                const double weighted_gain = static_cast<double>(cand.raw_gain) * rng.multiplier();
                update_best_candidate(best[i][j], cand.action, cand.raw_gain, weighted_gain);
            }
        }
    }
    return best;
}

// Increasing both i and j makes all moves in this turn non-crossing.
static TurnMove select_turn_by_dp(const RawCandidateGrid& raw, XorShift& rng) {
    const auto cand = choose_weighted_pair_candidates(raw, rng);
    constexpr double NEG_INF = -1e100;

    array<array<double, R + 1>, R + 1> dp;
    array<array<int, R + 1>, R + 1> prev_last;
    array<array<Action, R + 1>, R + 1> chosen;
    array<array<bool, R + 1>, R + 1> took;
    for (int i = 0; i <= R; ++i) {
        for (int j = 0; j <= R; ++j) {
            dp[i][j] = NEG_INF;
            prev_last[i][j] = -1;
            took[i][j] = false;
        }
    }
    dp[0][0] = 0.0;  // last_j = -1 is encoded as 0.

    for (int i = 0; i < R; ++i) {
        for (int last_idx = 0; last_idx <= R; ++last_idx) {
            if (dp[i][last_idx] <= NEG_INF / 2) {
                continue;
            }
            if (dp[i][last_idx] > dp[i + 1][last_idx]) {
                dp[i + 1][last_idx] = dp[i][last_idx];
                prev_last[i + 1][last_idx] = last_idx;
                took[i + 1][last_idx] = false;
            }

            const int last_j = last_idx - 1;
            for (int j = last_j + 1; j < R; ++j) {
                if (cand[i][j].raw_gain == numeric_limits<long long>::min()) {
                    continue;
                }
                const double value = dp[i][last_idx] + cand[i][j].weighted_gain;
                const int next_idx = j + 1;
                if (value > dp[i + 1][next_idx]) {
                    dp[i + 1][next_idx] = value;
                    prev_last[i + 1][next_idx] = last_idx;
                    took[i + 1][next_idx] = true;
                    chosen[i + 1][next_idx] = cand[i][j].action;
                }
            }
        }
    }

    int best_last = 0;
    for (int last_idx = 1; last_idx <= R; ++last_idx) {
        if (dp[R][last_idx] > dp[R][best_last]) {
            best_last = last_idx;
        }
    }

    TurnMove turn;
    if (dp[R][best_last] <= 0.0) {
        Candidate best_single;
        for (int i = 0; i < R; ++i) {
            for (int j = 0; j < R; ++j) {
                update_best_candidate(best_single, cand[i][j].action, cand[i][j].raw_gain, cand[i][j].weighted_gain);
            }
        }
        if (best_single.raw_gain != numeric_limits<long long>::min()) {
            turn.actions.push_back(best_single.action);
        }
        return turn;
    }

    int cur_last = best_last;
    for (int i = R; i >= 1; --i) {
        if (took[i][cur_last]) {
            turn.actions.push_back(chosen[i][cur_last]);
        }
        cur_last = prev_last[i][cur_last];
    }
    reverse(turn.actions.begin(), turn.actions.end());
    return turn;
}

static bool better_state(const State& a, const State& b) {
    if (a.penalty != b.penalty) {
        return a.penalty < b.penalty;
    }
    return a.turn < b.turn;
}

// Keep the best 100 different arrangements after each turn.
static State beam_search(State initial, const steady_clock::time_point deadline) {
    XorShift rng;
    vector<State> beam{initial};
    State best_seen = initial;

    while (!beam.empty() && steady_clock::now() < deadline) {
        vector<State> next;
        const int child_count = (beam.front().turn == 0 ? FIRST_CHILDREN : NORMAL_CHILDREN);

        for (const State& parent : beam) {
            if (steady_clock::now() >= deadline) {
                break;
            }
            if (parent.turn >= MAX_TURNS || parent.penalty == 0) {
                if (better_state(parent, best_seen)) {
                    best_seen = parent;
                }
                if (parent.penalty == 0) {
                    return parent;
                }
                continue;
            }

            const RawCandidateGrid raw_candidates = build_raw_pair_candidates(parent);
            unordered_set<uint64_t> local_seen;
            int added_children = 0;
            const int attempt_limit = child_count * 4;
            for (int rep = 0; rep < attempt_limit && added_children < child_count && steady_clock::now() < deadline; ++rep) {
                TurnMove turn = select_turn_by_dp(raw_candidates, rng);
                if (turn.actions.empty()) {
                    continue;
                }
                State child = parent;
                apply_turn(child, turn);
                const uint64_t h = hash_state(child);
                if (!local_seen.insert(h).second) {
                    continue;
                }
                if (child.penalty == 0) {
                    return child;
                }
                if (better_state(child, best_seen)) {
                    best_seen = child;
                }
                next.push_back(std::move(child));
                ++added_children;
            }
        }

        if (next.empty()) {
            break;
        }

        sort(next.begin(), next.end(), [](const State& a, const State& b) {
            return better_state(a, b);
        });

        unordered_set<uint64_t> seen;
        vector<State> trimmed;
        trimmed.reserve(BEAM_WIDTH);
        for (State& s : next) {
            const uint64_t h = hash_state(s);
            if (!seen.insert(h).second) {
                continue;
            }
            trimmed.push_back(std::move(s));
            if (static_cast<int>(trimmed.size()) >= BEAM_WIDTH) {
                break;
            }
        }
        beam = std::move(trimmed);
    }

    if (beam.empty()) {
        return best_seen;
    }

    State answer = beam.front();
    for (const State& s : beam) {
        if (s.penalty < answer.penalty || (s.penalty == answer.penalty && s.turn > answer.turn)) {
            answer = s;
        }
    }
    return answer;
}

static State read_input() {
    int r_in;
    cin >> r_in;
    (void)r_in;

    State s;
    for (int r = 0; r < R; ++r) {
        s.dep[r].resize(10);
        for (int c = 0; c < 10; ++c) {
            cin >> s.dep[r][c];
        }
    }
    refresh_state(s);
    return s;
}

static void output_answer(const State& s) {
    cout << s.operations.size() << '\n';
    for (const TurnMove& turn : s.operations) {
        cout << turn.actions.size() << '\n';
        for (const Action& action : turn.actions) {
            cout << action.type << ' ' << action.i << ' ' << action.j << ' ' << action.k << '\n';
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    const auto start = steady_clock::now();
    State initial = read_input();

#ifdef SIMPLE_BASELINE
    // Legal zero-turn baseline: keep the initial arrangement unchanged.
    cout << 0 << '\n';
    return 0;
#endif

    const auto beam_deadline = start + duration_cast<steady_clock::duration>(duration<double>(BEAM_TIME_LIMIT_SEC));
    State best = beam_search(std::move(initial), beam_deadline);
    output_answer(best);
    return 0;
}
