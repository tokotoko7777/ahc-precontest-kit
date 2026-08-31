#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,lzcnt,popcnt")
using namespace std;

// AHC066 standalone solver. Submit this one file only.
// The README gives a beginner-friendly reading order for the larger search.
#if defined(__GNUC__)
#define AHC066_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define AHC066_ALWAYS_INLINE inline
#endif

constexpr int INF = 1'000'000'000;  // 到達不能や十分悪い評価を表す大きな値。
constexpr int START_DIR = 0;  // 初期向き。0は右向き。
constexpr int MAX_N = 20;  // 問題制約上の盤面サイズ上限。
constexpr int MAX_M = 2 * MAX_N;  // 問題制約上のボール数上限。

constexpr int TOTAL_TIME_LIMIT_MILLIS = 1950;  // ソルバ全体の実行時間上限(ms)。
constexpr int REGISTER_BUILD_HELPER_MAX_LEN = 24;  // マクロ登録時に試す補助マクロの最大長。
constexpr int REGISTER_BUILD_HELPER_MIN_SAVE = 1;  // 補助マクロ採用に必要な最小短縮手数。
constexpr int PRE_REGISTER_MOVE_MAX_STEPS = 5;  // マクロ登録前に許す初期移動の最大手数。
constexpr int MATERIALIZE_TIME_RESERVE_MILLIS = 30;  // 最終的な経路復元のために残す時間(ms)。
constexpr int INTERNAL_MACRO_SEARCH_MILLIS = 30;  // 1回の固定順序マクロ焼きなましに使う時間(ms)。
constexpr int INTERNAL_MACRO_ORDER_MILLIS = 5; // マクロ決定後の順序焼きなましに使う時間(ms)。
constexpr int FINAL_ORDER_REFINE_MILLIS = 100;  // 最後にbest候補だけ順序を長めに焼き直す時間(ms)。
constexpr int INITIAL_ORDER_SEARCH_MILLIS = 20;  // 初期順序候補を1本作る時間(ms)。
constexpr int INTERNAL_MACRO_MIN_LEN = 7;  // 生成マクロとして評価する最小長。
constexpr int INTERNAL_MACRO_HARD_MAX_LEN = 30;  // 環境変数指定時以外は使わない固定上限の旧既定値。
constexpr int MAIN3_ALTERNATING_RESTART_LIMIT = 1'000'000;  // 交互探索の最大リスタート回数。
constexpr int ORDER_T_LIMIT_MARGIN = 10;  // T制限ぎりぎりの順序を避けるための余裕手数。
constexpr char INITIAL_ORDER_SEED_MACRO[] = "FFFFFF";  // 初期順序候補を作るための種マクロ。

constexpr array<int, 4> DR = {0, 1, 0, -1};
constexpr array<int, 4> DC = {1, 0, -1, 0};

using IntDir4 = array<int, 4>;
using IntMat4 = array<IntDir4, 4>;
using LongDir4 = array<long long, 4>;
using LongMat4 = array<LongDir4, 4>;

struct Point {
    int r = 0;
    int c = 0;
};

struct Task {
    Point ball;
    Point basket;
};

struct MotionPath {
    string ops;
    int end_state = 0;
};

struct MotionEval {
    int output_ops = INF;
    int basic_used = INF;
    int end_state = 0;
};

struct OutputCandidate {
    string macro;
    string ops;
    string registration_prefix;
    vector<int> order;
    int solved = 0;
    int output_ops = 0;
    long long basic_used = 0;
    long long registration_basic_used = 0;
    int registration_initial_state = 0;
    bool evaluation_complete = true;
};

struct RegistrationPlan {
    string prefix;
    int initial_state = 0;
    int output_ops = 0;
    long long basic_used = 0;
};

struct PreRegisterStart {
    string ops;
    int state = 0;
};

struct Solver {
    using Clock = chrono::steady_clock;

    struct RegistrationEncoding {
        bool use_helper = false;
        string helper;
        string encoded;
        int output_ops = 0;
        long long basic_used = 0;
    };

    struct InternalMacroBlock {
        int forward_count = 1;
        int rotation = 1;  // 0:L, 1:R, 2:LL
    };

    struct MacroSearchResult {
        string macro;
        vector<int> macro_effect;
        OutputCandidate fixed_candidate;
        long long score = static_cast<long long>(INF) * INF;
        int evaluated_macros = 0;
        int duplicate_skips = 0;
        int iterations = 0;
        int accepted = 0;
        int accepted_worse = 0;
        int improved = 0;
        double elapsed_ms = 0.0;
        bool has_fixed_candidate = false;
    };

    int n = 0;
    int m = 0;
    int t_limit = 0;
    int cells = 0;
    int states = 0;
    vector<string> v_wall;
    vector<string> h_wall;
    vector<Task> tasks;
    vector<vector<int>> cell_neighbors;
    vector<vector<int>> cell_dist;
    vector<int> unique_basket_cells;
    vector<int> task_ball_cells;
    vector<int> task_basket_cells;
    vector<vector<int>> task_ids_by_basket_cell;
    vector<vector<int>> task_ids_by_ball_cell;
    vector<array<int, 3>> motion_next;
    vector<array<int, 3>> reverse_motion_prev;
    Clock::time_point deadline = Clock::time_point::max();
    mutable int bfs_epoch = 0;
    mutable vector<int> bfs_seen_epoch;
    mutable vector<int> bfs_state_dist;
    mutable vector<int> bfs_basic_dist;
    mutable vector<int> bfs_queue;
    mutable vector<vector<int>> forward_jump_cache;
    mutable vector<int> reverse_macro_head_workspace;
    mutable vector<int> reverse_macro_next_workspace;
    mutable int task_done_epoch_counter = 0;
    mutable vector<int> task_done_epoch;
    mutable int pre_register_cache_start = -1;
    mutable int pre_register_cache_max_steps = -1;
    mutable vector<PreRegisterStart> pre_register_cache;
    mutable bool registration_encoding_cache_valid = false;
    mutable string registration_encoding_cache_macro;
    mutable RegistrationEncoding registration_encoding_cache;

    bool time_exceeded() const {
        return Clock::now() >= deadline;
    }

    bool trace_internal_macro_lens() const {
        static const bool enabled = std::getenv("AHC066_TRACE_MACRO_LENS") != nullptr;
        return enabled;
    }

    bool trace_order_sa() const {
        static const bool enabled = std::getenv("AHC066_TRACE_ORDER_SA") != nullptr;
        return enabled;
    }

    bool animate_order_sa() const {
        static const bool enabled = std::getenv("AHC066_ANIMATE_ORDER_SA") != nullptr;
        return enabled;
    }

    bool animate_macro_sa() const {
        static const bool enabled = std::getenv("AHC066_ANIMATE_MACRO_SA") != nullptr;
        return enabled;
    }

    int env_int_or(const char* name, int fallback) const {
        const char* value = std::getenv(name);
        if (value == nullptr || *value == '\0') {
            return fallback;
        }
        char* end = nullptr;
        const long parsed = std::strtol(value, &end, 10);
        if (end == value || parsed < 0 || parsed > 1'000'000) {
            return fallback;
        }
        return static_cast<int>(parsed);
    }

    string order_to_text(const vector<int>& order) const {
        string text;
        for (int index = 0; index < static_cast<int>(order.size()); ++index) {
            if (index > 0) {
                text.push_back(',');
            }
            text += to_string(order[index]);
        }
        return text;
    }

    int next_bfs_epoch() const {
        if (static_cast<int>(bfs_seen_epoch.size()) != states) {
            bfs_seen_epoch.assign(states, 0);
            bfs_state_dist.assign(states, 0);
            bfs_basic_dist.assign(states, 0);
            bfs_queue.reserve(states);
            bfs_epoch = 0;
        }
        ++bfs_epoch;
        if (bfs_epoch == INF) {
            fill(bfs_seen_epoch.begin(), bfs_seen_epoch.end(), 0);
            bfs_epoch = 1;
        }
        return bfs_epoch;
    }

    void read_input() {
        cin >> n >> m >> t_limit;
        v_wall.resize(n);
        for (int i = 0; i < n; ++i) {
            cin >> v_wall[i];
        }
        h_wall.resize(n - 1);
        for (int i = 0; i < n - 1; ++i) {
            cin >> h_wall[i];
        }
        tasks.resize(m);
        for (int i = 0; i < m; ++i) {
            cin >> tasks[i].ball.r >> tasks[i].ball.c >> tasks[i].basket.r >> tasks[i].basket.c;
        }

        cells = n * n;
        states = cells * 4;
        build_motion_transitions();
        build_cell_graph();
        build_cell_distances();
        build_task_indices();
    }

    void build_motion_transitions() {
        motion_next.assign(states, {});
        reverse_motion_prev.assign(states, {});
        for (auto& prevs : reverse_motion_prev) {
            prevs.fill(-1);
        }
        for (int state = 0; state < states; ++state) {
            const Point p = point_from_cell(state_cell(state));
            const int dir = state_dir(state);
            motion_next[state][0] = state_id(forward_or_stay(p, dir), dir);
            motion_next[state][1] = state_id(p, (dir + 1) % 4);
            motion_next[state][2] = state_id(p, (dir + 3) % 4);

            reverse_motion_prev[state][0] = state_id(p, (dir + 3) % 4);
            reverse_motion_prev[state][1] = state_id(p, (dir + 1) % 4);
            const Point prev{p.r - DR[dir], p.c - DC[dir]};
            if (0 <= prev.r && prev.r < n && 0 <= prev.c && prev.c < n &&
                can_move(prev, dir)) {
                reverse_motion_prev[state][2] = state_id(prev, dir);
            }
        }
    }

    void build_task_indices() {
        unique_basket_cells.clear();
        task_ball_cells.assign(m, 0);
        task_basket_cells.assign(m, 0);
        task_ids_by_basket_cell.assign(cells, {});
        task_ids_by_ball_cell.assign(cells, {});
        vector<char> used_basket(cells, false);
        for (int task_id = 0; task_id < m; ++task_id) {
            const int ball = cell_id(tasks[task_id].ball);
            const int basket = cell_id(tasks[task_id].basket);
            task_ball_cells[task_id] = ball;
            task_basket_cells[task_id] = basket;
            task_ids_by_ball_cell[ball].push_back(task_id);
            task_ids_by_basket_cell[basket].push_back(task_id);
            if (!used_basket[basket]) {
                used_basket[basket] = true;
                unique_basket_cells.push_back(basket);
            }
        }
    }

    AHC066_ALWAYS_INLINE int cell_id(Point p) const {
        return p.r * n + p.c;
    }

    AHC066_ALWAYS_INLINE Point point_from_cell(int id) const {
        return Point{id / n, id % n};
    }

    AHC066_ALWAYS_INLINE int state_id(Point p, int dir) const {
        return cell_id(p) * 4 + dir;
    }

    AHC066_ALWAYS_INLINE int state_cell(int sid) const {
        return sid / 4;
    }

    AHC066_ALWAYS_INLINE int state_dir(int sid) const {
        return sid % 4;
    }

    AHC066_ALWAYS_INLINE bool can_move(Point p, int dir) const {
        if (dir == 0) {
            return p.c + 1 < n && v_wall[p.r][p.c] == '0';
        }
        if (dir == 1) {
            return p.r + 1 < n && h_wall[p.r][p.c] == '0';
        }
        if (dir == 2) {
            return p.c > 0 && v_wall[p.r][p.c - 1] == '0';
        }
        return p.r > 0 && h_wall[p.r - 1][p.c] == '0';
    }

    AHC066_ALWAYS_INLINE Point forward(Point p, int dir) const {
        return Point{p.r + DR[dir], p.c + DC[dir]};
    }

    AHC066_ALWAYS_INLINE Point forward_or_stay(Point p, int dir) const {
        if (!can_move(p, dir)) {
            return p;
        }
        return forward(p, dir);
    }

    AHC066_ALWAYS_INLINE int apply_motion_state(int state, char op) const {
        if (op == 'F') {
            return motion_next[state][0];
        }
        if (op == 'R') {
            return motion_next[state][1];
        }
        if (op == 'L') {
            return motion_next[state][2];
        }
        return state;
    }

    int apply_macro_state(int state, const string& macro) const {
        for (char op : macro) {
            state = apply_motion_state(state, op);
        }
        return state;
    }

