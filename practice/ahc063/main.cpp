// AHC063: a full-depth beam search for the colored snake.
// Matching the target prefix is the main objective.  Distance to the next
// required food is used only as a tie-break.  Similar snake shapes in the
// same depth layer are merged to spend time on different futures.

#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")
#include <bits/stdc++.h>

using namespace std;

using Clock = chrono::steady_clock;
using TimePoint = Clock::time_point;

constexpr int MAX_CELLS = 256;
constexpr int MAX_COLORS = 8;
constexpr int FOOD_WORDS = MAX_CELLS / 64;
constexpr int MAX_DIST = 30;
constexpr int BEAM_DEPTH = 3000;
constexpr int DEFAULT_BEAM_WIDTH = 1000;
constexpr int MAX_BEAM_WIDTH = 32000;
constexpr int ANSWER_LIMIT = 2500;
constexpr int ANSWER_LIMIT_N8 = 1000;
constexpr int TIME_LIMIT_MS = 1900;
constexpr int FORCED_JUNK_CHECK_DIST = 6;
constexpr int FORCED_JUNK_TARGET_COUNT = 1;

const int DR[4] = {-1, 1, 0, 0};
const int DC[4] = {0, 0, -1, 1};
const char DIR_CH[4] = {'U', 'D', 'L', 'R'};
constexpr array<int, 4> DIR_ORDER = {0, 1, 2, 3};

int N, M, C;
int beam_width = DEFAULT_BEAM_WIDTH;
vector<int> target_colors;
array<array<short, 4>, MAX_CELLS> next_cell_table{};
array<array<array<uint64_t, FOOD_WORDS>, MAX_DIST + 1>, MAX_CELLS> dist_mask{};
int max_dist_limit = 0;

struct State {
    array<unsigned char, MAX_CELLS> pos;
    array<unsigned char, MAX_CELLS> colors;
    array<array<uint64_t, FOOD_WORDS>, MAX_COLORS> food;
    array<uint64_t, FOOD_WORDS> food_occ;
    array<uint64_t, FOOD_WORDS> bite_occ_eat;
    array<uint64_t, FOOD_WORDS> bite_occ_move;
    unsigned char head_idx;
    int len;
    int matched;
};

struct BeamNode {
    State st;
    int trace_id;
    int depth;
    char move;
    unsigned char target_dist;
    uint32_t hash;
};

struct TraceNode {
    int parent;
    char move;
};

struct SolveResult {
    string answer;
    int matched;
    int len;
    int target_dist;
    bool complete;
};

inline int remaining_targets(const State& st) {
    return M - st.matched;
}

inline bool cannot_finish_within(const State& st, int used_turns, int max_answer_len) {
    return used_turns + remaining_targets(st) > max_answer_len;
}

int compute_matched(const State& st) {
    int p = 0;
    while(p < st.len && p < M && st.colors[p] == target_colors[p]) {
        p++;
    }
    return p;
}

inline void add_food(State& st, int color, int cell) {
    int word = cell >> 6;
    uint64_t bit = 1ULL << (cell & 63);
    st.food[color][word] |= bit;
    st.food_occ[word] |= bit;
}

inline void remove_food(State& st, int color, int cell) {
    int word = cell >> 6;
    uint64_t bit = 1ULL << (cell & 63);
    st.food[color][word] &= ~bit;
    st.food_occ[word] &= ~bit;
}

inline void set_mask_bit(array<uint64_t, FOOD_WORDS>& mask, int cell) {
    mask[cell >> 6] |= 1ULL << (cell & 63);
}

inline void clear_mask_bit(array<uint64_t, FOOD_WORDS>& mask, int cell) {
    mask[cell >> 6] &= ~(1ULL << (cell & 63));
}

inline int pos_slot(const State& st, int index) {
    return (static_cast<unsigned>(st.head_idx) + static_cast<unsigned>(index)) & (MAX_CELLS - 1);
}

inline unsigned char pos_at(const State& st, int index) {
    return st.pos[pos_slot(st, index)];
}

void rebuild_bite_masks(State& st) {
    st.bite_occ_eat.fill(0);
    st.bite_occ_move.fill(0);
    for(int i = 2; i <= st.len - 2; i++) {
        set_mask_bit(st.bite_occ_eat, pos_at(st, i));
    }
    for(int i = 2; i <= st.len - 3; i++) {
        set_mask_bit(st.bite_occ_move, pos_at(st, i));
    }
}

inline int food_color_at(const State& st, int cell) {
    int word = cell >> 6;
    uint64_t mask = 1ULL << (cell & 63);
    if((st.food_occ[word] & mask) == 0) return 0;
    for(int color = 1; color <= C; color++) {
        if(st.food[color][word] & mask) return color;
    }
    return 0;
}

inline long long evaluate_state(int matched, int len) {
    long long score = matched*3;
    if(len > matched) {
        score -= 1LL;
    }
    return score;
}

inline long long evaluate_state(const State& st) {
    return evaluate_state(st.matched, st.len);
}

inline uint32_t snake_hash(const State& st) {
    uint32_t head = pos_at(st, 0);
    uint32_t mid = pos_at(st, st.len >> 1);
    uint32_t tail = pos_at(st, st.len - 1);
    return head | (tail << 8) | (mid << 16);
}

inline int nearest_target_distance(const State& st) {
    if(st.matched >= M) return 0;
    int want = target_colors[st.matched];
    int head = pos_at(st, 0);
    for(int dist = 0; dist <= max_dist_limit; dist++) {
        for(int word = 0; word < FOOD_WORDS; word++) {
            if(st.food[want][word] & dist_mask[head][dist][word]) {
                return dist;
            }
        }
    }
    return MAX_CELLS;
}

inline void ensure_target_dist(BeamNode& node) {
    if(node.target_dist == numeric_limits<unsigned char>::max()) {
        node.target_dist = static_cast<unsigned char>(nearest_target_distance(node.st));
    }
}

bool better_node_light(const BeamNode& a, const BeamNode& b) {
    long long score_a = evaluate_state(a.st);
    long long score_b = evaluate_state(b.st);
    if(score_a != score_b) return score_a > score_b;
    if(a.st.matched != b.st.matched) return a.st.matched > b.st.matched;
    if(a.depth != b.depth) return a.depth < b.depth;
    return a.hash < b.hash;
}

bool better_node(const BeamNode& a, const BeamNode& b) {
    long long score_a = evaluate_state(a.st);
    long long score_b = evaluate_state(b.st);
    if(score_a != score_b) return score_a > score_b;
    if(a.st.matched != b.st.matched) return a.st.matched > b.st.matched;
    if(a.target_dist != b.target_dist) return a.target_dist < b.target_dist;
    if(a.depth != b.depth) return a.depth < b.depth;
    return a.hash < b.hash;
}

bool better_result(const SolveResult& a, const SolveResult& b) {
    if(a.complete != b.complete) return a.complete > b.complete;
    if(a.complete && b.complete && a.answer.size() != b.answer.size()) {
        return a.answer.size() < b.answer.size();
    }
    long long score_a = evaluate_state(a.matched, a.len);
    long long score_b = evaluate_state(b.matched, b.len);
    if(score_a != score_b) return score_a > score_b;
    if(a.matched != b.matched) return a.matched > b.matched;
    if(a.target_dist != b.target_dist) return a.target_dist < b.target_dist;
    if(a.len != b.len) return a.len < b.len;
    return a.answer.size() < b.answer.size();
}

inline bool time_limit_reached(const TimePoint& deadline) {
    return Clock::now() >= deadline;
}

inline int count_food_of_color(const State& st, int color) {
    int count = 0;
    for(int word = 0; word < FOOD_WORDS; word++) {
        count += __builtin_popcountll(st.food[color][word]);
    }
    return count;
}