    void ensure_forward_jump_count(int count) const {
        if (count < 0 || states == 0) {
            return;
        }
        if (forward_jump_cache.empty() ||
            static_cast<int>(forward_jump_cache.front().size()) != states) {
            forward_jump_cache.clear();
            forward_jump_cache.emplace_back(states);
            for (int state = 0; state < states; ++state) {
                forward_jump_cache[0][state] = state;
            }
        }
        while (static_cast<int>(forward_jump_cache.size()) <= count) {
            const vector<int>& prev = forward_jump_cache.back();
            vector<int> next(states);
            for (int state = 0; state < states; ++state) {
                next[state] = motion_next[prev[state]][0];
            }
            forward_jump_cache.push_back(std::move(next));
        }
    }

    int apply_forward_count(int state, int count) const {
        ensure_forward_jump_count(count);
        return forward_jump_cache[count][state];
    }

    vector<int> build_macro_effect(const string& macro) const {
        int max_forward_run = 0;
        int forward_run = 0;
        for (char op : macro) {
            if (op == 'F') {
                ++forward_run;
                max_forward_run = max(max_forward_run, forward_run);
            } else {
                forward_run = 0;
            }
        }
        ensure_forward_jump_count(max_forward_run);

        vector<int> effect(states);
        for (int state = 0; state < states; ++state) {
            int current = state;
            int index = 0;
            while (index < static_cast<int>(macro.size())) {
                if (macro[index] == 'F') {
                    int next_index = index + 1;
                    while (next_index < static_cast<int>(macro.size()) &&
                           macro[next_index] == 'F') {
                        ++next_index;
                    }
                    current = forward_jump_cache[next_index - index][current];
                    index = next_index;
                    continue;
                }
                if (macro[index] == 'R') {
                    current = motion_next[current][1];
                } else if (macro[index] == 'L') {
                    current = motion_next[current][2];
                }
                ++index;
            }
            effect[state] = current;
        }
        return effect;
    }

    void build_macro_effect_from_blocks_into(const vector<InternalMacroBlock>& blocks,
                                             vector<int>& effect) const {
        int max_forward_count = 0;
        for (const InternalMacroBlock& block : blocks) {
            max_forward_count = max(max_forward_count, block.forward_count);
        }
        ensure_forward_jump_count(max_forward_count);

        effect.resize(states);
        for (int state = 0; state < states; ++state) {
            int current = state;
            for (const InternalMacroBlock& block : blocks) {
                current = forward_jump_cache[block.forward_count][current];
                if (block.rotation == 0) {
                    current = motion_next[current][2];
                } else if (block.rotation == 1) {
                    current = motion_next[current][1];
                } else {
                    current = motion_next[motion_next[current][2]][2];
                }
            }
            effect[state] = current;
        }
    }

    void build_cell_graph() {
        cell_neighbors.assign(cells, {});
        for (int cell = 0; cell < cells; ++cell) {
            Point p = point_from_cell(cell);
            for (int dir = 0; dir < 4; ++dir) {
                if (!can_move(p, dir)) {
                    continue;
                }
                cell_neighbors[cell].push_back(cell_id(forward(p, dir)));
            }
        }
    }

    void build_cell_distances() {
        cell_dist.assign(cells, vector<int>(cells, INF));
        queue<int> q;
        for (int src = 0; src < cells; ++src) {
            auto& dist = cell_dist[src];
            fill(dist.begin(), dist.end(), INF);
            dist[src] = 0;
            q.push(src);
            while (!q.empty()) {
                const int cur = q.front();
                q.pop();
                for (int nxt : cell_neighbors[cur]) {
                    if (dist[nxt] != INF) {
                        continue;
                    }
                    dist[nxt] = dist[cur] + 1;
                    q.push(nxt);
                }
            }
        }
    }

    long long order_cost(const vector<int>& order, int start_cell) const {
        if (order.empty()) {
            return 0;
        }
        long long cost = cell_dist[start_cell][cell_id(tasks[order[0]].ball)];
        for (int i = 0; i < static_cast<int>(order.size()); ++i) {
            const Task& task = tasks[order[i]];
            cost += cell_dist[cell_id(task.ball)][cell_id(task.basket)];
            if (i + 1 < static_cast<int>(order.size())) {
                cost += cell_dist[cell_id(task.basket)][cell_id(tasks[order[i + 1]].ball)];
            }
        }
        return cost;
    }

    vector<int> greedy_order(int mode, int start_cell) const {
        vector<int> order;
        vector<bool> used(m, false);
        int cur_cell = start_cell;

        for (int step = 0; step < m; ++step) {
            int best_task = -1;
            long long best_score = static_cast<long long>(INF) * INF;
            pair<int, int> best_tie{INF, INF};
            for (int i = 0; i < m; ++i) {
                if (used[i]) {
                    continue;
                }
                const int to_ball = cell_dist[cur_cell][cell_id(tasks[i].ball)];
                const int to_basket = cell_dist[cell_id(tasks[i].ball)][cell_id(tasks[i].basket)];
                long long score = 0;
                if (mode == 0) {
                    score = to_ball + to_basket;
                } else if (mode == 1) {
                    score = 2LL * to_ball + to_basket;
                } else if (mode == 2) {
                    score = to_ball;
                } else {
                    score = to_basket;
                }
                pair<int, int> tie{to_ball + to_basket, i};
                if (score < best_score || (score == best_score && tie < best_tie)) {
                    best_score = score;
                    best_tie = tie;
                    best_task = i;
                }
            }
            used[best_task] = true;
            order.push_back(best_task);
            cur_cell = cell_id(tasks[best_task].basket);
        }
        return order;
    }

    vector<int> improved_order(vector<int> order, int start_cell) const {
        long long current = order_cost(order, start_cell);
        for (int pass = 0; pass < 20; ++pass) {
            vector<int> best_order = order;
            long long best_cost = current;

            for (int i = 0; i < m; ++i) {
                for (int j = 0; j <= m; ++j) {
                    if (j == i || j == i + 1) {
                        continue;
                    }
                    vector<int> candidate = order;
                    const int task = candidate[i];
                    candidate.erase(candidate.begin() + i);
                    const int insert_pos = j > i ? j - 1 : j;
                    candidate.insert(candidate.begin() + insert_pos, task);
                    const long long cost = order_cost(candidate, start_cell);
                    if (cost < best_cost) {
                        best_cost = cost;
                        best_order = std::move(candidate);
                    }
                }
            }

            for (int i = 0; i < m; ++i) {
                for (int j = i + 1; j < m; ++j) {
                    vector<int> candidate = order;
                    swap(candidate[i], candidate[j]);
                    const long long cost = order_cost(candidate, start_cell);
                    if (cost < best_cost) {
                        best_cost = cost;
                        best_order = std::move(candidate);
                    }
                }
            }

            if (best_cost >= current) {
                break;
            }
            current = best_cost;
            order = std::move(best_order);
        }
        return order;
    }

    vector<vector<int>> initial_order_candidates(int start_cell) const {
        vector<vector<int>> orders;
        for (int mode = 0; mode < 4; ++mode) {
            vector<int> greedy = greedy_order(mode, start_cell);
            orders.push_back(greedy);
            orders.push_back(improved_order(std::move(greedy), start_cell));
        }
        return orders;
    }

    MotionPath shortest_motion_with_macro(int start_state, int target_cell,
                                          const vector<int>& macro_effect) const {
        vector<int> parent(states, -1);
        vector<char> parent_op(states, '?');
        queue<int> q;
        parent[start_state] = start_state;
        q.push(start_state);

        int goal_state = -1;
        while (!q.empty()) {
            const int cur = q.front();
            q.pop();
            if (state_cell(cur) == target_cell) {
                goal_state = cur;
                break;
            }

            for (char op : string("FRL")) {
                const int nxt = apply_motion_state(cur, op);
                if (parent[nxt] != -1) {
                    continue;
                }
                parent[nxt] = cur;
                parent_op[nxt] = op;
                q.push(nxt);
            }

            const int macro_next = macro_effect[cur];
            if (parent[macro_next] == -1) {
                parent[macro_next] = cur;
                parent_op[macro_next] = 'P';
                q.push(macro_next);
            }
        }

        if (goal_state == -1) {
            return MotionPath{"", start_state};
        }

        vector<char> rev;
        int cur = goal_state;
        while (cur != start_state) {
            rev.push_back(parent_op[cur]);
            cur = parent[cur];
        }
        reverse(rev.begin(), rev.end());
        return MotionPath{string(rev.begin(), rev.end()), goal_state};
    }

    MotionEval shortest_motion_eval_with_macro(int start_state, int target_cell,
                                               const vector<int>& macro_effect,
                                               int macro_length) const {
        const int epoch = next_bfs_epoch();
        bfs_queue.clear();
        bfs_seen_epoch[start_state] = epoch;
        bfs_state_dist[start_state] = 0;
        bfs_basic_dist[start_state] = 0;
        bfs_queue.push_back(start_state);

        for (int head = 0; head < static_cast<int>(bfs_queue.size()); ++head) {
            const int cur = bfs_queue[head];
            if (state_cell(cur) == target_cell) {
                return MotionEval{bfs_state_dist[cur], bfs_basic_dist[cur], cur};
            }

            for (int op = 0; op < 3; ++op) {
                const int nxt = motion_next[cur][op];
                if (bfs_seen_epoch[nxt] == epoch) {
                    continue;
                }
                bfs_seen_epoch[nxt] = epoch;
                bfs_state_dist[nxt] = bfs_state_dist[cur] + 1;
                bfs_basic_dist[nxt] = bfs_basic_dist[cur] + 1;
                bfs_queue.push_back(nxt);
            }

            const int macro_next = macro_effect[cur];
            if (bfs_seen_epoch[macro_next] != epoch) {
                bfs_seen_epoch[macro_next] = epoch;
                bfs_state_dist[macro_next] = bfs_state_dist[cur] + 1;
                bfs_basic_dist[macro_next] = bfs_basic_dist[cur] + macro_length;
                bfs_queue.push_back(macro_next);
            }
        }

        return MotionEval{INF, INF, start_state};
    }

    vector<int> macro_dist_from_state(int src, const vector<int>& macro_effect) const {
        vector<int> dist(cells, INF);
        const int epoch = next_bfs_epoch();
        bfs_queue.clear();
        bfs_seen_epoch[src] = epoch;
        bfs_state_dist[src] = 0;
        bfs_queue.push_back(src);

        for (int head = 0; head < static_cast<int>(bfs_queue.size()); ++head) {
            const int cur = bfs_queue[head];
            dist[state_cell(cur)] = min(dist[state_cell(cur)], bfs_state_dist[cur]);

            for (int op = 0; op < 3; ++op) {
                const int nxt = motion_next[cur][op];
                if (bfs_seen_epoch[nxt] == epoch) {
                    continue;
                }
                bfs_seen_epoch[nxt] = epoch;
                bfs_state_dist[nxt] = bfs_state_dist[cur] + 1;
                bfs_queue.push_back(nxt);
            }

            const int macro_next = macro_effect[cur];
            if (bfs_seen_epoch[macro_next] != epoch) {
                bfs_seen_epoch[macro_next] = epoch;
                bfs_state_dist[macro_next] = bfs_state_dist[cur] + 1;
                bfs_queue.push_back(macro_next);
            }
        }
        return dist;
    }

    vector<array<int, 4>> macro_dir_dist_from_state(int src,
                                                    const vector<int>& macro_effect) const {
        vector<array<int, 4>> dist(cells);
        for (auto& row : dist) {
            row.fill(INF);
        }
        const int epoch = next_bfs_epoch();
        bfs_queue.clear();
        bfs_seen_epoch[src] = epoch;
        bfs_state_dist[src] = 0;
        bfs_queue.push_back(src);

        for (int head = 0; head < static_cast<int>(bfs_queue.size()); ++head) {
            const int cur = bfs_queue[head];
            dist[state_cell(cur)][state_dir(cur)] = bfs_state_dist[cur];

            for (int op = 0; op < 3; ++op) {
                const int nxt = motion_next[cur][op];
                if (bfs_seen_epoch[nxt] == epoch) {
                    continue;
                }
                bfs_seen_epoch[nxt] = epoch;
                bfs_state_dist[nxt] = bfs_state_dist[cur] + 1;
                bfs_queue.push_back(nxt);
            }

            const int macro_next = macro_effect[cur];
            if (bfs_seen_epoch[macro_next] != epoch) {
                bfs_seen_epoch[macro_next] = epoch;
                bfs_state_dist[macro_next] = bfs_state_dist[cur] + 1;
                bfs_queue.push_back(macro_next);
            }
        }
        return dist;
    }