bool has_zero_junk_path_to_target(const State& st) {
    if(st.matched >= M) return true;
    const int want = target_colors[st.matched];
    if(count_food_of_color(st, want) == 0) return false;

    array<unsigned char, MAX_CELLS> blocked{};
    for(int i = 1; i < st.len; i++) {
        blocked[pos_at(st, i)] = 1;
    }

    queue<int> q;
    array<unsigned char, MAX_CELLS> vis{};
    const int start = pos_at(st, 0);
    vis[start] = 1;
    q.push(start);
    while(!q.empty()) {
        int v = q.front();
        q.pop();
        int word = v >> 6;
        uint64_t bit = 1ULL << (v & 63);
        if(st.food[want][word] & bit) return true;
        for(int dir = 0; dir < 4; dir++) {
            int to = next_cell_table[v][dir];
            if(to < 0 || vis[to] || blocked[to]) continue;
            int to_word = to >> 6;
            uint64_t to_bit = 1ULL << (to & 63);
            if((st.food_occ[to_word] & to_bit) && !(st.food[want][to_word] & to_bit)) continue;
            vis[to] = 1;
            q.push(to);
        }
    }
    return false;
}

int min_junk_to_target(const State& st) {
    if(st.matched >= M) return 0;
    const int want = target_colors[st.matched];
    if(count_food_of_color(st, want) == 0) return MAX_CELLS;

    array<unsigned char, MAX_CELLS> blocked{};
    for(int i = 1; i < st.len; i++) {
        blocked[pos_at(st, i)] = 1;
    }

    array<int, MAX_CELLS> dist;
    dist.fill(MAX_CELLS);
    deque<int> dq;
    const int start = pos_at(st, 0);
    dist[start] = 0;
    dq.push_back(start);
    while(!dq.empty()) {
        int v = dq.front();
        dq.pop_front();
        int word = v >> 6;
        uint64_t bit = 1ULL << (v & 63);
        if(st.food[want][word] & bit) return dist[v];
        for(int dir = 0; dir < 4; dir++) {
            int to = next_cell_table[v][dir];
            if(to < 0 || blocked[to]) continue;
            int to_word = to >> 6;
            uint64_t to_bit = 1ULL << (to & 63);
            int cost = ((st.food_occ[to_word] & to_bit) && !(st.food[want][to_word] & to_bit)) ? 1 : 0;
            int nd = dist[v] + cost;
            if(nd >= dist[to]) continue;
            dist[to] = nd;
            if(cost == 0) dq.push_front(to);
            else dq.push_back(to);
        }
    }
    return MAX_CELLS;
}

inline int collect_candidate_dirs(const State& st, array<int, 3>& out) {
    int count = 0;
    int head = pos_at(st, 0);
    int neck = (st.len >= 2 ? pos_at(st, 1) : -1);
    for(int idx = 0; idx < 4; idx++) {
        int dir = DIR_ORDER[idx];
        int next_cell = next_cell_table[head][dir];
        if(next_cell < 0) continue;
        if(neck >= 0 && next_cell == neck) continue;
        out[count++] = dir;
    }
    return count;
}