    void macro_dir_dist_from_state_to_target_cells_into(
        int src,
        int target_count,
        const vector<int>& target_index_by_cell,
        const vector<int>& macro_effect,
        array<IntDir4, MAX_M>& dist
    ) const {
        for (int index = 0; index < target_count; ++index) {
            dist[index].fill(INF);
        }
        if (target_count == 0) {
            return;
        }

        int remaining = target_count * 4;
        const int epoch = next_bfs_epoch();
        bfs_queue.clear();
        bfs_seen_epoch[src] = epoch;
        bfs_state_dist[src] = 0;
        bfs_queue.push_back(src);

        for (int head = 0; head < static_cast<int>(bfs_queue.size()); ++head) {
            const int cur = bfs_queue[head];
            const int cell = state_cell(cur);
            const int target_index = target_index_by_cell[cell];
            if (target_index >= 0 && target_index < target_count) {
                const int dir = state_dir(cur);
                if (dist[target_index][dir] == INF) {
                    dist[target_index][dir] = bfs_state_dist[cur];
                    --remaining;
                    if (remaining == 0) {
                        break;
                    }
                }
            }

            for (int op = 0; op < 3; ++op) {
                const int nxt = motion_next[cur][op];
                if (bfs_seen_epoch[nxt] == epoch) {
                    continue;
                }
                bfs_seen_epoch[nxt] = epoch;
                bfs_state_dist[nxt] = bfs_state_dist[cur] + 1;
                bfs_queue.push_back(nxt);
            }

            const int macro_next = macro_effect[cur];
            if (bfs_seen_epoch[macro_next] != epoch) {
                bfs_seen_epoch[macro_next] = epoch;
                bfs_state_dist[macro_next] = bfs_state_dist[cur] + 1;
                bfs_queue.push_back(macro_next);
            }
        }
    }

    array<IntDir4, MAX_M> pre_registered_start_to_ball_costs(
        const string& macro,
        const vector<int>& macro_effect,
        const vector<int>& ball_target_cells,
        const vector<int>& ball_target_index_by_cell
    ) const {
        array<IntDir4, MAX_M> best;
        const int target_count = static_cast<int>(ball_target_cells.size());
        for (int index = 0; index < target_count; ++index) {
            best[index].fill(INF);
        }
        if (macro.empty()) {
            return best;
        }

        vector<int> task_ball_indexes(m);
        for (int task_id = 0; task_id < m; ++task_id) {
            task_ball_indexes[task_id] =
                ball_target_index_by_cell[cell_id(tasks[task_id].ball)];
        }

        const int original_start = state_id(Point{0, 0}, START_DIR);
        array<IntDir4, MAX_M> dist;
        for (const PreRegisterStart& pre_start : pre_register_start_candidates(original_start)) {
            if (time_exceeded()) {
                break;
            }
            for (RegistrationPlan plan :
                 registration_plan_candidates(macro, pre_start.state, &macro_effect)) {
                const int prefix_output =
                    static_cast<int>(pre_start.ops.size()) + plan.output_ops;
                if (prefix_output > t_limit) {
                    continue;
                }
                macro_dir_dist_from_state_to_target_cells_into(
                    plan.initial_state,
                    target_count,
                    ball_target_index_by_cell,
                    macro_effect,
                    dist
                );
                for (int task_id = 0; task_id < m; ++task_id) {
                    const int ball_index = task_ball_indexes[task_id];
                    for (int dir = 0; dir < 4; ++dir) {
                        if (dist[ball_index][dir] >= INF) {
                            continue;
                        }
                        best[ball_index][dir] =
                            min(best[ball_index][dir], prefix_output + dist[ball_index][dir]);
                    }
                }
            }
        }
        return best;
    }

    array<int, 4> macro_dir_dist_from_state_to_cell(
        int src,
        int target_cell,
        const vector<int>& macro_effect
    ) const {
        array<int, 4> dist{};
        dist.fill(INF);
        int remaining = 4;
        const int epoch = next_bfs_epoch();
        bfs_queue.clear();
        bfs_seen_epoch[src] = epoch;
        bfs_state_dist[src] = 0;
        bfs_queue.push_back(src);

        for (int head = 0; head < static_cast<int>(bfs_queue.size()); ++head) {
            const int cur = bfs_queue[head];
            if (state_cell(cur) == target_cell) {
                const int dir = state_dir(cur);
                if (dist[dir] == INF) {
                    dist[dir] = bfs_state_dist[cur];
                    --remaining;
                    if (remaining == 0) {
                        break;
                    }
                }
            }

            for (int op = 0; op < 3; ++op) {
                const int nxt = motion_next[cur][op];
                if (bfs_seen_epoch[nxt] == epoch) {
                    continue;
                }
                bfs_seen_epoch[nxt] = epoch;
                bfs_state_dist[nxt] = bfs_state_dist[cur] + 1;
                bfs_queue.push_back(nxt);
            }

            const int macro_next = macro_effect[cur];
            if (bfs_seen_epoch[macro_next] != epoch) {
                bfs_seen_epoch[macro_next] = epoch;
                bfs_state_dist[macro_next] = bfs_state_dist[cur] + 1;
                bfs_queue.push_back(macro_next);
            }
        }
        return dist;
    }

    long long directed_pair_order_cost(const vector<int>& order,
                                       const array<IntDir4, MAX_M>& start_to_ball,
                                       const array<IntMat4, MAX_M>& ball_to_goal,
                                       const array<array<IntMat4, MAX_M>, MAX_M>& goal_to_ball) const {
        if (order.empty()) {
            return 0;
        }

        array<long long, 4> dp{};
        dp.fill(static_cast<long long>(INF) * INF);
        const int first = order[0];
        for (int ball_dir = 0; ball_dir < 4; ++ball_dir) {
            for (int goal_dir = 0; goal_dir < 4; ++goal_dir) {
                const long long cost =
                    static_cast<long long>(start_to_ball[first][ball_dir]) +
                    ball_to_goal[first][ball_dir][goal_dir] + 2;
                dp[goal_dir] = min(dp[goal_dir], cost);
            }
        }

        for (int index = 1; index < static_cast<int>(order.size()); ++index) {
            const int prev = order[index - 1];
            const int cur = order[index];
            array<long long, 4> next_dp{};
            next_dp.fill(static_cast<long long>(INF) * INF);
            for (int prev_goal_dir = 0; prev_goal_dir < 4; ++prev_goal_dir) {
                if (dp[prev_goal_dir] >= static_cast<long long>(INF) * INF / 2) {
                    continue;
                }
                for (int ball_dir = 0; ball_dir < 4; ++ball_dir) {
                    const long long to_ball =
                        goal_to_ball[prev][cur][prev_goal_dir][ball_dir];
                    for (int goal_dir = 0; goal_dir < 4; ++goal_dir) {
                        const long long cost =
                            dp[prev_goal_dir] + to_ball +
                            ball_to_goal[cur][ball_dir][goal_dir] + 2;
                        next_dp[goal_dir] = min(next_dp[goal_dir], cost);
                    }
                }
            }
            dp = next_dp;
        }
        return *min_element(dp.begin(), dp.end());
    }

    vector<int> greedy_pair_order(const array<int, MAX_M>& start_cost,
                                  const array<array<int, MAX_M>, MAX_M>& pair_cost,
                                  int mode) const {
        vector<int> order;
        vector<bool> used(m, false);
        int prev = -1;
        for (int step = 0; step < m; ++step) {
            int best_task = -1;
            long long best_score = static_cast<long long>(INF) * INF;
            pair<long long, int> best_tie{static_cast<long long>(INF) * INF, INF};
            for (int task_id = 0; task_id < m; ++task_id) {
                if (used[task_id]) {
                    continue;
                }
                const long long transition = prev == -1 ? start_cost[task_id]
                                                        : pair_cost[prev][task_id];
                const long long return_bias = pair_cost[task_id][task_id];
                long long score = transition;
                if (mode == 1) {
                    score = 2 * transition + return_bias;
                } else if (mode == 2) {
                    score = transition + 2 * return_bias;
                } else if (mode == 3) {
                    score = return_bias;
                }
                pair<long long, int> tie{transition + return_bias, task_id};
                if (score < best_score || (score == best_score && tie < best_tie)) {
                    best_score = score;
                    best_tie = tie;
                    best_task = task_id;
                }
            }
            used[best_task] = true;
            order.push_back(best_task);
            prev = best_task;
        }
        return order;
    }