bool apply_move(const State& src, int dir, State& dst) {
    int head = pos_at(src, 0);
    int next_cell = next_cell_table[head][dir];
    if(next_cell < 0 || (src.len >= 2 && next_cell == pos_at(src, 1))) {
        return false;
    }

    int src_hit_index = -1;
    uint64_t next_mask = 1ULL << (next_cell & 63);
    int next_word = next_cell >> 6;
    int eaten_color = food_color_at(src, next_cell);
    bool ate = eaten_color != 0;
    if(ate) {
        if(src.bite_occ_eat[next_word] & next_mask) {
            for(int i = 2; i <= src.len - 2; i++) {
                if(pos_at(src, i) == next_cell) {
                    src_hit_index = i;
                    break;
                }
            }
        }
    } else {
        if(src.bite_occ_move[next_word] & next_mask) {
            for(int i = 2; i <= src.len - 3; i++) {
                if(pos_at(src, i) == next_cell) {
                    src_hit_index = i;
                    break;
                }
            }
        }
    }

    dst = src;
    int next_matched = src.matched;
    if(ate) {
        int old_len = dst.len;
        dst.head_idx = static_cast<unsigned char>(src.head_idx - 1);
        dst.pos[dst.head_idx] = static_cast<unsigned char>(next_cell);
        dst.colors[old_len] = static_cast<unsigned char>(eaten_color);
        dst.len++;
        remove_food(dst, eaten_color, next_cell);
        if(next_matched == old_len && old_len < M && eaten_color == target_colors[old_len]) {
            next_matched++;
        }
    } else {
        dst.head_idx = static_cast<unsigned char>(src.head_idx - 1);
        dst.pos[dst.head_idx] = static_cast<unsigned char>(next_cell);
    }

    if(src_hit_index != -1) {
        int hit = src_hit_index + 1;
        for(int i = hit + 1; i < dst.len; i++) {
            add_food(dst, dst.colors[i], pos_at(dst, i));
        }
        dst.len = hit + 1;
        if(next_matched > dst.len) next_matched = dst.len;
        rebuild_bite_masks(dst);
    } else if(ate) {
        dst.bite_occ_eat = src.bite_occ_eat;
        dst.bite_occ_move = src.bite_occ_move;
        if(src.len >= 2) {
            int old_neck = pos_at(src, 1);
            set_mask_bit(dst.bite_occ_eat, old_neck);
            set_mask_bit(dst.bite_occ_move, old_neck);
        }
    } else {
        dst.bite_occ_eat = src.bite_occ_move;
        dst.bite_occ_move = src.bite_occ_move;
        if(src.len >= 2) {
            int old_neck = pos_at(src, 1);
            set_mask_bit(dst.bite_occ_eat, old_neck);
            set_mask_bit(dst.bite_occ_move, old_neck);
        }
        if(src.len >= 4) {
            clear_mask_bit(dst.bite_occ_move, pos_at(src, src.len - 3));
        }
    }

    dst.matched = next_matched;
    return true;
}

string reconstruct_path(const vector<TraceNode>& traces, int trace_id, int len) {
    string path(len, '?');
    while(trace_id > 0) {
        path[--len] = traces[trace_id].move;
        trace_id = traces[trace_id].parent;
    }
    return path;
}

SolveResult choose_path_by_beam(const State& start, int max_answer_len, const TimePoint& deadline) {
    if(cannot_finish_within(start, 0, max_answer_len)) {
        return {"", start.matched, start.len, nearest_target_distance(start), start.matched == M};
    }

    int search_depth_limit = min(BEAM_DEPTH, max_answer_len);
    if(search_depth_limit <= 0) {
        return {"", start.matched, start.len, nearest_target_distance(start), start.matched == M};
    }

    static thread_local vector<BeamNode> cur_layer;
    static thread_local vector<BeamNode> next_layer;
    static thread_local vector<BeamNode> cand_nodes;
    static thread_local vector<int> cand_ids;
    static thread_local vector<TraceNode> traces;
    static thread_local unordered_map<uint32_t, int> next_hash_best_id;

    cur_layer.clear();
    next_layer.clear();
    cand_nodes.clear();
    cand_ids.clear();
    traces.clear();
    next_hash_best_id.clear();
    cur_layer.reserve(beam_width);
    next_layer.reserve(beam_width);
    cand_nodes.reserve(beam_width * 3);
    cand_ids.reserve(beam_width * 3);
    traces.reserve(1 + search_depth_limit * beam_width);
    next_hash_best_id.reserve(beam_width * 3);

    traces.push_back({-1, '?'});
    cur_layer.push_back({
        start, 0, 0, '?',
        (unsigned char)nearest_target_distance(start), snake_hash(start)
    });
    BeamNode best_node = cur_layer[0];
    int best_complete_depth = max_answer_len + 1;

    auto push_candidate = [&](BeamNode&& candidate) {
        uint32_t h = candidate.hash;
        auto it = next_hash_best_id.find(h);
        if(it == next_hash_best_id.end()) {
            next_hash_best_id.emplace(h, (int)cand_nodes.size());
            cand_nodes.push_back(move(candidate));
            cand_ids.push_back((int)cand_nodes.size() - 1);
        } else {
            int best_id = it->second;
            if(better_node_light(candidate, cand_nodes[best_id])) {
                cand_nodes[best_id] = move(candidate);
            }
        }
    };

    for(int depth = 0; depth < search_depth_limit; depth++) {
        if(time_limit_reached(deadline)) break;
        if(depth + 1 >= best_complete_depth) break;
        if(cur_layer.empty()) break;

        cand_nodes.clear();
        cand_ids.clear();
        next_hash_best_id.clear();

        for(int node_index = 0; node_index < (int)cur_layer.size(); node_index++) {
            if((node_index & 63) == 0 && time_limit_reached(deadline)) break;
            const BeamNode& node = cur_layer[node_index];
            if(node.depth + remaining_targets(node.st) >= best_complete_depth) continue;

            bool forced_junk_mode = false;
            if(node.st.matched < M) {
                int target_dist = node.target_dist;
                if(target_dist == numeric_limits<unsigned char>::max()) {
                    target_dist = nearest_target_distance(node.st);
                }
                int want = target_colors[node.st.matched];
                if(target_dist >= FORCED_JUNK_CHECK_DIST &&
                   node.st.len == node.st.matched &&
                   count_food_of_color(node.st, want) <= FORCED_JUNK_TARGET_COUNT &&
                   !has_zero_junk_path_to_target(node.st)) {
                    forced_junk_mode = true;
                }
            }

            array<int, 3> candidate_dirs{};
            int dir_count = collect_candidate_dirs(node.st, candidate_dirs);
            array<BeamNode, 3> forced_candidates;
            array<int, 3> forced_costs{};
            int forced_count = 0;
            int best_forced_cost = MAX_CELLS;
            for(int idx = 0; idx < dir_count; idx++) {
                int dir = candidate_dirs[idx];
                State nxt;
                if(!apply_move(node.st, dir, nxt)) continue;

                int next_depth = node.depth + 1;
                if(cannot_finish_within(nxt, next_depth, max_answer_len)) continue;
                if(next_depth + remaining_targets(nxt) >= best_complete_depth) continue;

                BeamNode candidate{
                    move(nxt), node.trace_id, next_depth, DIR_CH[dir],
                    numeric_limits<unsigned char>::max(), 0
                };
                candidate.hash = snake_hash(candidate.st);
                if(forced_junk_mode) {
                    int forced_cost = min_junk_to_target(candidate.st);
                    best_forced_cost = min(best_forced_cost, forced_cost);
                    forced_costs[forced_count] = forced_cost;
                    forced_candidates[forced_count++] = move(candidate);
                } else {
                    push_candidate(move(candidate));
                }
            }
            if(forced_junk_mode) {
                for(int i = 0; i < forced_count; i++) {
                    if(forced_costs[i] == best_forced_cost) {
                        push_candidate(move(forced_candidates[i]));
                    }
                }
            }
        }

        if(cand_ids.empty()) break;

        if((int)cand_ids.size() > beam_width) {
            nth_element(cand_ids.begin(), cand_ids.begin() + beam_width, cand_ids.end(),
                [&](int lhs, int rhs) { return better_node_light(cand_nodes[lhs], cand_nodes[rhs]); });
            cand_ids.resize(beam_width);
        }
        for(int cand_id : cand_ids) {
            ensure_target_dist(cand_nodes[cand_id]);
        }
        sort(cand_ids.begin(), cand_ids.end(),
             [&](int lhs, int rhs) { return better_node(cand_nodes[lhs], cand_nodes[rhs]); });

        next_layer.clear();
        next_layer.reserve(cand_ids.size());
        for(int cand_id : cand_ids) {
            BeamNode node = move(cand_nodes[cand_id]);
            traces.push_back({node.trace_id, node.move});
            node.trace_id = (int)traces.size() - 1;
            if(better_node(node, best_node)) {
                best_node = node;
            }
            if(node.st.matched == M) {
                best_complete_depth = min(best_complete_depth, node.depth);
            }
            next_layer.push_back(move(node));
        }

        cur_layer.swap(next_layer);
    }

    if(best_node.depth == 0) {
        return {"", start.matched, start.len, nearest_target_distance(start), start.matched == M};
    }

    return {
        reconstruct_path(traces, best_node.trace_id, best_node.depth),
        best_node.st.matched,
        best_node.st.len,
        best_node.target_dist,
        best_node.st.matched == M
    };
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

#ifdef SIMPLE_BASELINE
    // An empty output is legal.  The initial five cells already match the
    // first five target colors, but all remaining length is penalized.
    return 0;
#endif

    const auto time_begin = Clock::now();
    const auto deadline = time_begin + chrono::milliseconds(TIME_LIMIT_MS);

    cin >> N >> M >> C;

    for(int cell = 0; cell < N * N; cell++) {
        int r = cell / N;
        int c = cell % N;
        for(int dir = 0; dir < 4; dir++) {
            int nr = r + DR[dir];
            int nc = c + DC[dir];
            if(0 <= nr && nr < N && 0 <= nc && nc < N) {
                next_cell_table[cell][dir] = static_cast<short>(nr * N + nc);
            } else {
                next_cell_table[cell][dir] = static_cast<short>(-1);
            }
        }
    }
    max_dist_limit = 2 * (N - 1);
    for(int cell = 0; cell < N * N; cell++) {
        int r = cell / N;
        int c = cell % N;
        for(int other = 0; other < N * N; other++) {
            int orow = other / N;
            int ocol = other % N;
            int dist = abs(r - orow) + abs(c - ocol);
            dist_mask[cell][dist][other >> 6] |= 1ULL << (other & 63);
        }
        for(int dist = 1; dist <= max_dist_limit; dist++) {
            for(int word = 0; word < FOOD_WORDS; word++) {
                dist_mask[cell][dist][word] |= dist_mask[cell][dist - 1][word];
            }
        }
    }

    target_colors.resize(M);
    for(int i = 0; i < M; i++) {
        cin >> target_colors[i];
    }

    State initial{};
    initial.head_idx = 0;
    initial.len = 5;
    initial.pos[0] = static_cast<unsigned char>(4 * N);
    initial.pos[1] = static_cast<unsigned char>(3 * N);
    initial.pos[2] = static_cast<unsigned char>(2 * N);
    initial.pos[3] = static_cast<unsigned char>(1 * N);
    initial.pos[4] = 0;
    for(int i = 0; i < 5; i++) initial.colors[i] = 1;

    for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {
            int x;
            cin >> x;
            if(x != 0) add_food(initial, x, i * N + j);
        }
    }

    initial.matched = compute_matched(initial);
    rebuild_bite_masks(initial);
    const int max_answer_len = (N == 8 ? ANSWER_LIMIT_N8 : ANSWER_LIMIT);

    SolveResult best{
        "", initial.matched, initial.len, nearest_target_distance(initial), initial.matched == M
    };
    int solve_runs = 0;
    for(int width = DEFAULT_BEAM_WIDTH; width <= MAX_BEAM_WIDTH; width <<= 1) {
        if(time_limit_reached(deadline)) break;
        beam_width = width;
        SolveResult cand = choose_path_by_beam(initial, max_answer_len, deadline);
        solve_runs++;
        if(better_result(cand, best)) {
            best = move(cand);
        }
    }

    for(char ch : best.answer) {
        cout << ch << '\n';
    }

    const auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(
        Clock::now() - time_begin).count();
    cerr << "N=" << N << '\n';
    cerr << "M=" << M << '\n';
    cerr << "C=" << C << '\n';
    cerr << "matched=" << best.matched << '\n';
    cerr << "len=" << best.len << '\n';
    cerr << "turns=" << best.answer.size() << '\n';
    cerr << "solve_runs=" << solve_runs << '\n';
    cerr << "last_beam_width=" << beam_width << '\n';
    cerr << "elapsed_ms=" << elapsed_ms << '\n';

    return 0;
}