    vector<int> annealed_macro_pair_order(int start_state,
                                          const vector<int>& macro_effect,
                                          const string& macro_text,
                                          int order_output_budget,
                                          int order_search_millis = 20,
                                          const vector<int>* seed_order = nullptr,
                                          bool use_pre_registered_start = false) const {
        vector<int> ball_target_cells;
        vector<int> ball_target_index_by_cell(cells, -1);
        auto add_ball_target_cell = [&](int cell) {
            if (ball_target_index_by_cell[cell] != -1) {
                return;
            }
            ball_target_index_by_cell[cell] = static_cast<int>(ball_target_cells.size());
            ball_target_cells.push_back(cell);
        };
        for (const Task& task : tasks) {
            add_ball_target_cell(cell_id(task.ball));
        }

        array<IntDir4, MAX_M> start_dist;
        if (use_pre_registered_start) {
            start_dist = pre_registered_start_to_ball_costs(
                macro_text,
                macro_effect,
                ball_target_cells,
                ball_target_index_by_cell
            );
        } else {
            macro_dir_dist_from_state_to_target_cells_into(
                start_state,
                static_cast<int>(ball_target_cells.size()),
                ball_target_index_by_cell,
                macro_effect,
                start_dist
            );
        }
        array<IntDir4, MAX_M> start_to_ball;
        array<IntMat4, MAX_M> ball_to_goal;
        array<array<IntMat4, MAX_M>, MAX_M> goal_to_ball;
        for (int task_id = 0; task_id < m; ++task_id) {
            start_to_ball[task_id].fill(INF);
            for (int ball_dir = 0; ball_dir < 4; ++ball_dir) {
                ball_to_goal[task_id][ball_dir].fill(INF);
            }
            for (int next = 0; next < m; ++next) {
                for (int goal_dir = 0; goal_dir < 4; ++goal_dir) {
                    goal_to_ball[task_id][next][goal_dir].fill(INF);
                }
            }
        }

        for (int task_id = 0; task_id < m; ++task_id) {
            const int ball = cell_id(tasks[task_id].ball);
            const int ball_index = ball_target_index_by_cell[ball];
            for (int ball_dir = 0; ball_dir < 4; ++ball_dir) {
                start_to_ball[task_id][ball_dir] = start_dist[ball_index][ball_dir];
                ball_to_goal[task_id][ball_dir] =
                    macro_dir_dist_from_state_to_cell(
                        state_id(tasks[task_id].ball, ball_dir),
                        cell_id(tasks[task_id].basket),
                        macro_effect
                    );
            }
        }

        for (int prev = 0; prev < m; ++prev) {
            for (int goal_dir = 0; goal_dir < 4; ++goal_dir) {
                array<IntDir4, MAX_M> dist_to_balls;
                macro_dir_dist_from_state_to_target_cells_into(
                    state_id(tasks[prev].basket, goal_dir),
                    static_cast<int>(ball_target_cells.size()),
                    ball_target_index_by_cell,
                    macro_effect,
                    dist_to_balls
                );
                for (int next = 0; next < m; ++next) {
                    const int next_ball = cell_id(tasks[next].ball);
                    const int next_ball_index = ball_target_index_by_cell[next_ball];
                    for (int next_ball_dir = 0; next_ball_dir < 4; ++next_ball_dir) {
                        goal_to_ball[prev][next][goal_dir][next_ball_dir] =
                            dist_to_balls[next_ball_index][next_ball_dir];
                    }
                }
            }
        }

        array<int, MAX_M> start_cost{};
        array<array<int, MAX_M>, MAX_M> pair_cost{};
        start_cost.fill(INF);
        for (auto& row : pair_cost) {
            row.fill(INF);
        }
        for (int task_id = 0; task_id < m; ++task_id) {
            for (int ball_dir = 0; ball_dir < 4; ++ball_dir) {
                for (int goal_dir = 0; goal_dir < 4; ++goal_dir) {
                    start_cost[task_id] = min(
                        start_cost[task_id],
                        start_to_ball[task_id][ball_dir] +
                            ball_to_goal[task_id][ball_dir][goal_dir] + 2
                    );
                }
            }
            for (int next = 0; next < m; ++next) {
                for (int goal_dir = 0; goal_dir < 4; ++goal_dir) {
                    for (int next_ball_dir = 0; next_ball_dir < 4; ++next_ball_dir) {
                        for (int next_goal_dir = 0; next_goal_dir < 4; ++next_goal_dir) {
                            pair_cost[task_id][next] = min(
                                pair_cost[task_id][next],
                                goal_to_ball[task_id][next][goal_dir][next_ball_dir] +
                                    ball_to_goal[next][next_ball_dir][next_goal_dir] + 2
                            );
                        }
                    }
                }
            }
        }

        const long long large_cost = static_cast<long long>(INF) * INF;
        array<LongDir4, MAX_M> start_vec;
        array<array<LongMat4, MAX_M>, MAX_M> edge_cost;
        for (int task_id = 0; task_id < m; ++task_id) {
            start_vec[task_id].fill(large_cost);
            for (int ball_dir = 0; ball_dir < 4; ++ball_dir) {
                for (int goal_dir = 0; goal_dir < 4; ++goal_dir) {
                    start_vec[task_id][goal_dir] = min(
                        start_vec[task_id][goal_dir],
                        static_cast<long long>(start_to_ball[task_id][ball_dir]) +
                            ball_to_goal[task_id][ball_dir][goal_dir] + 2
                    );
                }
            }
        }
        for (int prev = 0; prev < m; ++prev) {
            for (int next = 0; next < m; ++next) {
                for (int prev_goal_dir = 0; prev_goal_dir < 4; ++prev_goal_dir) {
                    edge_cost[prev][next][prev_goal_dir].fill(large_cost);
                    for (int next_ball_dir = 0; next_ball_dir < 4; ++next_ball_dir) {
                        for (int next_goal_dir = 0; next_goal_dir < 4; ++next_goal_dir) {
                            edge_cost[prev][next][prev_goal_dir][next_goal_dir] = min(
                                edge_cost[prev][next][prev_goal_dir][next_goal_dir],
                                static_cast<long long>(
                                    goal_to_ball[prev][next][prev_goal_dir][next_ball_dir]
                                ) + ball_to_goal[next][next_ball_dir][next_goal_dir] + 2
                            );
                        }
                    }
                }
            }
        }
        auto order_limit_score = [&](long long raw_cost) {
            if (order_output_budget < 0 || raw_cost <= order_output_budget) {
                return raw_cost;
            }
            return large_cost / 4 + (raw_cost - order_output_budget);
        };

        vector<vector<int>> candidates;
        auto add_order_candidate = [&](const vector<int>& order) {
            if (static_cast<int>(order.size()) != m) {
                return;
            }
            for (const vector<int>& candidate : candidates) {
                if (candidate == order) {
                    return;
                }
            }
            candidates.push_back(order);
        };
        if (seed_order != nullptr && static_cast<int>(seed_order->size()) == m) {
            add_order_candidate(*seed_order);
        } else {
            for (const vector<int>& order : initial_order_candidates(state_cell(start_state))) {
                add_order_candidate(order);
            }
        }
        for (int mode = 0; mode < 4; ++mode) {
            add_order_candidate(greedy_pair_order(start_cost, pair_cost, mode));
        }
        if (candidates.empty()) {
            return {};
        }

        vector<int> best = candidates.front();
        long long best_raw_cost =
            directed_pair_order_cost(best, start_to_ball, ball_to_goal, goal_to_ball);
        long long best_cost = order_limit_score(best_raw_cost);
        for (int candidate_index = 0; candidate_index < static_cast<int>(candidates.size());
             ++candidate_index) {
            const vector<int>& order = candidates[candidate_index];
            const long long raw_cost =
                directed_pair_order_cost(order, start_to_ball, ball_to_goal, goal_to_ball);
            const long long cost = order_limit_score(raw_cost);
            if (cost < best_cost) {
                best_cost = cost;
                best_raw_cost = raw_cost;
                best = order;
            }
        }

        const auto local_deadline = min(
            deadline,
            Clock::now() + chrono::milliseconds(max(0, order_search_millis))
        );
        mt19937 rng(0xBADC0DEu + static_cast<uint32_t>(m * 1291 + start_state * 17));
        uniform_real_distribution<double> unit(0.0, 1.0);
        vector<int> current = best;
        array<LongDir4, MAX_M> prefix_dp;
        array<LongDir4, MAX_M> suffix_dp;
        auto apply_edge = [&](const LongDir4& vec, int prev, int next) {
            LongDir4 result{};
            result.fill(large_cost);
            for (int prev_dir = 0; prev_dir < 4; ++prev_dir) {
                for (int next_dir = 0; next_dir < 4; ++next_dir) {
                    result[next_dir] = min(
                        result[next_dir],
                        vec[prev_dir] + edge_cost[prev][next][prev_dir][next_dir]
                    );
                }
            }
            return result;
        };
        array<array<LongMat4, MAX_M>, MAX_M> range_trans;
        auto empty_matrix = [&]() {
            LongMat4 matrix{};
            for (auto& row : matrix) {
                row.fill(large_cost);
            }
            return matrix;
        };
        auto identity_matrix = [&]() {
            LongMat4 matrix = empty_matrix();
            for (int dir = 0; dir < 4; ++dir) {
                matrix[dir][dir] = 0;
            }
            return matrix;
        };
        auto edge_matrix = [&](int prev, int next) {
            LongMat4 matrix{};
            for (int prev_dir = 0; prev_dir < 4; ++prev_dir) {
                for (int next_dir = 0; next_dir < 4; ++next_dir) {
                    matrix[prev_dir][next_dir] = edge_cost[prev][next][prev_dir][next_dir];
                }
            }
            return matrix;
        };
        auto multiply_matrix = [&](const LongMat4& lhs, const LongMat4& rhs) {
            LongMat4 result = empty_matrix();
            for (int from = 0; from < 4; ++from) {
                for (int mid = 0; mid < 4; ++mid) {
                    if (lhs[from][mid] >= large_cost) {
                        continue;
                    }
                    for (int to = 0; to < 4; ++to) {
                        result[from][to] =
                            min(result[from][to], lhs[from][mid] + rhs[mid][to]);
                    }
                }
            }
            return result;
        };
        auto apply_matrix = [&](const LongDir4& vec, const LongMat4& matrix) {
            LongDir4 result{};
            result.fill(large_cost);
            for (int from = 0; from < 4; ++from) {
                if (vec[from] >= large_cost) {
                    continue;
                }
                for (int to = 0; to < 4; ++to) {
                    result[to] = min(result[to], vec[from] + matrix[from][to]);
                }
            }
            return result;
        };
        auto rebuild_direction_dp = [&]() {
            if (m == 0) {
                return 0LL;
            }
            prefix_dp[0] = start_vec[current[0]];
            for (int pos = 1; pos < m; ++pos) {
                prefix_dp[pos] = apply_edge(prefix_dp[pos - 1], current[pos - 1], current[pos]);
            }
            suffix_dp[m - 1].fill(0);
            for (int pos = m - 2; pos >= 0; --pos) {
                suffix_dp[pos].fill(large_cost);
                for (int dir = 0; dir < 4; ++dir) {
                    for (int next_dir = 0; next_dir < 4; ++next_dir) {
                        suffix_dp[pos][dir] = min(
                            suffix_dp[pos][dir],
                            edge_cost[current[pos]][current[pos + 1]][dir][next_dir] +
                                suffix_dp[pos + 1][next_dir]
                        );
                    }
                }
            }
            for (int left = 0; left < m; ++left) {
                LongMat4 matrix = identity_matrix();
                range_trans[left][left] = matrix;
                for (int right = left + 1; right < m; ++right) {
                    matrix = multiply_matrix(
                        matrix,
                        edge_matrix(current[right - 1], current[right])
                    );
                    range_trans[left][right] = matrix;
                }
            }
            return *min_element(prefix_dp[m - 1].begin(), prefix_dp[m - 1].end());
        };
        auto relocated_order_cost = [&](int lhs, int rhs, int insert_pos) {
            struct ChainState {
                bool has = false;
                int prev_task = -1;
                array<long long, 4> dp{};
            };

            ChainState state;
            state.dp.fill(large_cost);
            auto init_prefix = [&](int end_exclusive) {
                if (end_exclusive <= 0) {
                    return;
                }
                state.has = true;
                state.prev_task = current[end_exclusive - 1];
                state.dp = prefix_dp[end_exclusive - 1];
            };
            auto append_segment = [&](int begin, int end) {
                if (begin >= end) {
                    return;
                }
                const int last = end - 1;
                if (!state.has) {
                    state.has = true;
                    state.prev_task = current[last];
                    state.dp = apply_matrix(start_vec[current[begin]], range_trans[begin][last]);
                    return;
                }
                state.dp = apply_edge(state.dp, state.prev_task, current[begin]);
                state.dp = apply_matrix(state.dp, range_trans[begin][last]);
                state.prev_task = current[last];
            };
            auto finish_with_suffix = [&](int suffix_begin) {
                if (suffix_begin >= m) {
                    if (!state.has) {
                        return 0LL;
                    }
                    return *min_element(state.dp.begin(), state.dp.end());
                }
                if (!state.has) {
                    long long result = large_cost;
                    const int first = current[suffix_begin];
                    for (int dir = 0; dir < 4; ++dir) {
                        result = min(result, start_vec[first][dir] + suffix_dp[suffix_begin][dir]);
                    }
                    return result;
                }

                long long result = large_cost;
                const int first = current[suffix_begin];
                for (int prev_dir = 0; prev_dir < 4; ++prev_dir) {
                    for (int next_dir = 0; next_dir < 4; ++next_dir) {
                        result = min(
                            result,
                            state.dp[prev_dir] +
                                edge_cost[state.prev_task][first][prev_dir][next_dir] +
                                suffix_dp[suffix_begin][next_dir]
                        );
                    }
                }
                return result;
            };

            if (insert_pos <= lhs) {
                init_prefix(insert_pos);
                append_segment(lhs, rhs + 1);
                append_segment(insert_pos, lhs);
                return finish_with_suffix(rhs + 1);
            }

            const int middle_begin = rhs + 1;
            const int middle_end = rhs + 1 + (insert_pos - lhs);
            init_prefix(lhs);
            append_segment(middle_begin, middle_end);
            append_segment(lhs, rhs + 1);
            return finish_with_suffix(middle_end);
        };
        long long current_raw_cost = rebuild_direction_dp();
        long long current_cost = order_limit_score(current_raw_cost);
        best_raw_cost = current_raw_cost;
        best_cost = current_cost;
        const long long initial_best_raw_cost = best_raw_cost;
        const auto start_time = Clock::now();
        const double total_ms = max(
            1.0,
            static_cast<double>(chrono::duration_cast<chrono::milliseconds>(
                                    local_deadline - start_time)
                                    .count())
        );
        auto now = start_time;
        int iteration = 0;
        int accepted = 0;
        int improved = 0;
        double progress = 0.0;
        double temperature = 8.0;
        const bool animate_sa = animate_order_sa();
        auto last_animation_time = start_time - chrono::milliseconds(1000);
        auto render_order_animation = [&](bool force) {
            if (!animate_sa) {
                return;
            }
            const auto frame_time = Clock::now();
            if (!force &&
                frame_time - last_animation_time < chrono::milliseconds(40)) {
                return;
            }
            last_animation_time = frame_time;
            const double elapsed_ms = static_cast<double>(
                chrono::duration_cast<chrono::microseconds>(frame_time - start_time).count()
            ) / 1000.0;
            const int bar_width = 24;
            const int filled = min(
                bar_width,
                max(0, static_cast<int>(progress * bar_width))
            );
            string bar(static_cast<size_t>(bar_width), '.');
            for (int index = 0; index < filled; ++index) {
                bar[index] = '#';
            }
            cerr << "\r\033[K"
                 << "order_sa [" << bar << "]"
                 << " " << static_cast<int>(progress * 100.0) << "%"
                 << " macro=" << macro_text
                 << " len=" << macro_text.size()
                 << " elapsed_ms=" << static_cast<int>(elapsed_ms)
                 << " iter=" << iteration
                 << " temp=" << static_cast<int>(temperature * 1000.0) / 1000.0
                 << " budget=" << order_output_budget
                 << " current=" << current_raw_cost
                 << " best=" << best_raw_cost
                 << " gain=" << (initial_best_raw_cost - best_raw_cost)
                 << " score=" << best_cost
                 << " accepted=" << accepted
                 << " improved=" << improved
                 << flush;
        };
        render_order_animation(true);
        while (now < local_deadline) {
            if ((iteration++ & 63) == 0) {
                now = Clock::now();
                const double elapsed_ms = static_cast<double>(
                    chrono::duration_cast<chrono::milliseconds>(now - start_time).count());
                progress = min(1.0, elapsed_ms / total_ms);
                temperature = 8.0 * pow(0.05 / 8.0, progress);
                render_order_animation(false);
                if (now >= local_deadline) {
                    break;
                }
            }
            if (m <= 1) {
                continue;
            }
            const int lhs = static_cast<int>(rng() % static_cast<uint32_t>(m));
            const int rhs = lhs + static_cast<int>(rng() % static_cast<uint32_t>(m - lhs));
            const int block_len = rhs - lhs + 1;
            const int remaining = m - block_len;
            if (remaining == 0) {
                continue;
            }
            const int insert_pos =
                static_cast<int>(rng() % static_cast<uint32_t>(remaining + 1));
            if (insert_pos == lhs) {
                continue;
            }

            const long long candidate_raw_cost = relocated_order_cost(lhs, rhs, insert_pos);
            const long long candidate_cost = order_limit_score(candidate_raw_cost);
            const long long delta = candidate_cost - current_cost;
            if (delta <= 0 || unit(rng) < exp(-static_cast<double>(delta) / temperature)) {
                if (insert_pos < lhs) {
                    rotate(current.begin() + insert_pos,
                           current.begin() + lhs,
                           current.begin() + rhs + 1);
                } else {
                    const int middle_end = rhs + 1 + (insert_pos - lhs);
                    rotate(current.begin() + lhs,
                           current.begin() + rhs + 1,
                           current.begin() + middle_end);
                }
                current_raw_cost = rebuild_direction_dp();
                current_cost = order_limit_score(current_raw_cost);
                ++accepted;
            } else {
                continue;
            }
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best_raw_cost = current_raw_cost;
                best = current;
                ++improved;
                render_order_animation(true);
            }
        }
        progress = 1.0;
        render_order_animation(true);
        if (animate_sa) {
            cerr << '\n';
        }
        if (trace_order_sa() && !animate_sa) {
            const double accept_rate =
                iteration > 0 ? static_cast<double>(accepted) / iteration : 0.0;
            cerr << "order_sa result"
                 << " iter=" << iteration
                 << " accepted=" << accepted
                 << " accept_rate=" << accept_rate
                 << " score=" << best_cost
                 << " raw_cost=" << best_raw_cost
                 << " best_order=" << order_to_text(best)
                 << '\n';
        }
        return best;
    }

    long long expanded_basic_cost(const string& ops, const string& macro) const {
        long long cost = 0;
        for (char op : ops) {
            cost += op == 'P' ? static_cast<int>(macro.size()) : 1;
        }
        return cost;
    }

    string encode_with_helper_macro(const string& target, const string& helper) const {
        if (helper.empty() || target.size() <= helper.size()) {
            return target;
        }
        string encoded;
        for (int index = 0; index < static_cast<int>(target.size());) {
            if (index + static_cast<int>(helper.size()) <= static_cast<int>(target.size()) &&
                target.compare(index, helper.size(), helper) == 0) {
                encoded.push_back('P');
                index += static_cast<int>(helper.size());
            } else {
                encoded.push_back(target[index]);
                ++index;
            }
        }
        return encoded.size() < target.size() ? encoded : target;
    }

    const RegistrationEncoding& registration_encoding_for_macro(const string& macro) const {
        if (registration_encoding_cache_valid && registration_encoding_cache_macro == macro) {
            return registration_encoding_cache;
        }

        RegistrationEncoding best;
        best.output_ops = static_cast<int>(macro.size()) + 2;
        best.basic_used = static_cast<int>(macro.size());

        bool has_helper = false;
        RegistrationEncoding best_helper;
        const int n_macro = static_cast<int>(macro.size());
        for (int length = 2; length <= min(REGISTER_BUILD_HELPER_MAX_LEN, n_macro / 2);
             ++length) {
            for (int left = 0; left + length <= n_macro; ++left) {
                const string helper = macro.substr(left, length);
                const string encoded = encode_with_helper_macro(macro, helper);
                if (encoded.size() >= macro.size()) {
                    continue;
                }
                const int output_ops =
                    static_cast<int>(helper.size() + encoded.size()) + 4;
                const int register_save = best.output_ops - output_ops;
                if (register_save < REGISTER_BUILD_HELPER_MIN_SAVE) {
                    continue;
                }
                const long long basic_used =
                    static_cast<long long>(helper.size() + macro.size());
                if (output_ops >= best.output_ops || output_ops > t_limit ||
                    basic_used > t_limit) {
                    continue;
                }
                if (!has_helper ||
                    pair<int, long long>{output_ops, basic_used} <
                        pair<int, long long>{best_helper.output_ops, best_helper.basic_used}) {
                    best_helper.use_helper = true;
                    best_helper.helper = helper;
                    best_helper.encoded = encoded;
                    best_helper.output_ops = output_ops;
                    best_helper.basic_used = basic_used;
                    has_helper = true;
                }
            }
        }

        registration_encoding_cache_macro = macro;
        registration_encoding_cache = has_helper ? std::move(best_helper) : std::move(best);
        registration_encoding_cache_valid = true;
        return registration_encoding_cache;
    }

    const vector<PreRegisterStart>& pre_register_start_candidates(int start_state) const {
        const int max_steps =
            env_int_or("AHC066_PRE_REGISTER_MOVE_MAX_STEPS", PRE_REGISTER_MOVE_MAX_STEPS);
        if (pre_register_cache_start == start_state &&
            pre_register_cache_max_steps == max_steps) {
            return pre_register_cache;
        }

        vector<int> dist(states, INF);
        vector<string> ops_to_state(states);
        queue<int> q;
        dist[start_state] = 0;
        q.push(start_state);
        while (!q.empty()) {
            const int state = q.front();
            q.pop();
            if (dist[state] >= max_steps) {
                continue;
            }
            for (int op = 0; op < 3; ++op) {
                const int next_state = motion_next[state][op];
                if (dist[next_state] <= dist[state] + 1) {
                    continue;
                }
                dist[next_state] = dist[state] + 1;
                ops_to_state[next_state] = ops_to_state[state] + "FRL"[op];
                q.push(next_state);
            }
        }

        vector<PreRegisterStart> reachable;
        reachable.reserve(states);
        for (int state = 0; state < states; ++state) {
            if (dist[state] <= max_steps) {
                reachable.push_back(PreRegisterStart{ops_to_state[state], state});
            }
        }

        const int start_cell = state_cell(start_state);
        sort(reachable.begin(), reachable.end(), [&](const PreRegisterStart& lhs,
                                                     const PreRegisterStart& rhs) {
            if (lhs.ops.empty() != rhs.ops.empty()) {
                return lhs.ops.empty();
            }
            const int lhs_move = cell_dist[start_cell][state_cell(lhs.state)];
            const int rhs_move = cell_dist[start_cell][state_cell(rhs.state)];
            if (lhs_move != rhs_move) {
                return lhs_move > rhs_move;
            }
            if (lhs.ops.size() != rhs.ops.size()) {
                return lhs.ops.size() < rhs.ops.size();
            }
            return lhs.ops < rhs.ops;
        });

        pre_register_cache_start = start_state;
        pre_register_cache_max_steps = max_steps;
        pre_register_cache = std::move(reachable);
        return pre_register_cache;
    }

    vector<RegistrationPlan> registration_plan_candidates(
        const string& macro,
        int start_state,
        const vector<int>* macro_effect = nullptr
    ) const {
        vector<RegistrationPlan> plans;
        const RegistrationEncoding& encoding = registration_encoding_for_macro(macro);
        RegistrationPlan direct;
        direct.prefix = "M" + macro + "M";
        direct.initial_state =
            macro_effect == nullptr ? apply_macro_state(start_state, macro)
                                    : (*macro_effect)[start_state];
        direct.output_ops = static_cast<int>(direct.prefix.size());
        direct.basic_used = static_cast<int>(macro.size());
        plans.push_back(direct);

        if (encoding.use_helper && encoding.output_ops < direct.output_ops) {
            RegistrationPlan best_helper;
            best_helper.prefix =
                "M" + encoding.helper + "M" + "M" + encoding.encoded + "M";
            const int after_helper = apply_macro_state(start_state, encoding.helper);
            best_helper.initial_state =
                macro_effect == nullptr ? apply_macro_state(after_helper, macro)
                                        : (*macro_effect)[after_helper];
            best_helper.output_ops = encoding.output_ops;
            best_helper.basic_used = encoding.basic_used;
            plans.clear();
            plans.push_back(std::move(best_helper));
        }
        return plans;
    }

    AHC066_ALWAYS_INLINE int internal_rotation_length(int rotation) const {
        return rotation == 2 ? 2 : 1;
    }

    string internal_rotation_text(int rotation) const {
        if (rotation == 0) {
            return "L";
        }
        if (rotation == 1) {
            return "R";
        }
        return "LL";
    }

    int internal_macro_length(const vector<InternalMacroBlock>& blocks) const {
        int length = 0;
        for (const InternalMacroBlock& block : blocks) {
            length += block.forward_count + internal_rotation_length(block.rotation);
        }
        return length;
    }

    string internal_macro_from_blocks(const vector<InternalMacroBlock>& blocks) const {
        string macro;
        macro.reserve(static_cast<size_t>(internal_macro_length(blocks)));
        for (const InternalMacroBlock& block : blocks) {
            macro.append(static_cast<size_t>(block.forward_count), 'F');
            macro += internal_rotation_text(block.rotation);
        }
        return macro;
    }

    uint64_t internal_macro_hash_from_blocks(const vector<InternalMacroBlock>& blocks) const {
        uint64_t hash = 1469598103934665603ULL;
        int length = 0;
        auto push_code = [&](uint64_t code) {
            hash ^= code + 1;
            hash *= 1099511628211ULL;
            ++length;
        };
        for (const InternalMacroBlock& block : blocks) {
            for (int count = 0; count < block.forward_count; ++count) {
                push_code(0);  // F
            }
            if (block.rotation == 0) {
                push_code(1);  // L
            } else if (block.rotation == 1) {
                push_code(2);  // R
            } else {
                push_code(1);
                push_code(1);
            }
        }
        hash ^= static_cast<uint64_t>(length) + 0x9E3779B97F4A7C15ULL;
        hash *= 1099511628211ULL;
        return hash;
    }

    long long internal_macro_carry_costs_impl(const vector<int>& macro_effect,
                                              vector<int>* costs_out) const {
        if (static_cast<int>(reverse_macro_head_workspace.size()) != states) {
            reverse_macro_head_workspace.assign(states, -1);
            reverse_macro_next_workspace.assign(states, -1);
        } else {
            fill(reverse_macro_head_workspace.begin(), reverse_macro_head_workspace.end(), -1);
        }
        vector<int>& reverse_macro_head = reverse_macro_head_workspace;
        vector<int>& reverse_macro_next = reverse_macro_next_workspace;
        for (int state = 0; state < states; ++state) {
            const int next_state = macro_effect[state];
            reverse_macro_next[state] = reverse_macro_head[next_state];
            reverse_macro_head[next_state] = state;
        }

        if (costs_out != nullptr) {
            costs_out->assign(tasks.size(), INF);
        }

        long long carry_sum = 0;
        bool all_reachable = true;
        if (static_cast<int>(task_done_epoch.size()) != m) {
            task_done_epoch.assign(m, 0);
            task_done_epoch_counter = 0;
        }
        ++task_done_epoch_counter;
        if (task_done_epoch_counter == INF) {
            fill(task_done_epoch.begin(), task_done_epoch.end(), 0);
            task_done_epoch_counter = 1;
        }
        for (int basket : unique_basket_cells) {
            const int epoch = next_bfs_epoch();
            bfs_queue.clear();
            for (int dir = 0; dir < 4; ++dir) {
                const int state = basket * 4 + dir;
                bfs_seen_epoch[state] = epoch;
                bfs_state_dist[state] = 0;
                bfs_queue.push_back(state);
            }

            auto push_reverse_state = [&](int state, int distance) {
                if (bfs_seen_epoch[state] == epoch) {
                    return;
                }
                bfs_seen_epoch[state] = epoch;
                bfs_state_dist[state] = distance;
                bfs_queue.push_back(state);
            };

            int remaining_tasks = static_cast<int>(task_ids_by_basket_cell[basket].size());
            for (int head = 0; head < static_cast<int>(bfs_queue.size()); ++head) {
                const int cur = bfs_queue[head];
                const int cell = state_cell(cur);
                for (int task_id : task_ids_by_ball_cell[cell]) {
                    if (task_done_epoch[task_id] == task_done_epoch_counter) {
                        continue;
                    }
                    if (task_basket_cells[task_id] != basket) {
                        continue;
                    }
                    const int cost = bfs_state_dist[cur] + 2;
                    if (costs_out != nullptr) {
                        (*costs_out)[task_id] = cost;
                    }
                    task_done_epoch[task_id] = task_done_epoch_counter;
                    carry_sum += cost;
                    --remaining_tasks;
                }
                if (remaining_tasks == 0) {
                    break;
                }

                const int distance = bfs_state_dist[cur] + 1;
                for (int op = 0; op < 3; ++op) {
                    const int prev_state = reverse_motion_prev[cur][op];
                    if (prev_state != -1) {
                        push_reverse_state(prev_state, distance);
                    }
                }

                for (int prev_state = reverse_macro_head[cur]; prev_state != -1;
                     prev_state = reverse_macro_next[prev_state]) {
                    push_reverse_state(prev_state, distance);
                }
            }
            if (remaining_tasks > 0) {
                all_reachable = false;
            }
        }

        if (!all_reachable) {
            return static_cast<long long>(INF) * INF;
        }
        return carry_sum;
    }

    long long internal_macro_carry_sum(const vector<int>& macro_effect) const {
        return internal_macro_carry_costs_impl(macro_effect, nullptr);
    }

    vector<InternalMacroBlock> initial_internal_macro_blocks(int max_macro_len) const {
        vector<InternalMacroBlock> blocks = {{6, 1}, {5, 0}, {2, 1}};
        while (internal_macro_length(blocks) > max_macro_len && blocks.size() > 1) {
            blocks.pop_back();
        }
        while (internal_macro_length(blocks) > max_macro_len && blocks.back().forward_count > 1) {
            --blocks.back().forward_count;
        }
        return blocks;
    }

    int random_rotation(mt19937& rng) const {
        return static_cast<int>(rng() % 3);
    }

    void mutate_internal_macro_blocks_inplace(vector<InternalMacroBlock>& blocks,
                                              mt19937& rng,
                                              int max_macro_len) const {
        if (blocks.empty()) {
            blocks.push_back({4, 1});
        }

        const int operation = static_cast<int>(rng() % 6);
        const int current_length = internal_macro_length(blocks);

        if (operation == 0) {
            const int index = static_cast<int>(rng() % static_cast<uint32_t>(blocks.size()));
            const int delta = 1 + static_cast<int>(rng() % 4);
            if (current_length + delta <= max_macro_len) {
                blocks[index].forward_count += delta;
            }
        } else if (operation == 1) {
            const int index = static_cast<int>(rng() % static_cast<uint32_t>(blocks.size()));
            const int delta = 1 + static_cast<int>(rng() % 4);
            blocks[index].forward_count = max(1, blocks[index].forward_count - delta);
        } else if (operation == 2) {
            const int index = static_cast<int>(rng() % static_cast<uint32_t>(blocks.size()));
            int next_rotation = random_rotation(rng);
            if (blocks[index].rotation == next_rotation) {
                next_rotation = (next_rotation + 1 + static_cast<int>(rng() % 2)) % 3;
            }
            const int next_length =
                current_length - internal_rotation_length(blocks[index].rotation) +
                internal_rotation_length(next_rotation);
            if (next_length <= max_macro_len) {
                blocks[index].rotation = next_rotation;
            }
        } else if (operation == 3) {
            const int rotation = random_rotation(rng);
            const int capacity = max_macro_len - current_length - internal_rotation_length(rotation);
            if (capacity >= 1) {
                const int forward_count = 1 + static_cast<int>(rng() % static_cast<uint32_t>(min(8, capacity)));
                const int position = static_cast<int>(rng() % static_cast<uint32_t>(blocks.size() + 1));
                blocks.insert(blocks.begin() + position, InternalMacroBlock{forward_count, rotation});
            }
        } else if (operation == 4 && blocks.size() > 1) {
            const int index = static_cast<int>(rng() % static_cast<uint32_t>(blocks.size()));
            blocks.erase(blocks.begin() + index);
        } else if (operation == 5 && blocks.size() > 1) {
            const int index = static_cast<int>(rng() % static_cast<uint32_t>(blocks.size() - 1));
            if (rng() & 1) {
                blocks[index].forward_count += blocks[index + 1].forward_count;
                blocks[index].rotation = blocks[index + 1].rotation;
                blocks.erase(blocks.begin() + index + 1);
            } else {
                const int from = index + static_cast<int>(rng() % 2);
                const int to = from == index ? index + 1 : index;
                if (blocks[from].forward_count > 1) {
                    --blocks[from].forward_count;
                    ++blocks[to].forward_count;
                }
            }
        }

    }

    long long fixed_order_direct_macro_score_from_effect(int macro_length,
                                                         const vector<int>& macro_effect,
                                                         const vector<int>& fixed_order,
                                                         int max_macro_len,
                                                         OutputCandidate* best_candidate = nullptr) const {
        if (fixed_order.empty() || macro_length <= 0 || macro_length > max_macro_len ||
            macro_length > t_limit || static_cast<int>(macro_effect.size()) != states) {
            if (best_candidate != nullptr) {
                *best_candidate = OutputCandidate{};
                best_candidate->order = fixed_order;
                best_candidate->evaluation_complete = false;
            }
            return static_cast<long long>(INF) * INF;
        }

        const int start_state = state_id(Point{0, 0}, START_DIR);
        RegistrationPlan direct;
        direct.initial_state = macro_effect[start_state];
        direct.output_ops = macro_length + 2;
        direct.basic_used = macro_length;

        OutputCandidate candidate = build_registered_macro_candidate_for_order_eval(
            macro_length,
            fixed_order,
            macro_effect,
            direct
        );
        if (!candidate.evaluation_complete) {
            if (best_candidate != nullptr) {
                *best_candidate = OutputCandidate{};
                best_candidate->order = fixed_order;
                best_candidate->evaluation_complete = false;
            }
            return static_cast<long long>(INF) * INF;
        }
        if (best_candidate != nullptr) {
            candidate.order = fixed_order;
            candidate.registration_basic_used = direct.basic_used;
            candidate.registration_initial_state = direct.initial_state;
            *best_candidate = candidate;
        }
        long long score = candidate.solved == m
                              ? static_cast<long long>(candidate.output_ops)
                              : 1LL * t_limit * (m - candidate.solved);
        score = score * 1000000LL + candidate.basic_used;
        return score;
    }

    MacroSearchResult search_internal_macro_candidate_for_order(
        int max_macro_len,
        int search_millis,
        const vector<int>& fixed_order,
        int trial_index,
        unordered_set<uint64_t>& seen_macro_keys
    ) const {
        MacroSearchResult result;
        const auto search_deadline = min(
            deadline - chrono::milliseconds(MATERIALIZE_TIME_RESERVE_MILLIS),
            Clock::now() + chrono::milliseconds(search_millis)
        );
        if (search_deadline <= Clock::now()) {
            return result;
        }
        const int min_macro_len = min(
            max_macro_len,
            max(1, env_int_or("AHC066_INTERNAL_MACRO_MIN_LEN", INTERNAL_MACRO_MIN_LEN))
        );
        mt19937 rng(
            0x3A6600C1u + static_cast<uint32_t>(
                              m * 1009 + n * 9173 + max_macro_len * 101 +
                              min_macro_len * 131 + trial_index * 1000003
                          )
        );
        uniform_real_distribution<double> unit(0.0, 1.0);

        int evaluated_macros = 0;
        int duplicate_skips = 0;
        auto evaluate_unseen = [&](const vector<InternalMacroBlock>& blocks,
                                   long long& score,
                                   string& macro,
                                   vector<int>& macro_effect_workspace,
                                   OutputCandidate& fixed_candidate) {
            const int macro_length = internal_macro_length(blocks);
            if (macro_length < min_macro_len) {
                score = static_cast<long long>(INF) * INF;
                macro.clear();
                fixed_candidate = OutputCandidate{};
                fixed_candidate.order = fixed_order;
                fixed_candidate.evaluation_complete = false;
                return false;
            }
            const uint64_t macro_key = internal_macro_hash_from_blocks(blocks);
            if (!seen_macro_keys.insert(macro_key).second) {
                ++duplicate_skips;
                return false;
            }
            ++evaluated_macros;
            build_macro_effect_from_blocks_into(blocks, macro_effect_workspace);
            macro.clear();
            score = fixed_order_direct_macro_score_from_effect(
                macro_length,
                macro_effect_workspace,
                fixed_order,
                max_macro_len,
                &fixed_candidate
            );
            return true;
        };

        vector<InternalMacroBlock> current_blocks;
        vector<int> current_effect;
        vector<int> effect_workspace;
        long long current_score = static_cast<long long>(INF) * INF;
        string current_macro;
        OutputCandidate current_fixed_candidate;
        current_fixed_candidate.evaluation_complete = false;
        bool has_current = false;
        for (int attempt = 0; attempt < 64; ++attempt) {
            vector<InternalMacroBlock> candidate_blocks =
                initial_internal_macro_blocks(max_macro_len);
            candidate_blocks.reserve(static_cast<size_t>(max_macro_len));
            const int warmup_mutations =
                (trial_index == 0 && attempt == 0)
                    ? 0
                    : 1 + static_cast<int>(rng() % static_cast<uint32_t>(8 + attempt / 4));
            for (int count = 0; count < warmup_mutations; ++count) {
                mutate_internal_macro_blocks_inplace(candidate_blocks, rng, max_macro_len);
            }
            if (evaluate_unseen(
                    candidate_blocks,
                    current_score,
                    current_macro,
                    effect_workspace,
                    current_fixed_candidate
                )) {
                current_blocks = std::move(candidate_blocks);
                if (current_macro.empty()) {
                    current_macro = internal_macro_from_blocks(current_blocks);
                }
                current_effect = effect_workspace;
                has_current = true;
                break;
            }
        }
        if (!has_current) {
            if (trace_internal_macro_lens()) {
                cerr << "main3_alternating_macro_search"
                     << " restart=" << trial_index
                     << " max_len=" << max_macro_len
                     << " iter=0"
                     << " status=no_unseen_initial"
                     << '\n';
            }
            result.evaluated_macros = evaluated_macros;
            result.duplicate_skips = duplicate_skips;
            return result;
        }
        vector<InternalMacroBlock> best_blocks = current_blocks;
        string best_macro = current_macro;
        vector<int> best_effect = current_effect;
        OutputCandidate best_fixed_candidate = current_fixed_candidate;
        long long best_score = current_score;

        const auto start_time = Clock::now();
        const double total_ms = max(
            1.0,
            static_cast<double>(
                chrono::duration_cast<chrono::milliseconds>(search_deadline - start_time).count()
            )
        );
        const bool animate_sa = animate_macro_sa();
        auto last_animation_time = start_time - chrono::milliseconds(1000);
        double progress = 0.0;
        double temperature = 16.0;
        int iteration = 0;
        int accepted = 0;
        int accepted_worse = 0;
        int improved = 0;
        auto render_macro_animation = [&](bool force) {
            if (!animate_sa) {
                return;
            }
            const auto frame_time = Clock::now();
            if (!force &&
                frame_time - last_animation_time < chrono::milliseconds(120)) {
                return;
            }
            last_animation_time = frame_time;
            const double elapsed_ms = static_cast<double>(
                chrono::duration_cast<chrono::microseconds>(frame_time - start_time).count()
            ) / 1000.0;
            const int bar_width = 32;
            const int filled = min(
                bar_width,
                max(0, static_cast<int>(progress * bar_width))
            );
            string bar(static_cast<size_t>(bar_width), '.');
            for (int index = 0; index < filled; ++index) {
                bar[index] = '#';
            }

            cerr << "\033[2J\033[H";
            cerr << "main3 fixed_order_macro_sa [" << bar << "] "
                 << static_cast<int>(progress * 100.0) << "%\n";
            cerr << "elapsed_ms=" << static_cast<int>(elapsed_ms)
                 << " iter=" << iteration
                 << " temp=" << static_cast<int>(temperature * 1000.0) / 1000.0
                 << " max_len=" << max_macro_len
                 << " best_len=" << best_macro.size()
                 << " solved=" << best_fixed_candidate.solved << "/" << m
                 << " output_ops=" << best_fixed_candidate.output_ops
                 << " basic_used=" << best_fixed_candidate.basic_used
                 << " score=" << best_score
                 << "\n";
            cerr << "best_macro=" << best_macro << "\n";
            cerr << flush;
        };
        render_macro_animation(true);
        auto now = start_time;
        vector<InternalMacroBlock> candidate_blocks;
        candidate_blocks.reserve(static_cast<size_t>(max_macro_len));
        while (true) {
            if ((iteration & 15) == 0) {
                now = Clock::now();
                const double elapsed_ms = static_cast<double>(
                    chrono::duration_cast<chrono::milliseconds>(now - start_time).count()
                );
                progress = min(1.0, elapsed_ms / total_ms);
                temperature = 16.0 * pow(0.05 / 16.0, progress);
                render_macro_animation(false);
                if (now >= search_deadline || time_exceeded()) {
                    break;
                }
            }
            candidate_blocks = current_blocks;
            const int mutation_count = 1 + static_cast<int>(rng() % 3);
            for (int count = 0; count < mutation_count; ++count) {
                mutate_internal_macro_blocks_inplace(candidate_blocks, rng, max_macro_len);
            }
            long long candidate_score = static_cast<long long>(INF) * INF;
            string candidate_macro;
            OutputCandidate candidate_fixed;
            candidate_fixed.evaluation_complete = false;
            if (!evaluate_unseen(
                    candidate_blocks,
                    candidate_score,
                    candidate_macro,
                    effect_workspace,
                    candidate_fixed
                )) {
                ++iteration;
                continue;
            }
            const long long delta = candidate_score - current_score;
            if (delta <= 0 || unit(rng) < exp(-static_cast<double>(delta) / temperature)) {
                ++accepted;
                if (delta > 0) {
                    ++accepted_worse;
                }
                if (candidate_macro.empty()) {
                    candidate_macro = internal_macro_from_blocks(candidate_blocks);
                }
                current_blocks = std::move(candidate_blocks);
                current_score = candidate_score;
                current_macro = std::move(candidate_macro);
                current_effect = effect_workspace;
                current_fixed_candidate = std::move(candidate_fixed);
            }
            if (current_score < best_score ||
                (current_score == best_score &&
                 internal_macro_length(current_blocks) < internal_macro_length(best_blocks))) {
                best_blocks = current_blocks;
                best_macro = current_macro;
                best_effect = current_effect;
                best_fixed_candidate = current_fixed_candidate;
                best_score = current_score;
                ++improved;
                render_macro_animation(true);
            }
            ++iteration;
        }

        progress = 1.0;
        render_macro_animation(true);
        const double elapsed_ms = static_cast<double>(
            chrono::duration_cast<chrono::microseconds>(Clock::now() - start_time).count()
        ) / 1000.0;
        if (trace_internal_macro_lens()) {
            const double accept_rate =
                iteration > 0 ? static_cast<double>(accepted) / iteration : 0.0;
            cerr << "main3_alternating_macro_search"
                 << " restart=" << trial_index
                 << " max_len=" << max_macro_len
                 << " iter=" << iteration
                 << " accepted=" << accepted
                 << " accept_rate=" << accept_rate
                 << " score=" << best_score
                 << " solved=" << best_fixed_candidate.solved << "/" << m
                 << " ops=" << best_fixed_candidate.output_ops
                 << " macro=" << best_macro
                 << '\n';
        }
        result.macro = std::move(best_macro);
        result.macro_effect = std::move(best_effect);
        result.fixed_candidate = std::move(best_fixed_candidate);
        if (result.fixed_candidate.macro.empty()) {
            result.fixed_candidate.macro = result.macro;
        }
        if (result.fixed_candidate.order.empty()) {
            result.fixed_candidate.order = fixed_order;
        }
        result.score = best_score;
        result.evaluated_macros = evaluated_macros;
        result.duplicate_skips = duplicate_skips;
        result.iterations = iteration;
        result.accepted = accepted;
        result.accepted_worse = accepted_worse;
        result.improved = improved;
        result.elapsed_ms = elapsed_ms;
        result.has_fixed_candidate = result.fixed_candidate.evaluation_complete;
        return result;
    }

    OutputCandidate build_registered_macro_candidate_for_order_eval(
        int macro_length,
        const vector<int>& order,
        const vector<int>& macro_effect,
        const RegistrationPlan& registration_plan
    ) const {
        OutputCandidate result;
        result.output_ops = registration_plan.output_ops;
        if (registration_plan.output_ops > t_limit || registration_plan.basic_used > t_limit) {
            return result;
        }

        int cur_state = registration_plan.initial_state;
        int output_ops = registration_plan.output_ops;
        result.basic_used = registration_plan.basic_used;
        int task_index = 0;
        for (int task_id : order) {
            if ((task_index++ & 7) == 0 && time_exceeded()) {
                result.evaluation_complete = false;
                break;
            }
            const Task& task = tasks[task_id];
            const MotionEval to_ball = shortest_motion_eval_with_macro(
                cur_state,
                cell_id(task.ball),
                macro_effect,
                macro_length
            );
            const MotionEval to_basket = shortest_motion_eval_with_macro(
                to_ball.end_state,
                cell_id(task.basket),
                macro_effect,
                macro_length
            );
            const int segment_output = to_ball.output_ops + 1 + to_basket.output_ops + 1;
            const long long segment_basic = to_ball.basic_used + 1 + to_basket.basic_used + 1;
            if (output_ops + segment_output > t_limit ||
                result.basic_used + segment_basic > t_limit) {
                break;
            }
            output_ops += segment_output;
            result.basic_used += segment_basic;
            cur_state = to_basket.end_state;
            ++result.solved;
        }
        result.output_ops = output_ops;
        return result;
    }

    OutputCandidate build_registered_macro_candidate_for_order(const string& macro,
                                                               const vector<int>& order,
                                                               const vector<int>& macro_effect,
                                                               const RegistrationPlan& registration_plan,
                                                               bool materialize_ops) const {
        OutputCandidate result;
        result.macro = macro;
        result.order = order;
        result.registration_prefix = registration_plan.prefix;
        result.registration_basic_used = registration_plan.basic_used;
        result.registration_initial_state = registration_plan.initial_state;
        result.output_ops = registration_plan.output_ops;
        if (registration_plan.output_ops > t_limit || registration_plan.basic_used > t_limit) {
            return result;
        }

        int cur_state = registration_plan.initial_state;
        int output_ops = registration_plan.output_ops;
        result.basic_used = registration_plan.basic_used;
        if (materialize_ops) {
            result.ops = registration_plan.prefix;
        }

        int task_index = 0;
        for (int task_id : order) {
            if (!materialize_ops && (task_index & 7) == 0 && time_exceeded()) {
                result.evaluation_complete = false;
                break;
            }
            ++task_index;
            const Task& task = tasks[task_id];

            if (materialize_ops) {
                const MotionPath to_ball =
                    shortest_motion_with_macro(cur_state, cell_id(task.ball), macro_effect);
                const MotionPath to_basket =
                    shortest_motion_with_macro(to_ball.end_state, cell_id(task.basket), macro_effect);
                const string segment = to_ball.ops + 'S' + to_basket.ops + 'S';
                const long long segment_basic = expanded_basic_cost(segment, macro);
                if (output_ops + static_cast<int>(segment.size()) > t_limit ||
                    result.basic_used + segment_basic > t_limit) {
                    break;
                }
                result.ops += segment;
                output_ops += static_cast<int>(segment.size());
                result.basic_used += segment_basic;
                cur_state = to_basket.end_state;
            } else {
                const MotionEval to_ball = shortest_motion_eval_with_macro(
                    cur_state,
                    cell_id(task.ball),
                    macro_effect,
                    static_cast<int>(macro.size())
                );
                const MotionEval to_basket = shortest_motion_eval_with_macro(
                    to_ball.end_state,
                    cell_id(task.basket),
                    macro_effect,
                    static_cast<int>(macro.size())
                );
                const int segment_output = to_ball.output_ops + 1 + to_basket.output_ops + 1;
                const long long segment_basic = to_ball.basic_used + 1 + to_basket.basic_used + 1;
                if (output_ops + segment_output > t_limit ||
                    result.basic_used + segment_basic > t_limit) {
                    break;
                }
                output_ops += segment_output;
                result.basic_used += segment_basic;
                cur_state = to_basket.end_state;
            }
            ++result.solved;
        }

        result.output_ops = output_ops;
        return result;
    }

    OutputCandidate build_registered_macro_candidate_with_effect(
        const string& macro,
        const vector<int>& macro_effect,
        bool materialize_ops,
        int order_search_millis = 20,
        const vector<int>* seed_order = nullptr
    ) const {
        OutputCandidate result;
        result.macro = macro;
        if (time_exceeded() || static_cast<int>(macro.size()) > t_limit ||
            static_cast<int>(macro_effect.size()) != states) {
            result.evaluation_complete = false;
            return result;
        }

        const int start_state = state_id(Point{0, 0}, START_DIR);
        const int order_output_budget = t_limit - ORDER_T_LIMIT_MARGIN;
        const vector<int> order = annealed_macro_pair_order(
            start_state,
            macro_effect,
            macro,
            order_output_budget,
            order_search_millis,
            seed_order,
            true
        );
        bool has_plan = false;
        for (const RegistrationPlan& registration_plan :
             registration_plan_candidates(macro, start_state, &macro_effect)) {
            if (time_exceeded()) {
                result.evaluation_complete = false;
                break;
            }
            OutputCandidate candidate = build_registered_macro_candidate_for_order(
                macro,
                order,
                macro_effect,
                registration_plan,
                materialize_ops
            );
            if (!has_plan || better_output_candidate(candidate, result)) {
                result = std::move(candidate);
                has_plan = true;
            }
        }
        return result;
    }

    OutputCandidate materialize_registered_macro_candidate(const OutputCandidate& draft) const {
        if (draft.macro.empty() || draft.order.empty()) {
            return draft;
        }
        const vector<int> macro_effect = build_macro_effect(draft.macro);
        RegistrationPlan base_plan;
        if (draft.registration_prefix.empty()) {
            base_plan.prefix = "M" + draft.macro + "M";
            base_plan.initial_state =
                apply_macro_state(state_id(Point{0, 0}, START_DIR), draft.macro);
        } else {
            base_plan.prefix = draft.registration_prefix;
            base_plan.initial_state = draft.registration_initial_state;
        }
        base_plan.output_ops = static_cast<int>(base_plan.prefix.size());
        base_plan.basic_used = draft.registration_basic_used == 0
                                   ? static_cast<int>(draft.macro.size())
                                   : draft.registration_basic_used;

        RegistrationPlan best_plan = base_plan;
        OutputCandidate best_eval;
        bool has_eval = false;
        auto consider_plan = [&](const RegistrationPlan& plan) {
            if (time_exceeded() || plan.output_ops > t_limit || plan.basic_used > t_limit) {
                return;
            }
            OutputCandidate candidate = build_registered_macro_candidate_for_order(
                draft.macro,
                draft.order,
                macro_effect,
                plan,
                false
            );
            if (!candidate.evaluation_complete) {
                return;
            }
            if (!has_eval || better_output_candidate(candidate, best_eval)) {
                best_eval = std::move(candidate);
                best_plan = plan;
                has_eval = true;
            }
        };

        consider_plan(base_plan);
        const int original_start = state_id(Point{0, 0}, START_DIR);
        for (const PreRegisterStart& pre_start : pre_register_start_candidates(original_start)) {
            if (time_exceeded()) {
                break;
            }
            RegistrationPlan direct;
            direct.prefix = pre_start.ops + "M" + draft.macro + "M";
            direct.initial_state = apply_macro_state(pre_start.state, draft.macro);
            direct.output_ops = static_cast<int>(direct.prefix.size());
            direct.basic_used = static_cast<long long>(pre_start.ops.size()) + draft.macro.size();
            consider_plan(direct);

            for (RegistrationPlan plan :
                 registration_plan_candidates(draft.macro, pre_start.state, &macro_effect)) {
                plan.prefix = pre_start.ops + plan.prefix;
                plan.output_ops = static_cast<int>(plan.prefix.size());
                plan.basic_used += static_cast<long long>(pre_start.ops.size());
                consider_plan(plan);
            }
        }

        return build_registered_macro_candidate_for_order(
            draft.macro,
            draft.order,
            macro_effect,
            best_plan,
            true
        );
    }

    OutputCandidate refine_registered_macro_candidate_order(const OutputCandidate& draft,
                                                            int order_search_millis) const {
        if (order_search_millis <= 0 || draft.macro.empty() || draft.order.empty() ||
            time_exceeded()) {
            return draft;
        }
        const vector<int> macro_effect = build_macro_effect(draft.macro);
        OutputCandidate refined = build_registered_macro_candidate_with_effect(
            draft.macro,
            macro_effect,
            false,
            order_search_millis,
            &draft.order
        );
        if (refined.evaluation_complete && better_output_candidate(refined, draft)) {
            return refined;
        }
        return draft;
    }

    OutputCandidate build_best_registered_macro_candidate(
        const vector<vector<int>>& fixed_orders
    ) const {
        OutputCandidate best;
        bool has_candidate = false;
        if (fixed_orders.empty()) {
            return best;
        }
        const int macro_search_millis =
            env_int_or("AHC066_INTERNAL_MACRO_SEARCH_MILLIS", INTERNAL_MACRO_SEARCH_MILLIS);
        const int order_millis =
            env_int_or("AHC066_INTERNAL_MACRO_ORDER_MILLIS", INTERNAL_MACRO_ORDER_MILLIS);
        const int final_order_millis =
            env_int_or("AHC066_FINAL_ORDER_REFINE_MILLIS", FINAL_ORDER_REFINE_MILLIS);
        const int default_max_macro_len = max(1, min(t_limit, 40));
        const int max_macro_len = max(
            1,
            min(
                t_limit,
                env_int_or("AHC066_INTERNAL_MACRO_RESTART_MAX_LEN", default_max_macro_len)
            )
        );
        const int restart_limit =
            env_int_or("AHC066_MAIN3_ALTERNATING_RESTART_LIMIT", MAIN3_ALTERNATING_RESTART_LIMIT);
        const auto alternating_deadline =
            deadline - chrono::milliseconds(
                MATERIALIZE_TIME_RESERVE_MILLIS + max(0, final_order_millis)
            );
        unordered_set<uint64_t> seen_macro_keys;
        seen_macro_keys.reserve(32768);
        vector<int> current_order = fixed_orders.front();
        for (int restart = 0; restart < restart_limit; ++restart) {
            const auto restart_time = Clock::now();
            if (restart_time >= alternating_deadline || time_exceeded()) {
                break;
            }
            if (restart < static_cast<int>(fixed_orders.size())) {
                current_order = fixed_orders[restart];
            }
            const int macro_budget = min(
                macro_search_millis,
                max(0, static_cast<int>(
                           chrono::duration_cast<chrono::milliseconds>(
                               alternating_deadline - restart_time
                           )
                               .count()
                       ))
            );
            if (macro_budget <= 0) {
                break;
            }
            MacroSearchResult search_result = search_internal_macro_candidate_for_order(
                max_macro_len,
                macro_budget,
                current_order,
                restart,
                seen_macro_keys
            );
            if (search_result.macro.empty() || time_exceeded()) {
                if (trace_internal_macro_lens()) {
                    cerr << "main3_alternating"
                         << " restart=" << restart
                         << " max_len=" << max_macro_len
                         << " macro_iter=" << search_result.iterations
                         << " status=skipped"
                         << '\n';
                }
                break;
            }
            const string& macro = search_result.macro;
            OutputCandidate fixed_candidate = search_result.fixed_candidate;
            OutputCandidate order_candidate;
            const auto before_order_time = Clock::now();
            const int order_budget = min(
                order_millis,
                max(0, static_cast<int>(
                           chrono::duration_cast<chrono::milliseconds>(
                               alternating_deadline - before_order_time
                           )
                               .count()
                       ))
            );
            if (order_budget > 0) {
                order_candidate = build_registered_macro_candidate_with_effect(
                    macro,
                    search_result.macro_effect,
                    false,
                    order_budget,
                    &current_order
                );
            } else {
                order_candidate.evaluation_complete = false;
            }
            bool has_iteration_candidate = search_result.has_fixed_candidate;
            OutputCandidate candidate = has_iteration_candidate ? fixed_candidate : order_candidate;
            bool selected_order_candidate = false;
            if (order_candidate.evaluation_complete &&
                (!has_iteration_candidate ||
                 better_output_candidate(order_candidate, fixed_candidate))) {
                candidate = std::move(order_candidate);
                has_iteration_candidate = true;
                selected_order_candidate = true;
            }
            const bool global_best_updated =
                has_iteration_candidate && candidate.evaluation_complete &&
                (!has_candidate || better_output_candidate(candidate, best));
            if (trace_internal_macro_lens()) {
                const long long candidate_score =
                    has_iteration_candidate
                        ? output_candidate_score(candidate)
                        : static_cast<long long>(INF) * INF;
                const bool has_global_best = global_best_updated || has_candidate;
                const OutputCandidate& global_best = global_best_updated ? candidate : best;
                const long long global_best_score = has_global_best
                    ? output_candidate_score(global_best)
                    : static_cast<long long>(INF) * INF;
                cerr << "main3_alternating"
                     << " restart=" << restart
                     << " max_len=" << max_macro_len
                     << " macro_iter=" << search_result.iterations
                     << " selected=" << (selected_order_candidate ? "order" : "fixed")
                     << " solved=" << candidate.solved << "/" << m
                     << " ops=" << candidate.output_ops
                     << " score=" << candidate_score
                     << " global_best_updated=" << (global_best_updated ? 1 : 0)
                     << " best_solved=" << global_best.solved << "/" << m
                     << " best_ops=" << global_best.output_ops
                     << " best_score=" << global_best_score
                     << " mode=alternating"
                     << '\n';
            }
            if (global_best_updated) {
                best = std::move(candidate);
                has_candidate = true;
            }
            if (has_iteration_candidate && candidate.evaluation_complete && !candidate.order.empty()) {
                current_order = candidate.order;
            } else if (!fixed_orders.empty()) {
                current_order = fixed_orders[(restart + 1) % fixed_orders.size()];
            }
        }

        if (has_candidate) {
            const auto before_refine_time = Clock::now();
            const int refine_budget = min(
                final_order_millis,
                max(0, static_cast<int>(
                           chrono::duration_cast<chrono::milliseconds>(
                               deadline - before_refine_time
                           )
                               .count()
                       ) - MATERIALIZE_TIME_RESERVE_MILLIS)
            );
            if (refine_budget > 0) {
                const int before_ops = best.output_ops;
                OutputCandidate refined =
                    refine_registered_macro_candidate_order(best, refine_budget);
                const bool refined_updated =
                    refined.evaluation_complete && better_output_candidate(refined, best);
                if (trace_internal_macro_lens()) {
                    cerr << "main3_final_order_refine"
                         << " budget_ms=" << refine_budget
                         << " before_ops=" << before_ops
                         << " after_ops=" << refined.output_ops
                         << " updated=" << (refined_updated ? 1 : 0)
                         << '\n';
                }
                if (refined_updated) {
                    best = std::move(refined);
                }
            }
            best = materialize_registered_macro_candidate(best);
        }
        return best;
    }

    vector<int> build_initial_order_with_seed_macro(const string& seed_macro,
                                                    int route_search_millis) const {
        const vector<int> seed_macro_effect = build_macro_effect(seed_macro);
        const int start_state = state_id(Point{0, 0}, START_DIR);
        const int registered_state = apply_macro_state(start_state, seed_macro);
        const int order_output_budget =
            t_limit - static_cast<int>(seed_macro.size()) - 2 - ORDER_T_LIMIT_MARGIN;
        return annealed_macro_pair_order(
            registered_state,
            seed_macro_effect,
            seed_macro,
            order_output_budget,
            route_search_millis
        );
    }

    vector<vector<int>> build_fixed_order_candidates() const {
        vector<vector<int>> orders;
        auto add_order = [&](vector<int> order) {
            if (order.empty()) {
                return;
            }
            for (const vector<int>& existing : orders) {
                if (existing == order) {
                    return;
                }
            }
            orders.push_back(std::move(order));
        };

        if (!time_exceeded()) {
            const int route_millis =
                env_int_or("AHC066_INITIAL_ORDER_SEARCH_MILLIS", INITIAL_ORDER_SEARCH_MILLIS);
            add_order(build_initial_order_with_seed_macro(INITIAL_ORDER_SEED_MACRO, route_millis));
        }
        return orders;
    }

    void write_output(const string& ops) const {
        for (char op : ops) {
            cout << op << '\n';
        }
    }

    AHC066_ALWAYS_INLINE int candidate_output_ops(const OutputCandidate& candidate) const {
        if (candidate.output_ops != 0 || candidate.ops.empty()) {
            return candidate.output_ops;
        }
        return static_cast<int>(candidate.ops.size());
    }

    AHC066_ALWAYS_INLINE long long output_candidate_score(const OutputCandidate& candidate) const {
        if (candidate.solved == m) {
            return static_cast<long long>(candidate_output_ops(candidate));
        }
        return 1LL * t_limit * (m - candidate.solved);
    }

    AHC066_ALWAYS_INLINE bool better_output_candidate(const OutputCandidate& lhs,
                                                     const OutputCandidate& rhs) const {
        const long long lhs_score = output_candidate_score(lhs);
        const long long rhs_score = output_candidate_score(rhs);
        if (lhs_score != rhs_score) {
            return lhs_score < rhs_score;
        }
        if (lhs.basic_used != rhs.basic_used) {
            return lhs.basic_used < rhs.basic_used;
        }
        return candidate_output_ops(lhs) < candidate_output_ops(rhs);
    }

    string build_simple_baseline() const {
        vector<int> no_macro_effect(static_cast<size_t>(states));
        iota(no_macro_effect.begin(), no_macro_effect.end(), 0);

        int current_state = state_id(Point{0, 0}, START_DIR);
        string operations;
        for (const Task& task : tasks) {
            const MotionPath to_ball = shortest_motion_with_macro(
                current_state, cell_id(task.ball), no_macro_effect
            );
            const MotionPath to_basket = shortest_motion_with_macro(
                to_ball.end_state, cell_id(task.basket), no_macro_effect
            );
            const string segment = to_ball.ops + "S" + to_basket.ops + "S";
            if (operations.size() + segment.size()
                > static_cast<size_t>(t_limit)) {
                break;
            }
            operations += segment;
            current_state = to_basket.end_state;
        }
        return operations;
    }

    void solve() {
        const int total_time_limit_millis =
            env_int_or("AHC066_TOTAL_TIME_LIMIT_MILLIS", TOTAL_TIME_LIMIT_MILLIS);
        deadline = Clock::now() + chrono::milliseconds(total_time_limit_millis);
        read_input();
#if defined(SIMPLE_BASELINE) || defined(BASELINE)
        write_output(build_simple_baseline());
        return;
#endif
        const vector<vector<int>> fixed_orders = build_fixed_order_candidates();
        OutputCandidate macro_candidate;
        if (!time_exceeded()) {
            macro_candidate = build_best_registered_macro_candidate(fixed_orders);
        }
        write_output(macro_candidate.ops);
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Solver solver;
    solver.solve();
    return 0;
}
