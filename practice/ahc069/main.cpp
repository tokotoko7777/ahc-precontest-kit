// AHC069 practice solver: online placement with compact shapes and limited relocation.
// Self-contained C++17: submit this one file as main.cpp.
#pragma GCC optimize "-O3,omit-frame-pointer,inline,unroll-all-loops,fast-math"
#pragma GCC target "tune=native"
// Current submission path: compact ordinary-placement candidates, packing-pressure scoring,
// and repairable target-lane relocation. Inactive experiment branches have been removed.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <string>
#include <time.h>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

constexpr int MAX_N = 50;
constexpr int MAX_P = 150;
constexpr int MAX_GROUPS = 1000;
constexpr int MAX_CELLS = MAX_N * MAX_N;
constexpr int SMALL_COMPONENT_LIMIT = 20;
constexpr int CONTACT_SCALE = 1024;
constexpr double SQRT2 = 1.4142135623730950488;
constexpr int DX[4] = {-1, 1, 0, 0};
constexpr int DY[4] = {0, 0, -1, 1};
// Experimental ordinary-placement term.  For small arrivals, prefer cells which are covered by
// few currently feasible compact rectangles for representative future group sizes.
constexpr int PACKING_PRESSURE_MAX_P = 32;
constexpr double PACKING_PRESSURE_WEIGHT = 0.15;
constexpr double PLACEMENT_CONTACT_WEIGHT = 0.045;
constexpr array<pair<int, int>, 15> PACKING_PROBE_SHAPES = {
    pair{4, 5},   pair{5, 4},   pair{6, 7},   pair{7, 6},  pair{7, 9},
    pair{9, 7},   pair{8, 10},  pair{10, 8},  pair{10, 10}, pair{10, 12},
    pair{12, 10}, pair{10, 15}, pair{15, 10}, pair{12, 13}, pair{13, 12},
};

// Absolute phase deadlines for the two-second interactive limit. Expensive relocation stops
// first, ordinary placement then becomes cheaper, and the final reserve is I/O-only.
constexpr double MOVE_START_DEADLINE = 1.52;
constexpr double MOVE_ENUM_DEADLINE = 1.62;
constexpr double MOVE_EXACT_DEADLINE = 1.645;
constexpr double MOVE_REPAIR_START_DEADLINE = 1.66;
constexpr double MOVE_REPAIR_DEADLINE = 1.70;
constexpr double NORMAL_CHEAP_MODE_TIME = 1.76;
constexpr double NORMAL_RECT_DEADLINE = 1.82;
constexpr double NORMAL_GROW_DEADLINE = 1.85;
constexpr double NORMAL_FAST_SELECT_DEADLINE = 1.87;
constexpr double NORMAL_USABILITY_DEADLINE = 1.89;
constexpr double HARD_STOP_DEADLINE = 1.92;
// Lower ten-percent quantile of V/(P*(T-S)) at theta=8000.  The instance-specific
// mean stay is compensated by theta^(-0.1) in low_value_cutoff().
constexpr double LOW_VALUE_BASE_CUTOFF = 0.21;
// Keep the relocation budget identical while spending the extra time on ordinary placement.
constexpr double MOVE_TAIL_BASE_DEADLINE = 1.86;
constexpr double INTERACTIVE_TAIL_PER_TURN = 0.00025;

constexpr size_t MOVE_ROUGH_KEEP = 512;
constexpr size_t MOVE_SPATIAL_SAMPLES = 64;
constexpr size_t MOVE_TARGET_KEEP = 16;
constexpr size_t MOVE_REPAIR_PREPOOL = 32;
constexpr size_t MOVE_REPAIR_BASE_KEEP = 8;
constexpr size_t MOVE_REPAIR_PREFIX_KEEP = 4;
constexpr size_t MOVE_REPAIR_LANE_KEEP = 4;
constexpr double MOVE_REPAIR_PROBE_BUDGET = 0.003;
constexpr size_t RELOCATION_KEEP = 12;
constexpr size_t RELOCATION_STRONG_KEEP = 8;
constexpr int RELOCATION_HASH_CAPACITY = 2048;
constexpr int COMPACT_TEMPLATE_MIN_P = 80;
constexpr int LARGE_SLOT_SIZE = 150;
constexpr int LARGE_SLOT_SEED_BUDGET = 128;
constexpr int DERIVED_SLOT_SEED_BUDGET = 24;
constexpr int ATLAS_REPACK_MAX_MOVES = 16;
constexpr int ATLAS_REPACK_BEAM_WIDTH = 64;
constexpr int ATLAS_REPACK_FINAL_KEEP = 8;
constexpr int ATLAS_REPACK_MASK_WORDS = (MAX_CELLS + 63) / 64;
constexpr double ATLAS_REPACK_Z_TRIGGER = 1.50;
constexpr int LIMITED_REPACK_SECONDARY_KEEP = 2;
constexpr int LIMITED_REPACK_PARENT_KEEP = 2;
constexpr double LIMITED_REPACK_LOCAL_BUDGET = 0.012;
constexpr double LIMITED_REPACK_TOTAL_BUDGET = 0.10;
constexpr double LIMITED_REPACK_CHILD_RESERVE = 0.004;
constexpr double LIMITED_REPACK_MIN_START_BUDGET = 0.006;
constexpr int LIMITED_REPACK_TERTIARY_KEEP = 1;
constexpr double LIMITED_REPACK_DEPTH3_EXTRA_BUDGET = 0.008;
constexpr double LIMITED_REPACK_DEPTH3_TOTAL_BUDGET = 0.04;
constexpr double LIMITED_REPACK_DEPTH3_FINAL_RESERVE = 0.003;

static_assert(MOVE_START_DEADLINE < MOVE_ENUM_DEADLINE);
static_assert(MOVE_ENUM_DEADLINE < MOVE_EXACT_DEADLINE);
static_assert(MOVE_EXACT_DEADLINE < MOVE_REPAIR_START_DEADLINE);
static_assert(MOVE_REPAIR_START_DEADLINE < MOVE_REPAIR_DEADLINE);
static_assert(NORMAL_CHEAP_MODE_TIME < NORMAL_RECT_DEADLINE);
static_assert(NORMAL_RECT_DEADLINE < NORMAL_GROW_DEADLINE);
static_assert(NORMAL_GROW_DEADLINE < NORMAL_FAST_SELECT_DEADLINE);
static_assert(NORMAL_FAST_SELECT_DEADLINE < NORMAL_USABILITY_DEADLINE);
static_assert(NORMAL_USABILITY_DEADLINE < HARD_STOP_DEADLINE);
static_assert(MOVE_REPAIR_DEADLINE < HARD_STOP_DEADLINE);
static_assert(MOVE_REPAIR_DEADLINE < MOVE_TAIL_BASE_DEADLINE);
static_assert(MOVE_TAIL_BASE_DEADLINE <= HARD_STOP_DEADLINE);
static_assert(RELOCATION_STRONG_KEEP < RELOCATION_KEEP);
static_assert((RELOCATION_HASH_CAPACITY & (RELOCATION_HASH_CAPACITY - 1)) == 0);
static_assert(ATLAS_REPACK_MAX_MOVES > 3);
static_assert(ATLAS_REPACK_MASK_WORDS == 40);

static uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

struct Group {
    long long s = 0;
    long long t = 0;
    long long v = 0;
    int p = 0;
    int max_perimeter = 0;
    long long cached_move_cost = 1;
    int cached_move_tier = 0;
    bool active = false;
    vector<int> cells;
};

struct Placement {
    vector<int> cells;
    int perimeter = 0;
    int weighted_contact = 0;
    double compactness = 0.0;
    double preliminary = -1e100;
    double future_usability = 0.0;
    double final_value = -1e100;
    double packing_pressure = 0.0;
    uint64_t hash = 0;
    bool pond_hole = false;
    bool forced_small_component = false;
    unsigned char compactness_lane = 0;

    bool valid() const { return !cells.empty(); }
};

struct RawRect {
    int x = 0;
    int y = 0;
    int h = 0;
    int w = 0;
    int slack = 0;
    int pond_count = 0;
    // -1 means that the whole bounding rectangle is free, so all corner variants are possible.
    int variant = -1;
    double score = -1e100;
};


struct MoveAction {
    int group_id = -1;
    Placement placement;
};

struct MovePlan {
    vector<MoveAction> moves;
    Placement arriving;
    long long total_move_cost = 0;
    long long total_fee_loss = 0;
    double value = -1e100;
    bool limited_repack = false;
    int limited_repack_depth = 0;

    bool valid() const { return arriving.valid() && !moves.empty(); }
};


struct AtlasBeamState {
    array<uint64_t, ATLAS_REPACK_MASK_WORDS> used{};
    array<int, ATLAS_REPACK_MAX_MOVES> choice{};
    long long fee_loss = 0;
    double placement_quality = 0.0;

    AtlasBeamState() { choice.fill(-1); }
};

struct RawMoveTarget {
    RawRect raw;
    int variant = 0;
    int occupied_cells = 0;
    double approximate_value = -1e100;
};

struct MoveTargetCandidate {
    Placement arriving;
    vector<int> blockers;
    double upper_value = -1e100;
    bool atlas = false;
};

struct RelocationCandidate {
    Placement placement;
    long long fee_loss = 0;
};

class Solver {
  public:
    inline __attribute__((always_inline, hot)) void run() {
        ios::sync_with_stdio(false);
        cin.tie(nullptr);
        start_cpu_time_ = process_cpu_time();

        double r_input;
        cin >> n_ >> m_ >> r_input;
        r_milli_ = static_cast<int>(llround(r_input * 1000.0));
        for (int p = 1; p <= MAX_P; ++p) {
            payment_scale_[p] = 4.0L * sqrt(static_cast<long double>(p));
        }
        park_.resize(n_);
        for (string &row : park_) cin >> row;

#if defined(SIMPLE_BASELINE) || defined(BASELINE)
        // Legal interactive baseline: reject every arriving group.
        for (int turn = 0; turn < m_; ++turn) {
            int i, p;
            long long s, t, v;
            cin >> i >> s >> t >> p >> v;
            cout << 0 << '\n' << "No\n";
            cout.flush();
        }
        return;
#endif

        close_three_sided_pond_pockets();

        owner_.fill(-2);
        grass_count_ = 0;
        for (int x = 0; x < n_; ++x) {
            for (int y = 0; y < n_; ++y) {
                const int c = id(x, y);
                if (park_[x][y] == '.') {
                    owner_[c] = -1;
                    ++grass_count_;
                }
            }
        }
        build_pond_prefix();
        build_large_slot_templates();
        usable_grass_ = compute_static_usable_grass();
        groups_.resize(m_);
        bool emergency_mode = false;

        for (int turn = 0; turn < m_; ++turn) {
            int i, p;
            long long s, t, v;
            cin >> i >> s >> t >> p >> v;

            if (emergency_mode || elapsed() > HARD_STOP_DEADLINE) {
                emergency_mode = true;
                cout << 0 << '\n' << "No\n";
                cout.flush();
                continue;
            }

            release_before(s);

            Group g;
            g.s = s;
            g.t = t;
            g.p = p;
            g.v = v;
            g.cached_move_cost = movement_cost_from_value(v);
            g.cached_move_tier = movement_cost_tier_from_cost(g.cached_move_cost);
            groups_[i] = g;

            const long long duration = t - s;
            sum_duration_ += static_cast<long double>(duration);
            sum_p_duration_ += static_cast<long double>(p) * duration;
            ++observed_;
            set_contact_target(t);

            // The low-value gate is intentionally checked after rebuilding the free components:
            // a group which can consume the smallest initially isolated island bypasses every
            // price threshold and is placed there unconditionally.
            prepare_free_state();
            Placement forced_placement;
            forced_placement = find_small_component_placement(p);
            const bool force_small_component = forced_placement.valid();
            const bool low_value = turn + 1 < m_ && is_low_value(g);

            // This gate depends only on the known generator distribution, not on the board.
            // Run it before placement generation so rejected low-value groups cost almost no CPU.
            // The last arrival is exempt because it has no future opportunity cost.
            if (!force_small_component && low_value) {
                cout << 0 << '\n' << "No\n";
                cout.flush();
                continue;
            }

            if (elapsed() > HARD_STOP_DEADLINE) {
                emergency_mode = true;
                cout << 0 << '\n' << "No\n";
                cout.flush();
                continue;
            }

            Placement placement = force_small_component
                                      ? move(forced_placement)
                                      : find_placement(p, true);
            const double threshold = force_small_component ? 0.0 : acceptance_threshold(turn);
            const double base_opportunity =
                threshold * static_cast<double>(p) * static_cast<double>(duration);
            const double opportunity = base_opportunity;
            const bool ordinary_accept =
                placement.valid() &&
                (force_small_component || should_accept(g, placement, threshold));
            bool accept = ordinary_accept;
            if (accept && !validate_region(placement.cells, p)) accept = false;

            double baseline_value = 0.0;
            if (accept) {
                baseline_value = max(
                    0.0, static_cast<double>(round_payment(v, p, placement.perimeter)) -
                             opportunity);
            }

            MovePlan move_plan;
            if (!force_small_component && elapsed() < MOVE_START_DEADLINE) {
                move_plan = find_move_plan(
                    i, g, opportunity, baseline_value, placement, accept);
            }

            if (move_plan.valid() && validate_move_plan(move_plan, i, g)) {
                cout << move_plan.moves.size() << '\n';
                for (const MoveAction &action : move_plan.moves) {
                    cout << action.group_id << '\n';
                    for (int c : action.placement.cells) {
                        cout << c / n_ << ' ' << c % n_ << '\n';
                    }
                }
                cout << "Yes\n";
                for (int c : move_plan.arriving.cells) {
                    cout << c / n_ << ' ' << c % n_ << '\n';
                }
                commit_move_plan(move_plan, i, t);
            } else {
                cout << 0 << '\n';
                if (!accept) {
                    cout << "No\n";
                } else {
                    cout << "Yes\n";
                    for (int c : placement.cells) {
                        cout << c / n_ << ' ' << c % n_ << '\n';
                        owner_[c] = i;
                    }
                    groups_[i].active = true;
                    groups_[i].max_perimeter = placement.perimeter;
                    groups_[i].cells = move(placement.cells);
                    departures_.push({t, i});
                }
            }
            cout.flush();
        }
    }

  private:
    int n_ = 0;
    int m_ = 0;
    int r_milli_ = 0;
    int grass_count_ = 0;
    int usable_grass_ = 0;
    int free_count_ = 0;
    vector<string> park_;
    array<int, MAX_CELLS> owner_{};
    vector<Group> groups_;
    priority_queue<pair<long long, int>, vector<pair<long long, int>>, greater<pair<long long, int>>>
        departures_;

    int blocked_prefix_[MAX_N + 1][MAX_N + 1]{};
    int contact_prefix_[MAX_N + 1][MAX_N + 1]{};
    double packing_pressure_diff_[MAX_N + 1][MAX_N + 1]{};
    double packing_pressure_prefix_[MAX_N + 1][MAX_N + 1]{};
    array<double, MAX_CELLS> packing_pressure_cell_{};
    bool packing_pressure_ready_ = false;
    int pond_prefix_[MAX_N + 1][MAX_N + 1]{};
    int occupied_prefix_[MAX_N + 1][MAX_N + 1]{};
    long double move_weight_prefix_[MAX_N + 1][MAX_N + 1]{};
    array<int, MAX_CELLS> component_{};
    array<int, MAX_CELLS> component_queue_{};
    vector<vector<int>> component_cells_;
    int component_count_ = 0;
    vector<vector<int>> initial_small_component_cells_;
    vector<Placement> large_slots_;
    array<vector<Placement>, MAX_P + 1> compact_slot_templates_;
    double free_component_value_ = 0.0;
    bool free_component_value_valid_ = false;

    array<int, MAX_CELLS> region_mark_{};
    array<int, MAX_CELLS> seen_mark_{};
    array<int, MAX_CELLS> grow_mark_{};
    array<int, MAX_CELLS> trim_mark_{};
    int region_token_ = 0;
    int seen_token_ = 0;
    int grow_token_ = 0;
    int trim_token_ = 0;
    array<int, MAX_CELLS> atlas_mark_{};
    int atlas_token_ = 0;
    array<int, MAX_GROUPS> limited_group_seen_{};
    array<int, MAX_GROUPS> limited_boundary_contact_{};
    int limited_group_token_ = 0;
    array<int, MAX_CELLS> limited_component_seen_{};
    int limited_component_token_ = 0;
    array<int, MAX_CELLS> limited_victim_cell_seen_{};
    int limited_victim_cell_token_ = 0;
    array<int, MAX_CELLS> limited_tertiary_cell_seen_{};
    int limited_tertiary_cell_token_ = 0;
    array<uint64_t, RELOCATION_HASH_CAPACITY> relocation_hash_key_{};
    array<int, RELOCATION_HASH_CAPACITY> relocation_hash_stamp_{};
    int relocation_hash_token_ = 0;

    long double sum_duration_ = 0.0L;
    long double sum_p_duration_ = 0.0L;
    int observed_ = 0;
    long long contact_target_t_ = 0;
    int contact_theta_ = 5000;
    int contact_theta_observed_ = -1;
    array<long double, MAX_P + 1> payment_scale_{};
    mutable array<int, MAX_GROUPS> contact_units_cache_{};
    mutable array<int, MAX_GROUPS> contact_units_stamp_{};
    int contact_cache_token_ = 1;
    double start_cpu_time_ = 0.0;
    double limited_repack_cpu_spent_ = 0.0;
    double limited_repack_depth3_cpu_spent_ = 0.0;

    int id(int x, int y) const { return x * n_ + y; }

    bool inside(int x, int y) const { return 0 <= x && x < n_ && 0 <= y && y < n_; }

    void close_three_sided_pond_pockets() {
        array<unsigned char, MAX_CELLS> pond_neighbors{};
        array<int, MAX_CELLS> queue{};
        int head = 0;
        int tail = 0;

        // Outside the board is not a pond.  Seed every grass cell surrounded by
        // at least three actual ponds, then propagate newly closed cells.
        for (int x = 0; x < n_; ++x) {
            for (int y = 0; y < n_; ++y) {
                if (park_[x][y] != '.') continue;
                const int c = id(x, y);
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + DX[d], ny = y + DY[d];
                    if (inside(nx, ny) && park_[nx][ny] == '#') {
                        ++pond_neighbors[c];
                    }
                }
                if (pond_neighbors[c] >= 3) queue[tail++] = c;
            }
        }

        while (head < tail) {
            const int c = queue[head++];
            const int x = c / n_, y = c % n_;
            if (park_[x][y] != '.') continue;
            park_[x][y] = '#';
            for (int d = 0; d < 4; ++d) {
                const int nx = x + DX[d], ny = y + DY[d];
                if (!inside(nx, ny) || park_[nx][ny] != '.') continue;
                const int nc = id(nx, ny);
                ++pond_neighbors[nc];
                if (pond_neighbors[nc] == 3) queue[tail++] = nc;
            }
        }
    }

    static double process_cpu_time() {
        timespec now{};
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
        return static_cast<double>(now.tv_sec) + 1e-9 * static_cast<double>(now.tv_nsec);
    }

    double elapsed() const {
        return process_cpu_time() - start_cpu_time_;
    }

    long long round_payment(long long v, int p, int perimeter) const {
        if (perimeter <= 0 || p <= 0) return 0;
        const __int128_t vi = static_cast<__int128_t>(v);
        const __int128_t squared = 64 * vi * vi * static_cast<__int128_t>(p);
        const __int128_t li = static_cast<__int128_t>(perimeter);
        auto lower_ok = [&](long long answer) {
            const __int128_t t = (2 * static_cast<__int128_t>(answer) - 1) * li;
            return t <= 0 || t * t <= squared;
        };
        auto upper_ok = [&](long long answer) {
            const __int128_t t = (2 * static_cast<__int128_t>(answer) + 1) * li;
            return squared < t * t;
        };

        long long answer = llround(static_cast<long double>(v) * payment_scale_[p] /
                                   perimeter);
        answer = max(answer, 0LL);
        while (!lower_ok(answer)) --answer;
        while (!upper_ok(answer)) ++answer;
        return answer;
    }

    long long movement_cost_from_value(long long value) const {
        const __int128_t numerator =
            2 * static_cast<__int128_t>(value) * r_milli_ + 1000;
        return max(static_cast<long long>(numerator / 2000), 1LL);
    }

    static int movement_cost_tier_from_cost(long long cost) {
        return 63 - __builtin_clzll(static_cast<unsigned long long>(cost));
    }

    long long movement_cost(const Group &group) const { return group.cached_move_cost; }

    long long move_cost(int group_id) const {
        return groups_[group_id].cached_move_cost;
    }

    int movement_cost_tier(const Group &group) const {
        return group.cached_move_tier;
    }

    void reset_relocation_hash() {
        if (++relocation_hash_token_ == numeric_limits<int>::max()) {
            relocation_hash_stamp_.fill(0);
            relocation_hash_token_ = 1;
        }
    }

    bool insert_relocation_hash(uint64_t hash) {
        int slot = static_cast<int>(hash & (RELOCATION_HASH_CAPACITY - 1));
        for (int probe = 0; probe < RELOCATION_HASH_CAPACITY; ++probe) {
            if (relocation_hash_stamp_[slot] != relocation_hash_token_) {
                relocation_hash_stamp_[slot] = relocation_hash_token_;
                relocation_hash_key_[slot] = hash;
                return true;
            }
            if (relocation_hash_key_[slot] == hash) return false;
            slot = (slot + 1) & (RELOCATION_HASH_CAPACITY - 1);
        }
        // The generated pool is below this capacity; retain legality if future widths grow.
        return true;
    }

    template <class T>
    T prefix_sum_in_rect(const T (&prefix)[MAX_N + 1][MAX_N + 1], int x1, int y1,
                         int x2, int y2) const {
        if (x1 >= x2 || y1 >= y2) return T{};
        return prefix[x2][y2] - prefix[x1][y2] - prefix[x2][y1] + prefix[x1][y1];
    }

    void removed_strip_bounds(int x, int y, int h, int w, int slack, int variant,
                              int &x1, int &y1, int &x2, int &y2) const {
        const bool top = variant == 1 || variant == 3;
        const bool left = variant == 2 || variant == 3;
        x1 = top ? x : x + h - slack;
        x2 = x1 + slack;
        y1 = left ? y : y + w - 1;
        y2 = y1 + 1;
    }

    template <class T>
    T removed_strip_sum(const T (&prefix)[MAX_N + 1][MAX_N + 1], const RawRect &raw,
                        int variant) const {
        if (raw.slack == 0) return T{};
        int x1, y1, x2, y2;
        removed_strip_bounds(raw.x, raw.y, raw.h, raw.w, raw.slack, variant, x1, y1,
                             x2, y2);
        return prefix_sum_in_rect(prefix, x1, y1, x2, y2);
    }

    void build_pond_prefix() {
        for (int x = 0; x <= n_; ++x) {
            for (int y = 0; y <= n_; ++y) pond_prefix_[x][y] = 0;
        }
        for (int x = 0; x < n_; ++x) {
            for (int y = 0; y < n_; ++y) {
                const int pond = park_[x][y] == '#' ? 1 : 0;
                pond_prefix_[x + 1][y + 1] = pond + pond_prefix_[x][y + 1] +
                                                pond_prefix_[x + 1][y] -
                                                pond_prefix_[x][y];
            }
        }
    }

    int atlas_growth_key(int c, int seed,
                         const array<unsigned char, MAX_CELLS> &available) const {
        const int x = c / n_, y = c % n_;
        int selected_neighbors = 0;
        int packed_neighbors = 0;
        for (int d = 0; d < 4; ++d) {
            const int nx = x + DX[d], ny = y + DY[d];
            if (!inside(nx, ny)) {
                ++packed_neighbors;
                continue;
            }
            const int nc = id(nx, ny);
            if (atlas_mark_[nc] == atlas_token_) {
                ++selected_neighbors;
            } else if (!available[nc]
            ) {
                ++packed_neighbors;
            }
        }
        const int sx = seed / n_, sy = seed % n_;
        const int distance = abs(x - sx) + abs(y - sy);
        const int tie = static_cast<int>(
            splitmix64(static_cast<uint64_t>(c) * 2503ULL + seed) & 31ULL);
        return selected_neighbors * 100000 + packed_neighbors * 16 - distance * 32 + tie;
    }

    Placement grow_atlas_region(int seed, int p,
                                const array<unsigned char, MAX_CELLS> &available) {
        Placement result;
        if (seed < 0 || seed >= n_ * n_ || !available[seed]) return result;
        ++atlas_token_;
        if (atlas_token_ == numeric_limits<int>::max()) {
            atlas_mark_.fill(0);
            atlas_token_ = 1;
        }
        priority_queue<pair<int, int>> frontier;
        result.cells.reserve(p);

        auto select_cell = [&](int c) {
            atlas_mark_[c] = atlas_token_;
            result.cells.push_back(c);
            const int x = c / n_, y = c % n_;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + DX[d], ny = y + DY[d];
                if (!inside(nx, ny)) continue;
                const int nc = id(nx, ny);
                if (available[nc] && atlas_mark_[nc] != atlas_token_) {
                    frontier.push({atlas_growth_key(nc, seed, available), nc});
                }
            }
        };

        select_cell(seed);
        while (static_cast<int>(result.cells.size()) < p) {
            if (frontier.empty()) return Placement{};
            const auto [stored_key, c] = frontier.top();
            frontier.pop();
            if (!available[c] || atlas_mark_[c] == atlas_token_) continue;
            const int current_key = atlas_growth_key(c, seed, available);
            if (stored_key != current_key) {
                frontier.push({current_key, c});
                continue;
            }
            select_cell(c);
        }
        if (!measure(result)) return Placement{};
        return result;
    }

    int atlas_packing_contact(const Placement &placement,
                              const array<unsigned char, MAX_CELLS> &available) const {
        int contact = 0;
        for (int c : placement.cells) {
            const int x = c / n_, y = c % n_;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + DX[d], ny = y + DY[d];
                if (!inside(nx, ny)) {
                    ++contact;
                    continue;
                }
                const int nc = id(nx, ny);
                if (!available[nc]) {
                    ++contact;
                }
            }
        }
        return contact;
    }

    vector<int> atlas_seeds(const array<unsigned char, MAX_CELLS> &available,
                            int p, int budget) const {
        array<unsigned char, MAX_CELLS> seen{};
        vector<vector<int>> components;
        vector<int> queue;
        queue.reserve(MAX_CELLS);
        int eligible_cells = 0;
        for (int start = 0; start < n_ * n_; ++start) {
            if (!available[start] || seen[start]) continue;
            queue.clear();
            queue.push_back(start);
            seen[start] = 1;
            for (size_t head = 0; head < queue.size(); ++head) {
                const int c = queue[head];
                const int x = c / n_, y = c % n_;
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + DX[d], ny = y + DY[d];
                    if (!inside(nx, ny)) continue;
                    const int nc = id(nx, ny);
                    if (available[nc] && !seen[nc]) {
                        seen[nc] = 1;
                        queue.push_back(nc);
                    }
                }
            }
            if (static_cast<int>(queue.size()) >= p) {
                eligible_cells += static_cast<int>(queue.size());
                components.push_back(queue);
            }
        }

        vector<int> seeds;
        if (eligible_cells == 0) return seeds;
        for (const vector<int> &component : components) {
            const int count = max(
                1, static_cast<int>(static_cast<long long>(budget) * component.size() /
                                    eligible_cells));
            for (int k = 0; k < count; ++k) {
                const size_t index =
                    static_cast<size_t>(k) * component.size() / count;
                seeds.push_back(component[index]);
            }
        }
        return seeds;
    }

    Placement best_atlas_region(const array<unsigned char, MAX_CELLS> &available,
                                int p, int seed_budget) {
        Placement best;
        int best_packing_contact = -1;
        const vector<int> seeds = atlas_seeds(available, p, seed_budget);
        for (int seed : seeds) {
            Placement candidate = grow_atlas_region(seed, p, available);
            if (!candidate.valid()) continue;
            const int packing_contact = atlas_packing_contact(candidate, available);
            if (!best.valid() || candidate.perimeter < best.perimeter ||
                (candidate.perimeter == best.perimeter &&
                 packing_contact > best_packing_contact) ||
                (candidate.perimeter == best.perimeter &&
                 packing_contact == best_packing_contact &&
                 candidate.hash < best.hash)) {
                best = move(candidate);
                best_packing_contact = packing_contact;
            }
        }
        return best;
    }

    void build_large_slot_templates() {
        array<unsigned char, MAX_CELLS> available{};
        for (int c = 0; c < n_ * n_; ++c) available[c] = owner_[c] == -1;

        while (true) {
            Placement slot =
                best_atlas_region(available, LARGE_SLOT_SIZE, LARGE_SLOT_SEED_BUDGET);
            if (!slot.valid()) break;
            for (int c : slot.cells) available[c] = 0;
            large_slots_.push_back(move(slot));
        }

        for (int p = COMPACT_TEMPLATE_MIN_P; p <= MAX_P; ++p) {
            vector<Placement> &templates = compact_slot_templates_[p];
            templates.reserve(large_slots_.size());
            for (const Placement &slot : large_slots_) {
                if (p == LARGE_SLOT_SIZE) {
                    templates.push_back(slot);
                    continue;
                }
                array<unsigned char, MAX_CELLS> inside_slot{};
                for (int c : slot.cells) inside_slot[c] = 1;
                Placement derived =
                    best_atlas_region(inside_slot, p, DERIVED_SLOT_SEED_BUDGET);
                if (derived.valid()) templates.push_back(move(derived));
            }
        }
    }

    void build_move_prefixes() {
        for (int x = 0; x <= n_; ++x) {
            for (int y = 0; y <= n_; ++y) {
                occupied_prefix_[x][y] = 0;
                move_weight_prefix_[x][y] = 0.0L;
            }
        }
        for (int x = 0; x < n_; ++x) {
            for (int y = 0; y < n_; ++y) {
                const int group_id = owner_[id(x, y)];
                const int occupied = group_id >= 0 ? 1 : 0;
                long double weight = 0.0L;
                if (group_id >= 0 && groups_[group_id].active) {
                    weight = static_cast<long double>(move_cost(group_id)) /
                             max(groups_[group_id].p, 1);
                }
                occupied_prefix_[x + 1][y + 1] =
                    occupied + occupied_prefix_[x][y + 1] + occupied_prefix_[x + 1][y] -
                    occupied_prefix_[x][y];
                move_weight_prefix_[x + 1][y + 1] =
                    weight + move_weight_prefix_[x][y + 1] +
                    move_weight_prefix_[x + 1][y] - move_weight_prefix_[x][y];
            }
        }
    }

    void release_before(long long s) {
        while (!departures_.empty() && departures_.top().first < s) {
            const int i = departures_.top().second;
            departures_.pop();
            if (!groups_[i].active) continue;
            for (int c : groups_[i].cells) {
                if (owner_[c] == i) owner_[c] = -1;
            }
            groups_[i].cells.clear();
            groups_[i].active = false;
        }
    }

    int compute_static_usable_grass() {
        array<unsigned char, MAX_CELLS> seen{};
        int usable = 0;
        vector<int> stack;
        stack.reserve(MAX_CELLS);
        initial_small_component_cells_.clear();
        for (int start = 0; start < n_ * n_; ++start) {
            if (owner_[start] != -1 || seen[start]) continue;
            seen[start] = 1;
            stack.clear();
            stack.push_back(start);
            int size = 0;
            vector<int> cells;
            while (!stack.empty()) {
                const int c = stack.back();
                stack.pop_back();
                ++size;
                cells.push_back(c);
                const int x = c / n_, y = c % n_;
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + DX[d], ny = y + DY[d];
                    if (!inside(nx, ny)) continue;
                    const int nc = id(nx, ny);
                    if (owner_[nc] == -1 && !seen[nc]) {
                        seen[nc] = 1;
                        stack.push_back(nc);
                    }
                }
            }
            if (size >= 4) usable += size;
            if (4 <= size && size <= SMALL_COMPONENT_LIMIT) {
                initial_small_component_cells_.push_back(move(cells));
            }
        }
        return max(usable, 1);
    }

    void set_contact_target(long long departure_time) {
        int theta = contact_theta_;
        if (contact_theta_observed_ != observed_) {
            constexpr long double PRIOR = 16.0L;
            const long double estimate =
                (sum_duration_ + PRIOR * 5000.0L) / (observed_ + PRIOR);
            theta =
                static_cast<int>(llround(clamp(estimate, 2000.0L, 8000.0L)));
            contact_theta_observed_ = observed_;
        }
        if (contact_target_t_ == departure_time && contact_theta_ == theta) return;
        contact_target_t_ = departure_time;
        contact_theta_ = theta;
        if (++contact_cache_token_ == numeric_limits<int>::max()) {
            contact_units_stamp_.fill(0);
            contact_cache_token_ = 1;
        }
    }

    int contact_units_of_cell(int c) const {
        const int group_id = owner_[c];
        if (group_id == -1) return 0;
        if (group_id < 0) {
            return CONTACT_SCALE;
        }
        if (group_id >= m_ || groups_[group_id].t <= 0) return CONTACT_SCALE;
        if (contact_units_stamp_[group_id] == contact_cache_token_) {
            return contact_units_cache_[group_id];
        }
        const long long other_t = groups_[group_id].t;
        const long long delta = other_t >= contact_target_t_
                                    ? other_t - contact_target_t_
                                    : contact_target_t_ - other_t;
        const long long denominator = static_cast<long long>(contact_theta_) + delta;
        const int units =
            static_cast<int>((static_cast<long long>(CONTACT_SCALE) * contact_theta_ +
                              denominator / 2) /
                             max(denominator, 1LL));
        contact_units_stamp_[group_id] = contact_cache_token_;
        contact_units_cache_[group_id] = units;
        return units;
    }

    void rebuild_contact_prefix() {
        for (int y = 0; y <= n_; ++y) contact_prefix_[0][y] = 0;
        for (int x = 1; x <= n_; ++x) contact_prefix_[x][0] = 0;
        for (int x = 0; x < n_; ++x) {
            int row_contact = 0;
            for (int y = 0; y < n_; ++y) {
                row_contact += contact_units_of_cell(id(x, y));
                contact_prefix_[x + 1][y + 1] =
                    contact_prefix_[x][y + 1] + row_contact;
            }
        }
    }

    void prepare_free_state(bool with_contact = true) {
        for (int y = 0; y <= n_; ++y) blocked_prefix_[0][y] = 0;
        for (int x = 1; x <= n_; ++x) blocked_prefix_[x][0] = 0;
        if (with_contact) {
            for (int y = 0; y <= n_; ++y) contact_prefix_[0][y] = 0;
            for (int x = 1; x <= n_; ++x) contact_prefix_[x][0] = 0;
        }
        free_count_ = 0;
        for (int x = 0; x < n_; ++x) {
            int row_blocked = 0;
            int row_contact = 0;
            for (int y = 0; y < n_; ++y) {
                const int blocked = owner_[id(x, y)] == -1 ? 0 : 1;
                if (!blocked) ++free_count_;
                row_blocked += blocked;
                blocked_prefix_[x + 1][y + 1] =
                    blocked_prefix_[x][y + 1] + row_blocked;
                if (with_contact) {
                    row_contact += contact_units_of_cell(id(x, y));
                    contact_prefix_[x + 1][y + 1] =
                        contact_prefix_[x][y + 1] + row_contact;
                }
            }
        }

        component_.fill(-1);
        for (vector<int> &cells : component_cells_) cells.clear();
        component_count_ = 0;
        for (int start = 0; start < n_ * n_; ++start) {
            if (owner_[start] != -1 || component_[start] != -1) continue;
            const int label = component_count_++;
            if (label == static_cast<int>(component_cells_.size())) {
                component_cells_.push_back({});
                component_cells_.back().reserve(MAX_P);
            }
            vector<int> &cells = component_cells_[label];
            int head = 0, tail = 0;
            component_queue_[tail++] = start;
            component_[start] = label;
            while (head < tail) {
                const int c = component_queue_[head++];
                cells.push_back(c);
                const int x = c / n_, y = c % n_;
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + DX[d], ny = y + DY[d];
                    if (!inside(nx, ny)) continue;
                    const int nc = id(nx, ny);
                    if (owner_[nc] == -1 && component_[nc] == -1) {
                        component_[nc] = label;
                        component_queue_[tail++] = nc;
                    }
                }
            }
        }
        free_component_value_valid_ = false;
    }

    int blocked_in_rect(int x1, int y1, int x2, int y2) const {
        if (x1 >= x2 || y1 >= y2) return 0;
        return blocked_prefix_[x2][y2] - blocked_prefix_[x1][y2] -
               blocked_prefix_[x2][y1] + blocked_prefix_[x1][y1];
    }

    bool build_packing_pressure() {
        for (int x = 0; x <= n_; ++x) {
            fill(packing_pressure_diff_[x], packing_pressure_diff_[x] + n_ + 1, 0.0);
        }

        for (const auto &[h, w] : PACKING_PROBE_SHAPES) {
            if (h > n_ || w > n_) continue;
            int valid_count = 0;
            for (int x = 0; x + h <= n_; ++x) {
                if ((x & 7) == 0 && elapsed() > NORMAL_CHEAP_MODE_TIME) return false;
                for (int y = 0; y + w <= n_; ++y) {
                    valid_count += blocked_in_rect(x, y, x + h, y + w) == 0;
                }
            }
            if (valid_count == 0) continue;

            // Every probe size has total weight one, so abundant small rectangles do not drown
            // out a rare surviving large rectangle.  The imos update gives each cell the expected
            // coverage probability of a uniformly chosen valid rectangle of this size.
            const double weight = 1.0 / valid_count;
            for (int x = 0; x + h <= n_; ++x) {
                for (int y = 0; y + w <= n_; ++y) {
                    if (blocked_in_rect(x, y, x + h, y + w) != 0) continue;
                    packing_pressure_diff_[x][y] += weight;
                    packing_pressure_diff_[x + h][y] -= weight;
                    packing_pressure_diff_[x][y + w] -= weight;
                    packing_pressure_diff_[x + h][y + w] += weight;
                }
            }
        }

        for (int y = 0; y <= n_; ++y) packing_pressure_prefix_[0][y] = 0.0;
        for (int x = 0; x < n_; ++x) {
            packing_pressure_prefix_[x + 1][0] = 0.0;
            double row_sum = 0.0;
            for (int y = 0; y < n_; ++y) {
                double pressure = packing_pressure_diff_[x][y];
                if (x > 0) pressure += packing_pressure_diff_[x - 1][y];
                if (y > 0) pressure += packing_pressure_diff_[x][y - 1];
                if (x > 0 && y > 0) pressure -= packing_pressure_diff_[x - 1][y - 1];
                packing_pressure_diff_[x][y] = pressure;
                packing_pressure_cell_[id(x, y)] = pressure;
                row_sum += pressure;
                packing_pressure_prefix_[x + 1][y + 1] =
                    packing_pressure_prefix_[x][y + 1] + row_sum;
            }
        }
        return true;
    }

    double packing_pressure_in_rect(int x1, int y1, int x2, int y2) const {
        if (x1 >= x2 || y1 >= y2) return 0.0;
        return packing_pressure_prefix_[x2][y2] - packing_pressure_prefix_[x1][y2] -
               packing_pressure_prefix_[x2][y1] + packing_pressure_prefix_[x1][y1];
    }

    double placement_packing_pressure(const Placement &placement) const {
        if (placement.cells.empty()) return 0.0;
        double pressure = 0.0;
        for (int c : placement.cells) pressure += packing_pressure_cell_[c];
        return pressure / static_cast<double>(placement.cells.size());
    }

    int contact_in_rect(int x1, int y1, int x2, int y2) const {
        if (x1 >= x2 || y1 >= y2) return 0;
        return contact_prefix_[x2][y2] - contact_prefix_[x1][y2] -
               contact_prefix_[x2][y1] + contact_prefix_[x1][y1];
    }

    int bounding_rect_contact(int x, int y, int h, int w) const {
        int contact = 0;
        contact += x == 0 ? w * CONTACT_SCALE
                          : contact_in_rect(x - 1, y, x, y + w);
        contact += x + h == n_ ? w * CONTACT_SCALE
                                : contact_in_rect(x + h, y, x + h + 1, y + w);
        contact += y == 0 ? h * CONTACT_SCALE
                          : contact_in_rect(x, y - 1, x + h, y);
        contact += y + w == n_ ? h * CONTACT_SCALE
                                : contact_in_rect(x, y + w, x + h, y + w + 1);
        return contact;
    }

    bool removed_cell(int dx, int dy, int h, int w, int slack, int variant) const {
        if (slack == 0) return false;
        const bool top = variant == 1 || variant == 3;
        const bool left = variant == 2 || variant == 3;
        const bool in_rows = top ? dx < slack : dx >= h - slack;
        const bool in_column = left ? dy == 0 : dy == w - 1;
        return in_rows && in_column;
    }

    int removed_blocked_count(int x, int y, int h, int w, int slack, int variant) const {
        if (slack == 0) return 0;
        const bool top = variant == 1 || variant == 3;
        const bool left = variant == 2 || variant == 3;
        const int x1 = top ? x : x + h - slack;
        const int x2 = x1 + slack;
        const int y1 = left ? y : y + w - 1;
        return blocked_in_rect(x1, y1, x2, y1 + 1);
    }

    vector<int> build_rect_region(const RawRect &raw, int variant) const {
        vector<int> cells;
        cells.reserve(raw.h * raw.w - raw.slack);
        for (int dx = 0; dx < raw.h; ++dx) {
            for (int dy = 0; dy < raw.w; ++dy) {
                if (!removed_cell(dx, dy, raw.h, raw.w, raw.slack, variant)) {
                    cells.push_back(id(raw.x + dx, raw.y + dy));
                }
            }
        }
        return cells;
    }

    vector<int> build_pond_rect_region(const RawRect &raw, int variant, int p) {
        ++trim_token_;
        if (trim_token_ == numeric_limits<int>::max()) {
            trim_mark_.fill(0);
            trim_token_ = 1;
        }

        const bool top = variant == 1 || variant == 3;
        const bool left = variant == 2 || variant == 3;
        const int edge_y = raw.y + (left ? 0 : raw.w - 1);
        int remaining_trim = raw.slack;
        for (int step = 0; step < raw.h && remaining_trim > 0; ++step) {
            const int dx = top ? step : raw.h - 1 - step;
            const int c = id(raw.x + dx, edge_y);
            if (owner_[c] != -1) continue;
            trim_mark_[c] = trim_token_;
            --remaining_trim;
        }
        if (remaining_trim > 0) return {};

        vector<int> cells;
        cells.reserve(p);
        for (int dx = 0; dx < raw.h; ++dx) {
            for (int dy = 0; dy < raw.w; ++dy) {
                const int c = id(raw.x + dx, raw.y + dy);
                if (owner_[c] == -1 && trim_mark_[c] != trim_token_) {
                    cells.push_back(c);
                }
            }
        }
        if (static_cast<int>(cells.size()) != p) return {};
        return cells;
    }

    bool connected_region(const vector<int> &cells) {
        if (cells.empty()) return false;
        ++region_token_;
        if (region_token_ == numeric_limits<int>::max()) {
            region_mark_.fill(0);
            region_token_ = 1;
        }
        for (int c : cells) region_mark_[c] = region_token_;

        ++seen_token_;
        if (seen_token_ == numeric_limits<int>::max()) {
            seen_mark_.fill(0);
            seen_token_ = 1;
        }
        vector<int> stack;
        stack.reserve(cells.size());
        stack.push_back(cells.front());
        seen_mark_[cells.front()] = seen_token_;
        int reached = 0;
        while (!stack.empty()) {
            const int c = stack.back();
            stack.pop_back();
            ++reached;
            const int x = c / n_, y = c % n_;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + DX[d], ny = y + DY[d];
                if (!inside(nx, ny)) continue;
                const int nc = id(nx, ny);
                if (region_mark_[nc] == region_token_ &&
                    seen_mark_[nc] != seen_token_) {
                    seen_mark_[nc] = seen_token_;
                    stack.push_back(nc);
                }
            }
        }
        return reached == static_cast<int>(cells.size());
    }

    void keep_two(array<RawRect, 2> &best, const RawRect &candidate) const {
        if (candidate.score > best[0].score) {
            best[1] = best[0];
            best[0] = candidate;
        } else if (candidate.score > best[1].score) {
            best[1] = candidate;
        }
    }


    vector<Placement> rectangle_candidates(int p, double deadline = NORMAL_RECT_DEADLINE,
                                           bool use_packing_pressure = false) {
        vector<Placement> result;
        result.reserve(100);
        bool timed_out = false;
        for (int h = 1; h <= min(n_, p); ++h) {
            if (elapsed() > deadline) break;
            const int w = (p + h - 1) / h;
            if (w > n_) continue;
            const int slack = h * w - p;
            array<RawRect, 2> best{};
            array<RawRect, 2> pond_best{};
            RawRect compact_raw;
            auto record_compact_raw = [&](const RawRect &candidate) {
                RawRect raw = candidate;
                raw.score = 0.0;
                if (compact_raw.score < -1e50) compact_raw = raw;
            };

            for (int x = 0; x + h <= n_; ++x) {
                if ((x & 7) == 0 && elapsed() > deadline) {
                    timed_out = true;
                    break;
                }
                for (int y = 0; y + w <= n_; ++y) {
                    const int blocked = blocked_in_rect(x, y, x + h, y + w);
                    if (blocked > slack) continue;
                    const int contact = bounding_rect_contact(x, y, h, w);
                    RawRect raw;
                    raw.x = x;
                    raw.y = y;
                    raw.h = h;
                    raw.w = w;
                    raw.slack = slack;
                    // For fixed h, compactness and perimeter are constant.  The experimental
                    // pressure term distinguishes a broad still-packable area from a tight edge
                    // even when both candidates leave the same component-size multiset.
                    raw.score = contact;
                    if (use_packing_pressure) {
                        const double average_pressure =
                            packing_pressure_in_rect(x, y, x + h, y + w) / (h * w);
                        const int estimated_perimeter = 2 * (h + w);
                        raw.score -= (PACKING_PRESSURE_WEIGHT / PLACEMENT_CONTACT_WEIGHT) *
                                     average_pressure * CONTACT_SCALE * estimated_perimeter;
                    }

                    const int ponds = prefix_sum_in_rect(
                        pond_prefix_, x, y, x + h, y + w);
                    // Occupied grass is not a permanent hole and must remain forbidden.  A pond
                    // rectangle uses every currently free grass cell except a short outer trim.
                    if (ponds > 0 && blocked == ponds && ponds <= slack) {
                        RawRect pond_raw = raw;
                        pond_raw.slack = slack - ponds;
                        pond_raw.pond_count = ponds;
                        // Isolated holes tend to add more perimeter than clustered ponds.  Pond
                        // count is only a cheap entrance ranking; exact perimeter is measured later.
                        pond_raw.score = contact - 2.0 * ponds * CONTACT_SCALE;
                        keep_two(pond_best, pond_raw);
                    }

                    if (blocked == 0) {
                        raw.variant = -1;
                        record_compact_raw(raw);
                        keep_two(best, raw);
                    } else if (slack > 0) {
                        for (int variant = 0; variant < 4; ++variant) {
                            if (removed_blocked_count(x, y, h, w, slack, variant) == blocked) {
                                raw.variant = variant;
                                record_compact_raw(raw);
                                keep_two(best, raw);
                                break;
                            }
                        }
                    }
                }
            }
            if (timed_out) break;

            for (const RawRect &raw : best) {
                if (raw.score < -1e50) continue;
                if (raw.variant >= 0) {
                    Placement candidate;
                    candidate.cells = build_rect_region(raw, raw.variant);
                    if (measure(candidate)) result.push_back(move(candidate));
                } else {
                    const int variants = raw.slack == 0 ? 1 : 4;
                    for (int variant = 0; variant < variants; ++variant) {
                        Placement candidate;
                        candidate.cells = build_rect_region(raw, variant);
                        if (measure(candidate)) result.push_back(move(candidate));
                    }
                }
            }
            auto add_compact_candidate = [&](const RawRect &raw,
                                             unsigned char lane) {
                if (raw.score < -1e50) return;
                int compact_variant = raw.variant;
                if (compact_variant < 0) compact_variant = 0;
                Placement candidate;
                candidate.cells = build_rect_region(raw, compact_variant);
                if (measure(candidate)) {
                    candidate.compactness_lane = lane;
                    result.push_back(move(candidate));
                }
            };
            add_compact_candidate(compact_raw, 1);
            for (const RawRect &raw : pond_best) {
                if (raw.score < -1e50) continue;
                const int variants = raw.slack == 0 ? 1 : 4;
                for (int variant = 0; variant < variants; ++variant) {
                    Placement candidate;
                    candidate.cells = build_pond_rect_region(raw, variant, p);
                    if (connected_region(candidate.cells) && measure(candidate)) {
                        candidate.pond_hole = true;
                        result.push_back(move(candidate));
                    }
                }
            }
        }
        return result;
    }

    int weighted_contact_neighbors(int c) const {
        const int x = c / n_, y = c % n_;
        int weight = 0;
        for (int d = 0; d < 4; ++d) {
            const int nx = x + DX[d], ny = y + DY[d];
            weight += !inside(nx, ny) ? CONTACT_SCALE : contact_units_of_cell(id(nx, ny));
        }
        return weight;
    }


    vector<int> choose_growth_seeds(int p) const {
        vector<int> seeds;
        seeds.reserve(64);
        array<unsigned char, MAX_CELLS> used{};
        vector<int> eligible;
        eligible.reserve(component_count_);
        for (int k = 0; k < component_count_; ++k) {
            if (static_cast<int>(component_cells_[k].size()) >= p) eligible.push_back(k);
        }

        auto add_seed = [&](int c) {
            if (c >= 0 && !used[c]) {
                used[c] = 1;
                seeds.push_back(c);
            }
        };

        // At least one seed per usable component. The total work of these seeds is O(N^2).
        for (int k : eligible) {
            int best = component_cells_[k][0];
            int best_score = -1;
            for (int c : component_cells_[k]) {
                const int score = weighted_contact_neighbors(c) -
                                  static_cast<int>(splitmix64(c + 17) & 255ULL);
                if (score > best_score) {
                    best_score = score;
                    best = c;
                }
            }
            add_seed(best);
        }

        int extra_budget = 28;
        for (int k : eligible) {
            if (extra_budget <= 0) break;
            const vector<int> &cells = component_cells_[k];
            long long sx = 0, sy = 0;
            for (int c : cells) {
                sx += c / n_;
                sy += c % n_;
            }
            const double component_size = static_cast<double>(cells.size());
            const double cx = static_cast<double>(sx) / component_size;
            const double cy = static_cast<double>(sy) / component_size;
            int center = cells[0];
            double center_dist = 1e100;
            array<int, 4> extreme = {cells[0], cells[0], cells[0], cells[0]};
            for (int c : cells) {
                const int x = c / n_, y = c % n_;
                const double dist = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                if (dist < center_dist) {
                    center_dist = dist;
                    center = c;
                }
                if (x + y < extreme[0] / n_ + extreme[0] % n_) extreme[0] = c;
                if (x + y > extreme[1] / n_ + extreme[1] % n_) extreme[1] = c;
                if (x - y < extreme[2] / n_ - extreme[2] % n_) extreme[2] = c;
                if (x - y > extreme[3] / n_ - extreme[3] % n_) extreme[3] = c;
            }
            const array<int, 5> extras = {center, extreme[0], extreme[1], extreme[2], extreme[3]};
            for (int c : extras) {
                const size_t before = seeds.size();
                add_seed(c);
                if (seeds.size() != before && --extra_budget <= 0) break;
            }
        }
        return seeds;
    }

    int growth_key(int c, int seed) const {
        const int x = c / n_, y = c % n_;
        int selected_neighbors = 0;
        for (int d = 0; d < 4; ++d) {
            const int nx = x + DX[d], ny = y + DY[d];
            if (inside(nx, ny) && grow_mark_[id(nx, ny)] == grow_token_) ++selected_neighbors;
        }
        const int sx = seed / n_, sy = seed % n_;
        const int distance = abs(x - sx) + abs(y - sy);
        const int tie = static_cast<int>(splitmix64(static_cast<uint64_t>(c) * 2503 + seed) & 31ULL);
        return selected_neighbors * 10000 +
               weighted_contact_neighbors(c) * 300 / CONTACT_SCALE - distance * 8 + tie;
    }

    Placement grow_from_seed(int seed, int p) {
        Placement result;
        if (owner_[seed] != -1) return result;
        const int label = component_[seed];
        if (label < 0 || static_cast<int>(component_cells_[label].size()) < p) return result;

        ++grow_token_;
        if (grow_token_ == numeric_limits<int>::max()) {
            grow_mark_.fill(0);
            grow_token_ = 1;
        }
        priority_queue<pair<int, int>> frontier;
        result.cells.reserve(p);

        auto select_cell = [&](int c) {
            grow_mark_[c] = grow_token_;
            result.cells.push_back(c);
            const int x = c / n_, y = c % n_;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + DX[d], ny = y + DY[d];
                if (!inside(nx, ny)) continue;
                const int nc = id(nx, ny);
                if (owner_[nc] == -1 && grow_mark_[nc] != grow_token_) {
                    frontier.push({growth_key(nc, seed), nc});
                }
            }
        };

        select_cell(seed);
        while (static_cast<int>(result.cells.size()) < p) {
            if (frontier.empty()) {
                result.cells.clear();
                return result;
            }
            const auto [stored_key, c] = frontier.top();
            frontier.pop();
            if (grow_mark_[c] == grow_token_ || owner_[c] != -1) continue;
            const int current_key = growth_key(c, seed);
            if (stored_key != current_key) {
                frontier.push({current_key, c});
                continue;
            }
            select_cell(c);
        }
        if (!measure(result)) result.cells.clear();
        return result;
    }

    vector<int> component_complement(const vector<int> &component_cells,
                                     const vector<int> &subset) {
        ++trim_token_;
        if (trim_token_ == numeric_limits<int>::max()) {
            trim_mark_.fill(0);
            trim_token_ = 1;
        }
        for (int c : subset) trim_mark_[c] = trim_token_;
        vector<int> result;
        result.reserve(component_cells.size() - subset.size());
        for (int c : component_cells) {
            if (trim_mark_[c] != trim_token_) result.push_back(c);
        }
        return result;
    }

    Placement find_small_component_placement(int p) {
        const vector<int> *target_cells = nullptr;
        int target_size = numeric_limits<int>::max();
        for (const vector<int> &island : initial_small_component_cells_) {
            const int size = static_cast<int>(island.size());
            if (size < p || size >= target_size) continue;
            bool completely_free = true;
            for (int c : island) {
                if (owner_[c] != -1) {
                    completely_free = false;
                    break;
                }
            }
            if (completely_free) {
                target_size = size;
                target_cells = &island;
            }
        }
        if (target_cells == nullptr) return {};

        const vector<int> &cells = *target_cells;
        if (target_size == p) {
            Placement exact;
            exact.cells = cells;
            if (measure(exact)) exact.forced_small_component = true;
            return exact;
        }

        const int remaining_size = target_size - p;
        auto grow_remaining = [&](int seed) {
            Placement remaining = grow_from_seed(seed, remaining_size);
            if (!remaining.valid()) return Placement{};
            Placement arriving;
            arriving.cells = component_complement(cells, remaining.cells);
            if (!connected_region(arriving.cells) || !measure(arriving)) return Placement{};
            arriving.forced_small_component = true;
            return arriving;
        };
        auto grow_arriving = [&](int seed) {
            Placement arriving = grow_from_seed(seed, p);
            if (!arriving.valid()) return Placement{};
            const vector<int> remaining = component_complement(cells, arriving.cells);
            if (!connected_region(remaining)) return Placement{};
            arriving.forced_small_component = true;
            return arriving;
        };

        // Grow the smaller side first.  Both sides are checked for connectivity, so a successful
        // result is a legal bipartition of the chosen component rather than a disconnected scrap.
        if (remaining_size <= p) {
            for (int seed : cells) {
                Placement candidate = grow_remaining(seed);
                if (candidate.valid()) return candidate;
            }
            for (int seed : cells) {
                Placement candidate = grow_arriving(seed);
                if (candidate.valid()) return candidate;
            }
        } else {
            for (int seed : cells) {
                Placement candidate = grow_arriving(seed);
                if (candidate.valid()) return candidate;
            }
            for (int seed : cells) {
                Placement candidate = grow_remaining(seed);
                if (candidate.valid()) return candidate;
            }
        }
        return {};
    }

    bool measure(Placement &placement) {
        if (placement.cells.empty()) return false;
        ++region_token_;
        if (region_token_ == numeric_limits<int>::max()) {
            region_mark_.fill(0);
            region_token_ = 1;
        }
        uint64_t hash = 0;
        for (int c : placement.cells) {
            if (c < 0 || c >= n_ * n_ || owner_[c] != -1 || region_mark_[c] == region_token_) {
                return false;
            }
            region_mark_[c] = region_token_;
            hash ^= splitmix64(static_cast<uint64_t>(c) + 0x12345678ULL);
        }

        int perimeter = 0;
        int weighted_contact = 0;
        for (int c : placement.cells) {
            const int x = c / n_, y = c % n_;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + DX[d], ny = y + DY[d];
                if (inside(nx, ny) && region_mark_[id(nx, ny)] == region_token_) continue;
                ++perimeter;
                weighted_contact += !inside(nx, ny)
                                        ? CONTACT_SCALE
                                        : contact_units_of_cell(id(nx, ny));
            }
        }
        placement.perimeter = perimeter;
        placement.weighted_contact = weighted_contact;
        placement.compactness = 4.0 * sqrt(static_cast<double>(placement.cells.size())) / perimeter;
        placement.preliminary = placement.compactness +
                                0.045 * weighted_contact /
                                    (CONTACT_SCALE * max(1, perimeter));
        placement.hash = hash;
        return true;
    }

    double size_cdf(int size) const {
        if (size < 4) return 0.0;
        if (size >= 150) return 1.0;
        const double lo = 2.0;
        const double hi = sqrt(150.0);
        return clamp((sqrt(size + 0.5) - lo) / (hi - lo), 0.0, 1.0);
    }


    double full_future_component_value(int placement_token) {
        ++seen_token_;
        if (seen_token_ == numeric_limits<int>::max()) {
            seen_mark_.fill(0);
            seen_token_ = 1;
        }
        vector<int> stack;
        stack.reserve(MAX_CELLS);
        double value = 0.0;
        for (int start = 0; start < n_ * n_; ++start) {
            if (owner_[start] != -1 || region_mark_[start] == placement_token ||
                seen_mark_[start] == seen_token_) {
                continue;
            }
            seen_mark_[start] = seen_token_;
            stack.clear();
            stack.push_back(start);
            int size = 0;
            while (!stack.empty()) {
                const int c = stack.back();
                stack.pop_back();
                ++size;
                const int x = c / n_, y = c % n_;
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + DX[d], ny = y + DY[d];
                    if (!inside(nx, ny)) continue;
                    const int nc = id(nx, ny);
                    if (owner_[nc] == -1 && region_mark_[nc] != placement_token &&
                        seen_mark_[nc] != seen_token_) {
                        seen_mark_[nc] = seen_token_;
                        stack.push_back(nc);
                    }
                }
            }
            value += size * size_cdf(size);
        }
        return value;
    }

    double future_usability(const Placement &placement) {
        if (placement.cells.empty()) return 0.0;
        ++region_token_;
        if (region_token_ == numeric_limits<int>::max()) {
            region_mark_.fill(0);
            region_token_ = 1;
        }
        const int parent_label = component_[placement.cells.front()];
        bool single_parent = parent_label >= 0;
        for (int c : placement.cells) {
            region_mark_[c] = region_token_;
            if (owner_[c] != -1 || component_[c] != parent_label) single_parent = false;
        }

        const int remaining = free_count_ - static_cast<int>(placement.cells.size());
        if (remaining <= 0) return 0.0;
        if (!single_parent) {
            return full_future_component_value(region_token_) / remaining;
        }

        ++seen_token_;
        if (seen_token_ == numeric_limits<int>::max()) {
            seen_mark_.fill(0);
            seen_token_ = 1;
        }
        vector<int> stack;
        const vector<int> &parent_cells = component_cells_[parent_label];
        stack.reserve(parent_cells.size());
        const int parent_size = static_cast<int>(parent_cells.size());
        if (!free_component_value_valid_) {
            free_component_value_ = 0.0;
            for (int component_index = 0; component_index < component_count_;
                 ++component_index) {
                const vector<int> &cells = component_cells_[component_index];
                const int size = static_cast<int>(cells.size());
                free_component_value_ += size * size_cdf(size);
            }
            free_component_value_valid_ = true;
        }
        double value = free_component_value_ - parent_size * size_cdf(parent_size);
        for (int start : parent_cells) {
            if (region_mark_[start] == region_token_ || seen_mark_[start] == seen_token_) {
                continue;
            }
            seen_mark_[start] = seen_token_;
            stack.clear();
            stack.push_back(start);
            int size = 0;
            while (!stack.empty()) {
                const int c = stack.back();
                stack.pop_back();
                ++size;
                const int x = c / n_, y = c % n_;
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + DX[d], ny = y + DY[d];
                    if (!inside(nx, ny)) continue;
                    const int nc = id(nx, ny);
                    if (component_[nc] == parent_label &&
                        region_mark_[nc] != region_token_ &&
                        seen_mark_[nc] != seen_token_) {
                        seen_mark_[nc] = seen_token_;
                        stack.push_back(nc);
                    }
                }
            }
            value += size * size_cdf(size);
        }
        return max(value, 0.0) / remaining;
    }



    Placement find_placement(int p, bool free_state_prepared = false) {
        if (!free_state_prepared) prepare_free_state();
        packing_pressure_ready_ = false;
        Placement none;
        // Once the hard reserve is reached, rejecting is preferable to missing the interactive
        // deadline. `prepare_free_state` was still run so the admission threshold sees the real
        // occupancy.
        if (elapsed() > HARD_STOP_DEADLINE) return none;
        bool has_component = false;
        for (int component_index = 0; component_index < component_count_;
             ++component_index) {
            const vector<int> &cells = component_cells_[component_index];
            if (static_cast<int>(cells.size()) >= p) {
                has_component = true;
                break;
            }
        }
        if (!has_component) return none;

        // Leave a generous I/O margin near the two-second limit.
        const bool cheap_mode = elapsed() > NORMAL_CHEAP_MODE_TIME;
        if (!cheap_mode && p <= PACKING_PRESSURE_MAX_P) {
            packing_pressure_ready_ = build_packing_pressure();
        }
        vector<int> seeds = choose_growth_seeds(p);
        if (cheap_mode) {
            Placement best;
            for (int seed : seeds) {
                if (elapsed() > HARD_STOP_DEADLINE) break;
                Placement candidate = grow_from_seed(seed, p);
                if (!candidate.valid()) continue;
                if (candidate.preliminary > best.preliminary) {
                    best = move(candidate);
                }
            }
            return best;
        }

        vector<Placement> candidates;
        vector<Placement> rectangle_pool =
            rectangle_candidates(p, NORMAL_RECT_DEADLINE,
                                 packing_pressure_ready_
            );
        candidates.reserve(candidates.size() + rectangle_pool.size() + seeds.size());
        for (Placement &candidate : rectangle_pool) {
            candidates.push_back(move(candidate));
        }
        for (int seed : seeds) {
            if (elapsed() > NORMAL_GROW_DEADLINE) break;
            Placement candidate = grow_from_seed(seed, p);
            if (candidate.valid()) candidates.push_back(move(candidate));
        }
        if (candidates.empty()) return none;

        if (packing_pressure_ready_) {
            for (Placement &candidate : candidates) {
                candidate.packing_pressure = placement_packing_pressure(candidate);
                candidate.preliminary -=
                    PACKING_PRESSURE_WEIGHT * candidate.packing_pressure;
            }
        }


        array<Placement, 1> compactness_lanes;
        Placement compactness_only;
        for (const Placement &candidate : candidates) {
            if (!candidate.compactness_lane) continue;
            const int lane_index = candidate.compactness_lane - 1;
            if (lane_index >= 0 && lane_index < static_cast<int>(compactness_lanes.size()) &&
                (!compactness_lanes[lane_index].valid() ||
                 candidate.compactness > compactness_lanes[lane_index].compactness)) {
                compactness_lanes[lane_index] = candidate;
            }
            if (!compactness_only.valid() || candidate.compactness > compactness_only.compactness)
                compactness_only = candidate;
        }
        sort(candidates.begin(), candidates.end(), [](const Placement &a, const Placement &b) {
            if (a.preliminary != b.preliminary) return a.preliminary > b.preliminary;
            return a.hash < b.hash;
        });
        if (elapsed() > NORMAL_FAST_SELECT_DEADLINE) {
            if (compactness_only.valid() &&
                compactness_only.compactness > candidates.front().compactness)
                return compactness_only;
            return candidates.front();
        }


        for (auto it = compactness_lanes.rbegin(); it != compactness_lanes.rend(); ++it) {
            if (it->valid()) candidates.insert(candidates.begin(), move(*it));
        }
        unordered_set<uint64_t> used;
        used.reserve(32);
        Placement best;
        int evaluated = 0;
        for (Placement &candidate : candidates) {
            if (evaluated > 0 && elapsed() > NORMAL_USABILITY_DEADLINE) break;
            if (!used.insert(candidate.hash).second) continue;
            candidate.future_usability = future_usability(candidate);
            const double contact_ratio = static_cast<double>(candidate.weighted_contact) /
                                         (CONTACT_SCALE * max(1, candidate.perimeter));
            candidate.final_value = candidate.compactness + 0.045 * contact_ratio +
                                    0.09 * candidate.future_usability;
            if (packing_pressure_ready_) {
                candidate.final_value -=
                    PACKING_PRESSURE_WEIGHT * candidate.packing_pressure;
            }
            if (candidate.final_value > best.final_value) best = candidate;
            if (++evaluated == 8) break;
        }
        return best;
    }

    bool valid_shape(const vector<int> &cells, int p, int &perimeter,
                     uint64_t &hash) const {
        if (static_cast<int>(cells.size()) != p || cells.empty()) return false;
        array<unsigned char, MAX_CELLS> in_region{};
        for (int c : cells) {
            if (c < 0 || c >= n_ * n_ || park_[c / n_][c % n_] != '.' || in_region[c]) {
                return false;
            }
            in_region[c] = 1;
        }

        array<unsigned char, MAX_CELLS> seen{};
        vector<int> stack = {cells[0]};
        seen[cells[0]] = 1;
        int reached = 0;
        perimeter = 0;
        hash = 0;
        while (!stack.empty()) {
            const int c = stack.back();
            stack.pop_back();
            ++reached;
            hash ^= splitmix64(static_cast<uint64_t>(c) + 0x12345678ULL);
            const int x = c / n_, y = c % n_;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + DX[d], ny = y + DY[d];
                if (!inside(nx, ny) || !in_region[id(nx, ny)]) {
                    ++perimeter;
                } else {
                    const int nc = id(nx, ny);
                    if (!seen[nc]) {
                        seen[nc] = 1;
                        stack.push_back(nc);
                    }
                }
            }
        }
        return reached == p;
    }

    Placement make_target_placement(const RawMoveTarget &target, int p) const {
        Placement placement;
        placement.cells = build_rect_region(target.raw, target.variant);
        if (static_cast<int>(placement.cells.size()) != p) {
            placement.cells.clear();
            return placement;
        }
        // A ceil(P/h) rectangle with a strict corner strip removed is connected and keeps
        // the bounding-box perimeter. enumerate_move_targets already excluded every pond.
        placement.perimeter = 2 * (target.raw.h + target.raw.w);
        placement.compactness =
            static_cast<double>(payment_scale_[p]) / placement.perimeter;
        placement.preliminary = placement.compactness;
        for (int c : placement.cells) {
            placement.hash ^= splitmix64(static_cast<uint64_t>(c) + 0x12345678ULL);
        }
        return placement;
    }

    int minimum_shape_perimeter(int p) const {
        int best = numeric_limits<int>::max();
        for (int h = 1; h <= min(n_, p); ++h) {
            const int w = (p + h - 1) / h;
            if (w <= n_) best = min(best, 2 * (h + w));
        }
        return best;
    }

    double movement_margin(long long arriving_fee) const {
        return max(100.0, 0.20 * static_cast<double>(arriving_fee));
    }

    double estimated_theta() const {
        constexpr long double PRIOR = 16.0L;
        return clamp(
            static_cast<double>((sum_duration_ + PRIOR * 5000.0L) /
                                (observed_ + PRIOR)),
            2000.0, 8000.0);
    }

    bool depth3_high_absolute_value(const Group &group) const {
        if (group.v <= 0) return false;
        const double normalized_v =
            static_cast<double>(group.v) / pow(estimated_theta(), 0.9);
        return normalized_v >= 400.0;
    }


    bool atlas_repack_enabled(const Group &group) const {
        const long long duration = group.t - group.s;
        if (group.p < COMPACT_TEMPLATE_MIN_P || duration <= 0 || group.v <= 0) {
            return false;
        }
        const double theta_hat = estimated_theta();
        const double density = static_cast<double>(group.v) /
                               (static_cast<double>(group.p) *
                                static_cast<double>(duration));
        const double log2_mean = -0.1 * (log2(theta_hat) + 0.610);
        const double z = (log2(density) - log2_mean) / 0.81;
        return z >= ATLAS_REPACK_Z_TRIGGER;
    }


    bool move_target_has_component_capacity(
        const MoveTargetCandidate &target) const {
        array<unsigned char, MAX_GROUPS> removed_group{};
        vector<int> required_sizes;
        required_sizes.reserve(target.blockers.size());
        for (int group_id : target.blockers) {
            if (group_id < 0 || group_id >= m_ || !groups_[group_id].active) {
                return false;
            }
            removed_group[group_id] = 1;
            required_sizes.push_back(groups_[group_id].p);
        }
        if (required_sizes.empty()) return false;

        array<unsigned char, MAX_CELLS> arrival_cell{};
        for (int cell : target.arriving.cells) arrival_cell[cell] = 1;
        auto available_after_target = [&](int cell) {
            if (arrival_cell[cell]) return false;
            const int group_id = owner_[cell];
            return group_id == -1 ||
                   (group_id >= 0 && group_id < m_ &&
                    removed_group[group_id]);
        };

        array<unsigned char, MAX_CELLS> seen{};
        array<int, MAX_CELLS> queue{};
        vector<int> component_capacities;
        component_capacities.reserve(target.blockers.size() + 8);
        for (int start = 0; start < n_ * n_; ++start) {
            if (seen[start] || !available_after_target(start)) continue;
            int head = 0;
            int tail = 0;
            int component_size = 0;
            seen[start] = 1;
            queue[tail++] = start;
            while (head < tail) {
                const int cell = queue[head++];
                ++component_size;
                const int x = cell / n_;
                const int y = cell % n_;
                for (int direction = 0; direction < 4; ++direction) {
                    const int nx = x + DX[direction];
                    const int ny = y + DY[direction];
                    if (!inside(nx, ny)) continue;
                    const int next = id(nx, ny);
                    if (seen[next] || !available_after_target(next)) continue;
                    seen[next] = 1;
                    queue[tail++] = next;
                }
            }
            component_capacities.push_back(component_size);
        }

        sort(required_sizes.begin(), required_sizes.end(), greater<int>());
        for (int required : required_sizes) {
            int best_component = -1;
            for (int index = 0;
                 index < static_cast<int>(component_capacities.size());
                 ++index) {
                if (component_capacities[index] < required) continue;
                if (best_component < 0 ||
                    component_capacities[index] <
                        component_capacities[best_component]) {
                    best_component = index;
                }
            }
            if (best_component < 0) return false;
            component_capacities[best_component] -= required;
        }
        return true;
    }

    vector<MoveTargetCandidate> enumerate_move_targets(const Group &g, double opportunity,
                                                        double baseline_value,
                                                        long long minimum_move_cost,
                                                        double enumeration_deadline,
                                                        double exact_deadline) {
        build_move_prefixes();
        vector<RawMoveTarget> raw_targets;
        raw_targets.reserve(60000);

        bool enumeration_timed_out = false;
        for (int h = 1; h <= min(n_, g.p); ++h) {
            if (elapsed() > enumeration_deadline) break;
            const int w = (g.p + h - 1) / h;
            if (w > n_) continue;
            const int slack = h * w - g.p;
            const int perimeter = 2 * (h + w);
            const long long fee = round_payment(g.v, g.p, perimeter);
            if (static_cast<double>(fee) - opportunity -
                    static_cast<double>(minimum_move_cost) <=
                baseline_value + movement_margin(fee)) {
                continue;
            }

            const int variants = slack == 0 ? 1 : 4;
            for (int x = 0; x + h <= n_; ++x) {
                if ((x & 7) == 0 && elapsed() > enumeration_deadline) {
                    enumeration_timed_out = true;
                    break;
                }
                for (int y = 0; y + w <= n_; ++y) {
                    RawRect raw;
                    raw.x = x;
                    raw.y = y;
                    raw.h = h;
                    raw.w = w;
                    raw.slack = slack;
                    const int pond_in_box =
                        prefix_sum_in_rect(pond_prefix_, x, y, x + h, y + w);
                    const int occupied_in_box =
                        prefix_sum_in_rect(occupied_prefix_, x, y, x + h, y + w);
                    const long double weight_in_box =
                        prefix_sum_in_rect(move_weight_prefix_, x, y, x + h, y + w);
                    for (int variant = 0; variant < variants; ++variant) {
                        const int ponds =
                            pond_in_box - removed_strip_sum(pond_prefix_, raw, variant);
                        if (ponds != 0) continue;
                        const int occupied = occupied_in_box -
                                             removed_strip_sum(occupied_prefix_, raw, variant);
                        if (occupied == 0) continue;
                        const long double fractional_move_cost =
                            weight_in_box -
                            removed_strip_sum(move_weight_prefix_, raw, variant);
                        RawMoveTarget target;
                        target.raw = raw;
                        target.variant = variant;
                        target.occupied_cells = occupied;
                        target.approximate_value =
                            static_cast<double>(static_cast<long double>(fee) - opportunity -
                                                fractional_move_cost);
                        raw_targets.push_back(target);
                    }
                }
            }
            if (enumeration_timed_out) break;
        }
        const bool has_compact_templates =
            COMPACT_TEMPLATE_MIN_P <= g.p && g.p <= MAX_P &&
            !compact_slot_templates_[g.p].empty();
        if (raw_targets.empty() && !has_compact_templates) return {};

        vector<RawMoveTarget> pool;
        const size_t sample_count = min(MOVE_SPATIAL_SAMPLES, raw_targets.size());
        pool.reserve(min(MOVE_ROUGH_KEEP, raw_targets.size()) + sample_count);
        for (size_t k = 0; k < sample_count; ++k) {
            const size_t index = k * raw_targets.size() / sample_count;
            pool.push_back(raw_targets[index]);
        }

        auto rough_better = [](const RawMoveTarget &a, const RawMoveTarget &b) {
            if (a.approximate_value != b.approximate_value) {
                return a.approximate_value > b.approximate_value;
            }
            return a.occupied_cells < b.occupied_cells;
        };
        const size_t rough_count = min(MOVE_ROUGH_KEEP, raw_targets.size());
        if (raw_targets.size() > rough_count) {
            nth_element(raw_targets.begin(), raw_targets.begin() + static_cast<ptrdiff_t>(rough_count),
                        raw_targets.end(), rough_better);
        }
        for (size_t k = 0; k < rough_count; ++k) pool.push_back(raw_targets[k]);

        vector<MoveTargetCandidate> exact_targets;
        exact_targets.reserve(pool.size() +
                              (has_compact_templates
                                   ? compact_slot_templates_[g.p].size()
                                   : 0));
        unordered_set<uint64_t> used_shapes;
        used_shapes.reserve(exact_targets.capacity() * 2 + 1);
        vector<int> blocker_seen(m_, -1);
        int blocker_token = 0;
        const bool allow_large_atlas_repack = atlas_repack_enabled(g);
        auto inspect_exact = [&](Placement arriving, bool atlas) {
            if (!arriving.valid() || !used_shapes.insert(arriving.hash).second) return;

            ++blocker_token;
            vector<int> blockers;
            bool valid = true;
            const int blocker_limit =
                atlas && allow_large_atlas_repack ? ATLAS_REPACK_MAX_MOVES : 3;
            for (int c : arriving.cells) {
                const int group_id = owner_[c];
                if (group_id < 0) continue;
                if (group_id >= m_ || !groups_[group_id].active) {
                    valid = false;
                    break;
                }
                if (blocker_seen[group_id] != blocker_token) {
                    blocker_seen[group_id] = blocker_token;
                    blockers.push_back(group_id);
                    if (static_cast<int>(blockers.size()) > blocker_limit) {
                        valid = false;
                        break;
                    }
                }
            }
            if (!valid || blockers.empty()) return;
            sort(blockers.begin(), blockers.end());

            long long cost = 0;
            for (int group_id : blockers) cost += move_cost(group_id);
            const long long fee = round_payment(g.v, g.p, arriving.perimeter);
            const double upper = static_cast<double>(fee) - opportunity -
                                 static_cast<double>(cost);
            if (upper <= baseline_value + movement_margin(fee)) return;

            MoveTargetCandidate candidate;
            candidate.arriving = move(arriving);
            candidate.blockers = move(blockers);
            candidate.upper_value = upper;
            candidate.atlas = atlas;
            exact_targets.push_back(move(candidate));
        };

        // The atlas candidates are few and mutually disjoint by construction.  Inspect all of
        // them exactly before the broad rectangle pool and before any deadline check.
        if (has_compact_templates) {
            for (const Placement &placement : compact_slot_templates_[g.p]) {
                inspect_exact(placement, true);
            }
        }

        size_t inspected_targets = 0;
        for (const RawMoveTarget &raw_target : pool) {
            if (((++inspected_targets) & 63U) == 0U && elapsed() > exact_deadline) break;
            inspect_exact(make_target_placement(raw_target, g.p), false);
        }

        sort(exact_targets.begin(), exact_targets.end(), [](const MoveTargetCandidate &a,
                                                             const MoveTargetCandidate &b) {
            if (a.upper_value != b.upper_value) return a.upper_value > b.upper_value;
            return a.arriving.perimeter < b.arriving.perimeter;
        });

        vector<MoveTargetCandidate> prepool;
        prepool.reserve(min(MOVE_REPAIR_PREPOOL, exact_targets.size()));
        vector<pair<vector<int>, int>> blocker_set_counts;
        for (MoveTargetCandidate &candidate : exact_targets) {
            int *count = nullptr;
            for (auto &entry : blocker_set_counts) {
                if (entry.first == candidate.blockers) {
                    count = &entry.second;
                    break;
                }
            }
            if (count == nullptr) {
                blocker_set_counts.push_back({candidate.blockers, 0});
                count = &blocker_set_counts.back().second;
            }
            if (*count >= 2) continue;
            ++*count;
            prepool.push_back(move(candidate));
            if (prepool.size() == MOVE_REPAIR_PREPOOL) break;
        }

        vector<size_t> selected_indices;
        selected_indices.reserve(min(MOVE_TARGET_KEEP, prepool.size()));
        vector<unsigned char> selected_flag(prepool.size(), 0);
        auto add_index = [&](size_t index) {
            if (selected_flag[index] ||
                selected_indices.size() == MOVE_TARGET_KEEP) {
                return false;
            }
            selected_flag[index] = 1;
            selected_indices.push_back(index);
            return true;
        };

        const size_t base_count =
            min(MOVE_REPAIR_BASE_KEEP, prepool.size());
        const size_t prefix_count =
            min(MOVE_REPAIR_PREFIX_KEEP, base_count);
        for (size_t index = 0; index < prefix_count; ++index) {
            add_index(index);
        }

        size_t repair_lane_count = 0;
        size_t probed = 0;
        for (size_t index = prefix_count;
             index < prepool.size() &&
             repair_lane_count < MOVE_REPAIR_LANE_KEEP;
             ++index) {
            if (((probed++) & 7U) == 0U &&
                elapsed() > exact_deadline + MOVE_REPAIR_PROBE_BUDGET) {
                break;
            }
            if (!move_target_has_component_capacity(prepool[index])) {
                continue;
            }
            if (add_index(index)) ++repair_lane_count;
        }

        for (size_t index = prefix_count; index < base_count; ++index) {
            add_index(index);
        }
        for (size_t index = base_count;
             index < prepool.size() &&
             selected_indices.size() < MOVE_TARGET_KEEP;
             ++index) {
            add_index(index);
        }

        vector<MoveTargetCandidate> selected;
        selected.reserve(selected_indices.size());
        for (size_t index : selected_indices) {
            selected.push_back(move(prepool[index]));
        }
        return selected;
    }

    vector<RelocationCandidate> relocation_candidates(int group_id, double deadline) {
        if (elapsed() >= deadline) return {};
        const Group &group = groups_[group_id];
        set_contact_target(group.t);
        rebuild_contact_prefix();
        bool has_component = false;
        for (int component_index = 0; component_index < component_count_;
             ++component_index) {
            const vector<int> &cells = component_cells_[component_index];
            if (static_cast<int>(cells.size()) >= group.p) {
                has_component = true;
                break;
            }
        }
        if (!has_component) return {};

        vector<Placement> placements = rectangle_candidates(group.p, deadline);
        if (elapsed() < deadline) {
            vector<int> seeds = choose_growth_seeds(group.p);
            int used_seeds = 0;
            for (int seed : seeds) {
                if (elapsed() > deadline || used_seeds == 16) break;
                Placement candidate = grow_from_seed(seed, group.p);
                if (candidate.valid()) placements.push_back(move(candidate));
                ++used_seeds;
            }
        }

        const long long old_fee =
            round_payment(group.v, group.p, group.max_perimeter);
        vector<RelocationCandidate> candidates;
        candidates.reserve(placements.size());
        reset_relocation_hash();
        for (Placement &placement : placements) {
            if (!placement.valid() || !insert_relocation_hash(placement.hash)) continue;
            const int new_max_perimeter = max(group.max_perimeter, placement.perimeter);
            RelocationCandidate candidate;
            candidate.fee_loss =
                old_fee - round_payment(group.v, group.p, new_max_perimeter);
            candidate.placement = move(placement);
            candidates.push_back(move(candidate));
        }
        sort(candidates.begin(), candidates.end(), [](const RelocationCandidate &a,
                                                       const RelocationCandidate &b) {
            if (a.fee_loss != b.fee_loss) return a.fee_loss < b.fee_loss;
            if (a.placement.perimeter != b.placement.perimeter) {
                return a.placement.perimeter < b.placement.perimeter;
            }
            if (a.placement.preliminary != b.placement.preliminary) {
                return a.placement.preliminary > b.placement.preliminary;
            }
            return a.placement.hash < b.placement.hash;
        });
        if (candidates.size() <= RELOCATION_KEEP) return candidates;

        // Retain the strongest candidates, then spend the remaining slots on distinct spatial
        // buckets so several moved groups do not all target the same attractive corner.
        auto bucket_of = [&](const RelocationCandidate &candidate) {
            int sx = 0, sy = 0;
            for (int c : candidate.placement.cells) {
                sx += c / n_;
                sy += c % n_;
            }
            const int size = max(static_cast<int>(candidate.placement.cells.size()), 1);
            const int bx = min((sx / size) / 10, 4);
            const int by = min((sy / size) / 10, 4);
            return bx * 5 + by;
        };
        vector<RelocationCandidate> selected;
        selected.reserve(RELOCATION_KEEP);
        array<unsigned char, 25> used_buckets{};
        for (size_t index = 0; index < RELOCATION_STRONG_KEEP; ++index) {
            used_buckets[bucket_of(candidates[index])] = 1;
            selected.push_back(move(candidates[index]));
        }
        for (size_t index = RELOCATION_STRONG_KEEP;
             index < candidates.size() && selected.size() < RELOCATION_KEEP; ++index) {
            const int bucket = bucket_of(candidates[index]);
            if (used_buckets[bucket]) continue;
            used_buckets[bucket] = 1;
            selected.push_back(move(candidates[index]));
        }
        for (size_t index = RELOCATION_STRONG_KEEP;
             index < candidates.size() && selected.size() < RELOCATION_KEEP; ++index) {
            if (candidates[index].placement.valid()) {
                selected.push_back(move(candidates[index]));
            }
        }
        return selected;
    }


    MovePlan solve_atlas_repack(const MoveTargetCandidate &target,
                                const array<int, MAX_CELLS> &base_owner,
                                int arriving_id, const Group &arriving,
                                double opportunity, double baseline_value,
                                double deadline) {
        MovePlan best;
        if (!target.atlas || target.blockers.size() <= 3 ||
            target.blockers.size() > ATLAS_REPACK_MAX_MOVES) {
            return best;
        }

        owner_ = base_owner;
        bool valid = true;
        for (int group_id : target.blockers) {
            for (int c : groups_[group_id].cells) {
                if (owner_[c] != group_id) {
                    valid = false;
                    break;
                }
                owner_[c] = -1;
            }
            if (!valid) break;
        }
        if (valid) {
            for (int c : target.arriving.cells) {
                if (owner_[c] != -1) {
                    valid = false;
                    break;
                }
                owner_[c] = arriving_id;
            }
        }
        if (!valid) {
            owner_ = base_owner;
            return best;
        }

        const array<int, MAX_CELLS> scratch_owner = owner_;
        prepare_free_state(false);
        vector<vector<RelocationCandidate>> candidate_lists;
        candidate_lists.reserve(target.blockers.size());
        for (int group_id : target.blockers) {
            candidate_lists.push_back(relocation_candidates(group_id, deadline));
            if (candidate_lists.back().empty() || elapsed() > deadline) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            owner_ = base_owner;
            return best;
        }

        using CellMask = array<uint64_t, ATLAS_REPACK_MASK_WORDS>;
        vector<vector<CellMask>> masks(candidate_lists.size());
        for (size_t group_index = 0; group_index < candidate_lists.size(); ++group_index) {
            masks[group_index].resize(candidate_lists[group_index].size());
            for (size_t candidate_index = 0;
                 candidate_index < candidate_lists[group_index].size(); ++candidate_index) {
                for (int c : candidate_lists[group_index][candidate_index].placement.cells) {
                    masks[group_index][candidate_index][c >> 6] |= 1ULL << (c & 63);
                }
            }
        }

        vector<vector<int>> orders;
        auto add_order = [&](vector<int> order) {
            if (find(orders.begin(), orders.end(), order) == orders.end()) {
                orders.push_back(move(order));
            }
        };
        vector<int> order(target.blockers.size());
        iota(order.begin(), order.end(), 0);
        add_order(order);
        sort(order.begin(), order.end(), [&](int a, int b) {
            if (groups_[target.blockers[a]].p != groups_[target.blockers[b]].p) {
                return groups_[target.blockers[a]].p > groups_[target.blockers[b]].p;
            }
            return target.blockers[a] < target.blockers[b];
        });
        add_order(order);
        sort(order.begin(), order.end(), [&](int a, int b) {
            if (candidate_lists[a].size() != candidate_lists[b].size()) {
                return candidate_lists[a].size() < candidate_lists[b].size();
            }
            return groups_[target.blockers[a]].p > groups_[target.blockers[b]].p;
        });
        add_order(order);
        sort(order.begin(), order.end(), [&](int a, int b) {
            if (groups_[target.blockers[a]].t != groups_[target.blockers[b]].t) {
                return groups_[target.blockers[a]].t > groups_[target.blockers[b]].t;
            }
            return target.blockers[a] < target.blockers[b];
        });
        add_order(order);
        reverse(order.begin(), order.end());
        add_order(order);

        vector<AtlasBeamState> complete;
        for (const vector<int> &placement_order : orders) {
            if (elapsed() > deadline) break;
            vector<AtlasBeamState> beam(1);
            for (int group_index : placement_order) {
                vector<AtlasBeamState> next;
                next.reserve(beam.size() * candidate_lists[group_index].size());
                for (const AtlasBeamState &state : beam) {
                    for (size_t candidate_index = 0;
                         candidate_index < candidate_lists[group_index].size();
                         ++candidate_index) {
                        bool overlaps = false;
                        for (int word = 0; word < ATLAS_REPACK_MASK_WORDS; ++word) {
                            if (state.used[word] & masks[group_index][candidate_index][word]) {
                                overlaps = true;
                                break;
                            }
                        }
                        if (overlaps) continue;
                        AtlasBeamState child = state;
                        for (int word = 0; word < ATLAS_REPACK_MASK_WORDS; ++word) {
                            child.used[word] |= masks[group_index][candidate_index][word];
                        }
                        child.choice[group_index] = static_cast<int>(candidate_index);
                        child.fee_loss += candidate_lists[group_index][candidate_index].fee_loss;
                        child.placement_quality +=
                            candidate_lists[group_index][candidate_index].placement.preliminary;
                        next.push_back(move(child));
                    }
                }
                if (next.empty()) {
                    beam.clear();
                    break;
                }
                auto better = [](const AtlasBeamState &a, const AtlasBeamState &b) {
                    if (a.fee_loss != b.fee_loss) return a.fee_loss < b.fee_loss;
                    return a.placement_quality > b.placement_quality;
                };
                if (next.size() > ATLAS_REPACK_BEAM_WIDTH) {
                    nth_element(next.begin(), next.begin() + ATLAS_REPACK_BEAM_WIDTH,
                                next.end(), better);
                    next.resize(ATLAS_REPACK_BEAM_WIDTH);
                }
                beam = move(next);
                if (elapsed() > deadline) break;
            }
            if (beam.empty()) continue;
            sort(beam.begin(), beam.end(), [](const AtlasBeamState &a,
                                               const AtlasBeamState &b) {
                if (a.fee_loss != b.fee_loss) return a.fee_loss < b.fee_loss;
                return a.placement_quality > b.placement_quality;
            });
            const int keep = min(static_cast<int>(beam.size()),
                                 ATLAS_REPACK_FINAL_KEEP);
            for (int index = 0; index < keep; ++index) complete.push_back(beam[index]);
        }

        sort(complete.begin(), complete.end(), [](const AtlasBeamState &a,
                                                   const AtlasBeamState &b) {
            if (a.fee_loss != b.fee_loss) return a.fee_loss < b.fee_loss;
            return a.placement_quality > b.placement_quality;
        });
        const int evaluate_count = min(static_cast<int>(complete.size()),
                                       ATLAS_REPACK_FINAL_KEEP);
        long long fixed_move_cost = 0;
        for (int group_id : target.blockers) fixed_move_cost += move_cost(group_id);
        const long long arriving_fee =
            round_payment(arriving.v, arriving.p, target.arriving.perimeter);
        const double required = baseline_value + movement_margin(arriving_fee);

        for (int state_index = 0;
             state_index < evaluate_count && elapsed() <= deadline; ++state_index) {
            const AtlasBeamState &state = complete[state_index];
            owner_ = scratch_owner;
            vector<MoveAction> actions;
            actions.reserve(target.blockers.size());
            bool placed = true;
            for (size_t group_index = 0; group_index < target.blockers.size(); ++group_index) {
                const int candidate_index = state.choice[group_index];
                if (candidate_index < 0) {
                    placed = false;
                    break;
                }
                const Placement &placement =
                    candidate_lists[group_index][candidate_index].placement;
                for (int c : placement.cells) {
                    if (owner_[c] != -1) {
                        placed = false;
                        break;
                    }
                    owner_[c] = target.blockers[group_index];
                }
                if (!placed) break;
                actions.push_back({target.blockers[group_index], placement});
            }
            if (!placed) continue;

            const double value = static_cast<double>(arriving_fee) - opportunity -
                                 static_cast<double>(fixed_move_cost) -
                                 static_cast<double>(state.fee_loss);
            if (value <= required || value <= best.value) continue;
            best.moves = move(actions);
            best.arriving = target.arriving;
            best.total_move_cost = fixed_move_cost;
            best.total_fee_loss = state.fee_loss;
            best.value = value;
        }

        owner_ = base_owner;
        return best;
    }

    MovePlan solve_limited_repack_chain(const MoveTargetCandidate &target,
                                         const array<int, MAX_CELLS> &parent_scratch,
                                         const Group &arriving,
                                         double opportunity, double baseline_value,
                                         double incumbent_value, bool allow_depth3,
                                         double deadline) {
        MovePlan best{};
        if (target.atlas || target.blockers.size() != 1 || elapsed() >= deadline) {
            return best;
        }

        const int parent_id = target.blockers.front();
        const Group &parent = groups_[parent_id];
        const long long root_cost = movement_cost(arriving);
        const long long parent_cost = move_cost(parent_id);
        if (movement_cost_tier(parent) >= movement_cost_tier(arriving) ||
            parent_cost >= root_cost) {
            return best;
        }

        // The caller already removed the sole parent, placed the arrival, and prepared
        // components for exactly this scratch board.
        owner_ = parent_scratch;
        bool valid = true;
        const double depth2_deadline =
            allow_depth3 ? deadline - LIMITED_REPACK_DEPTH3_EXTRA_BUDGET : deadline;
        function<void(double)> deferred_depth3;

        struct SecondaryVictim {
            int group_id = -1;
            int boundary_contact = 0;
            int unlocked_capacity = 0;
            long long cost = 0;
        };
        if (++limited_group_token_ == numeric_limits<int>::max()) {
            limited_group_seen_.fill(0);
            limited_group_token_ = 1;
        }
        array<int, 4 * MAX_P> adjacent_groups{};
        int adjacent_group_count = 0;
        for (int c : parent.cells) {
            const int x = c / n_, y = c % n_;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + DX[d], ny = y + DY[d];
                if (!inside(nx, ny)) continue;
                const int group_id = owner_[id(nx, ny)];
                if (group_id >= 0 && group_id < m_ && group_id != parent_id &&
                    groups_[group_id].active) {
                    if (limited_group_seen_[group_id] != limited_group_token_) {
                        limited_group_seen_[group_id] = limited_group_token_;
                        limited_boundary_contact_[group_id] = 0;
                        adjacent_groups[adjacent_group_count++] = group_id;
                    }
                    ++limited_boundary_contact_[group_id];
                }
            }
        }

        auto better_victim = [](const SecondaryVictim &a,
                                const SecondaryVictim &b) {
            if (a.cost != b.cost) return a.cost < b.cost;
            if (a.boundary_contact != b.boundary_contact) {
                return a.boundary_contact > b.boundary_contact;
            }
            if (a.unlocked_capacity != b.unlocked_capacity) {
                return a.unlocked_capacity > b.unlocked_capacity;
            }
            return a.group_id < b.group_id;
        };
        array<SecondaryVictim, LIMITED_REPACK_SECONDARY_KEEP> victims{};
        int victim_count = 0;
        for (int adjacent_index = 0; adjacent_index < adjacent_group_count;
             ++adjacent_index) {
            const int group_id = adjacent_groups[adjacent_index];
            const long long cost = move_cost(group_id);
            if (movement_cost_tier(groups_[group_id]) >= movement_cost_tier(parent)) {
                continue;
            }
            if (parent_cost + cost > root_cost) continue;

            if (++limited_component_token_ == numeric_limits<int>::max()) {
                limited_component_seen_.fill(0);
                limited_component_token_ = 1;
            }
            int unlocked_capacity = groups_[group_id].p;
            for (int c : groups_[group_id].cells) {
                const int x = c / n_, y = c % n_;
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + DX[d], ny = y + DY[d];
                    if (!inside(nx, ny)) continue;
                    const int nc = id(nx, ny);
                    if (owner_[nc] != -1) continue;
                    const int label = component_[nc];
                    if (label < 0 ||
                        limited_component_seen_[label] == limited_component_token_) {
                        continue;
                    }
                    limited_component_seen_[label] = limited_component_token_;
                    unlocked_capacity +=
                        static_cast<int>(component_cells_[label].size());
                }
            }
            if (unlocked_capacity < parent.p) continue;
            const SecondaryVictim candidate{
                group_id, limited_boundary_contact_[group_id], unlocked_capacity, cost};
            if (victim_count < LIMITED_REPACK_SECONDARY_KEEP) {
                victims[victim_count++] = candidate;
            } else if (!better_victim(candidate, victims[victim_count - 1])) {
                continue;
            } else {
                victims[victim_count - 1] = candidate;
            }
            for (int index = victim_count - 1;
                 index > 0 && better_victim(victims[index], victims[index - 1]);
                 --index) {
                swap(victims[index], victims[index - 1]);
            }
        }

        const long long arriving_fee =
            round_payment(arriving.v, arriving.p, target.arriving.perimeter);
        const double required_value =
            max(baseline_value + movement_margin(arriving_fee), incumbent_value);

        for (int victim_index = 0; victim_index < victim_count; ++victim_index) {
            const SecondaryVictim &victim = victims[victim_index];
            if (elapsed() >= depth2_deadline) break;
            const int victim_id = victim.group_id;
            const Group &victim_group = groups_[victim_id];
            const double cost_only_upper =
                static_cast<double>(arriving_fee) - opportunity -
                static_cast<double>(parent_cost + victim.cost);
            if (cost_only_upper <= max(required_value, best.value)) break;

            owner_ = parent_scratch;
            valid = true;
            for (int c : victim_group.cells) {
                if (owner_[c] != victim_id) {
                    valid = false;
                    break;
                }
                owner_[c] = -1;
            }
            if (!valid) continue;

            if (++limited_victim_cell_token_ == numeric_limits<int>::max()) {
                limited_victim_cell_seen_.fill(0);
                limited_victim_cell_token_ = 1;
            }
            for (int c : victim_group.cells) {
                limited_victim_cell_seen_[c] = limited_victim_cell_token_;
            }
            const double parent_deadline =
                depth2_deadline - LIMITED_REPACK_CHILD_RESERVE;
            if (elapsed() >= parent_deadline) break;
            prepare_free_state(false);
            vector<RelocationCandidate> parent_candidates =
                relocation_candidates(parent_id, parent_deadline);

            int examined_parent_candidates = 0;
            for (const RelocationCandidate &parent_candidate : parent_candidates) {
                if (elapsed() >= depth2_deadline ||
                    examined_parent_candidates >= LIMITED_REPACK_PARENT_KEEP) {
                    break;
                }
                if (cost_only_upper - static_cast<double>(parent_candidate.fee_loss) <=
                    max(required_value, best.value)) {
                    break;
                }
                bool uses_victim_space = false;
                for (int c : parent_candidate.placement.cells) {
                    if (limited_victim_cell_seen_[c] == limited_victim_cell_token_) {
                        uses_victim_space = true;
                        break;
                    }
                }
                if (!uses_victim_space) continue;
                ++examined_parent_candidates;

                valid = true;
                for (int c : parent_candidate.placement.cells) {
                    if (owner_[c] != -1) {
                        valid = false;
                        break;
                    }
                    owner_[c] = parent_id;
                }
                if (!valid) {
                    for (int c : parent_candidate.placement.cells) {
                        if (owner_[c] == parent_id) owner_[c] = -1;
                    }
                    continue;
                }

                prepare_free_state(false);
                vector<RelocationCandidate> victim_candidates =
                    relocation_candidates(victim_id, depth2_deadline);
                const RelocationCandidate *victim_candidate = nullptr;
                for (const RelocationCandidate &candidate : victim_candidates) {
                    if (candidate.placement.perimeter <= victim_group.max_perimeter) {
                        victim_candidate = &candidate;
                        break;
                    }
                }
                if (victim_candidate == nullptr) {
                    if (allow_depth3 && !deferred_depth3) {
                        const SecondaryVictim depth3_victim = victim;
                        const RelocationCandidate depth3_parent_candidate =
                            parent_candidate;
                        deferred_depth3 =
                            [&, depth3_victim,
                             depth3_parent_candidate](double depth3_deadline) {
                        const SecondaryVictim &chain_victim = depth3_victim;
                        const int chain_victim_id = chain_victim.group_id;
                        const Group &chain_victim_group = groups_[chain_victim_id];
                        const RelocationCandidate &chain_parent_candidate =
                            depth3_parent_candidate;

                        const double depth3_upper_before_tertiary =
                            static_cast<double>(arriving_fee) - opportunity -
                            static_cast<double>(parent_cost + chain_victim.cost + 1) -
                            static_cast<double>(chain_parent_candidate.fee_loss);
                        if (depth3_upper_before_tertiary <=
                            max(required_value, best.value)) {
                            return;
                        }

                        // Rebuild the saved depth-2 failure only after the ordinary
                        // depth-2 search has consumed its full, unchanged budget.
                        owner_ = parent_scratch;
                        bool depth3_scratch_valid = true;
                        for (int c : chain_victim_group.cells) {
                            if (owner_[c] != chain_victim_id) {
                                depth3_scratch_valid = false;
                                break;
                            }
                            owner_[c] = -1;
                        }
                        if (depth3_scratch_valid) {
                            for (int c : chain_parent_candidate.placement.cells) {
                                if (owner_[c] != -1) {
                                    depth3_scratch_valid = false;
                                    break;
                                }
                                owner_[c] = parent_id;
                            }
                        }
                        if (!depth3_scratch_valid ||
                            elapsed() + LIMITED_REPACK_DEPTH3_FINAL_RESERVE >=
                                depth3_deadline) {
                            owner_ = parent_scratch;
                            return;
                        }
                        prepare_free_state(false);
                        const double secondary_deadline =
                            depth3_deadline - LIMITED_REPACK_DEPTH3_FINAL_RESERVE;
                        auto attempt_depth3 = [&]() -> bool {
                            if (elapsed() >= secondary_deadline) return false;
                            struct TertiaryVictim {
                                int group_id = -1;
                                int boundary_contact = 0;
                                int unlocked_capacity = 0;
                                long long cost = 0;
                            };
                            auto better_tertiary = [](const TertiaryVictim &a,
                                                      const TertiaryVictim &b) {
                                if (a.cost != b.cost) return a.cost < b.cost;
                                if (a.boundary_contact != b.boundary_contact) {
                                    return a.boundary_contact > b.boundary_contact;
                                }
                                if (a.unlocked_capacity != b.unlocked_capacity) {
                                    return a.unlocked_capacity > b.unlocked_capacity;
                                }
                                return a.group_id < b.group_id;
                            };

                            if (++limited_group_token_ == numeric_limits<int>::max()) {
                                limited_group_seen_.fill(0);
                                limited_group_token_ = 1;
                            }
                            array<int, 4 * MAX_P> adjacent_tertiary{};
                            int adjacent_tertiary_count = 0;
                            for (int c : chain_victim_group.cells) {
                                if (elapsed() >= secondary_deadline) return false;
                                const int x = c / n_, y = c % n_;
                                for (int d = 0; d < 4; ++d) {
                                    const int nx = x + DX[d], ny = y + DY[d];
                                    if (!inside(nx, ny)) continue;
                                    const int group_id = owner_[id(nx, ny)];
                                    if (group_id < 0 || group_id >= m_ ||
                                        group_id == parent_id ||
                                        group_id == chain_victim_id ||
                                        !groups_[group_id].active) {
                                        continue;
                                    }
                                    if (limited_group_seen_[group_id] !=
                                        limited_group_token_) {
                                        limited_group_seen_[group_id] =
                                            limited_group_token_;
                                        limited_boundary_contact_[group_id] = 0;
                                        adjacent_tertiary[adjacent_tertiary_count++] =
                                            group_id;
                                    }
                                    ++limited_boundary_contact_[group_id];
                                }
                            }

                            array<TertiaryVictim, LIMITED_REPACK_TERTIARY_KEEP>
                                tertiary_victims{};
                            int tertiary_count = 0;
                            for (int adjacent_index = 0;
                                 adjacent_index < adjacent_tertiary_count;
                                 ++adjacent_index) {
                                if ((adjacent_index & 3) == 0 &&
                                    elapsed() >= secondary_deadline) {
                                    return false;
                                }
                                const int group_id = adjacent_tertiary[adjacent_index];
                                const Group &group = groups_[group_id];
                                const long long cost = move_cost(group_id);
                                if (movement_cost_tier(group) >=
                                    movement_cost_tier(chain_victim_group)) {
                                    continue;
                                }
                                if (parent_cost + chain_victim.cost + cost >
                                    root_cost) {
                                    continue;
                                }

                                if (++limited_component_token_ ==
                                    numeric_limits<int>::max()) {
                                    limited_component_seen_.fill(0);
                                    limited_component_token_ = 1;
                                }
                                int unlocked_capacity = group.p;
                                int capacity_cells_scanned = 0;
                                for (int c : group.cells) {
                                    if (((++capacity_cells_scanned) & 31) == 0 &&
                                        elapsed() >= secondary_deadline) {
                                        return false;
                                    }
                                    const int x = c / n_, y = c % n_;
                                    for (int d = 0; d < 4; ++d) {
                                        const int nx = x + DX[d], ny = y + DY[d];
                                        if (!inside(nx, ny)) continue;
                                        const int nc = id(nx, ny);
                                        if (owner_[nc] != -1) continue;
                                        const int label = component_[nc];
                                        if (label < 0 ||
                                            limited_component_seen_[label] ==
                                                limited_component_token_) {
                                            continue;
                                        }
                                        limited_component_seen_[label] =
                                            limited_component_token_;
                                        unlocked_capacity += static_cast<int>(
                                            component_cells_[label].size());
                                    }
                                }
                                if (unlocked_capacity < chain_victim_group.p) continue;

                                const TertiaryVictim candidate{
                                    group_id, limited_boundary_contact_[group_id],
                                    unlocked_capacity, cost};
                                if (tertiary_count < LIMITED_REPACK_TERTIARY_KEEP) {
                                    tertiary_victims[tertiary_count++] = candidate;
                                } else if (better_tertiary(candidate,
                                                           tertiary_victims[0])) {
                                    tertiary_victims[0] = candidate;
                                }
                            }
                            if (tertiary_count == 0) return false;

                            const TertiaryVictim &tertiary = tertiary_victims[0];
                            const int tertiary_id = tertiary.group_id;
                            const Group &tertiary_group = groups_[tertiary_id];
                            const long long total_move_cost =
                                parent_cost + chain_victim.cost + tertiary.cost;
                            const double cost_only_upper3 =
                                static_cast<double>(arriving_fee) - opportunity -
                                static_cast<double>(total_move_cost);
                            if (cost_only_upper3 -
                                    static_cast<double>(
                                        chain_parent_candidate.fee_loss) <=
                                max(required_value, best.value)) {
                                return false;
                            }

                            const array<int, MAX_CELLS> secondary_scratch = owner_;
                            for (int c : tertiary_group.cells) {
                                if (owner_[c] != tertiary_id) {
                                    owner_ = secondary_scratch;
                                    return false;
                                }
                                owner_[c] = -1;
                            }
                            if (++limited_tertiary_cell_token_ ==
                                numeric_limits<int>::max()) {
                                limited_tertiary_cell_seen_.fill(0);
                                limited_tertiary_cell_token_ = 1;
                            }
                            for (int c : tertiary_group.cells) {
                                limited_tertiary_cell_seen_[c] =
                                    limited_tertiary_cell_token_;
                            }

                            if (elapsed() >= secondary_deadline) {
                                owner_ = secondary_scratch;
                                return false;
                            }
                            prepare_free_state(false);
                            if (elapsed() >= secondary_deadline) {
                                owner_ = secondary_scratch;
                                return false;
                            }
                            vector<RelocationCandidate> secondary_candidates =
                                relocation_candidates(chain_victim_id,
                                                      secondary_deadline);
                            const RelocationCandidate *secondary_candidate = nullptr;
                            for (const RelocationCandidate &candidate :
                                 secondary_candidates) {
                                if (candidate.placement.perimeter >
                                    chain_victim_group.max_perimeter) {
                                    continue;
                                }
                                bool uses_tertiary_space = false;
                                for (int c : candidate.placement.cells) {
                                    if (limited_tertiary_cell_seen_[c] ==
                                        limited_tertiary_cell_token_) {
                                        uses_tertiary_space = true;
                                        break;
                                    }
                                }
                                if (!uses_tertiary_space) continue;
                                secondary_candidate = &candidate;
                                break;
                            }
                            if (secondary_candidate == nullptr) {
                                owner_ = secondary_scratch;
                                return false;
                            }

                            for (int c : secondary_candidate->placement.cells) {
                                if (owner_[c] != -1) {
                                    owner_ = secondary_scratch;
                                    return false;
                                }
                                owner_[c] = chain_victim_id;
                            }
                            if (elapsed() >= depth3_deadline) {
                                owner_ = secondary_scratch;
                                return false;
                            }
                            prepare_free_state(false);
                            if (elapsed() >= depth3_deadline) {
                                owner_ = secondary_scratch;
                                return false;
                            }
                            vector<RelocationCandidate> tertiary_candidates =
                                relocation_candidates(tertiary_id, depth3_deadline);
                            const RelocationCandidate *tertiary_candidate = nullptr;
                            for (const RelocationCandidate &candidate :
                                 tertiary_candidates) {
                                if (candidate.placement.perimeter <=
                                    tertiary_group.max_perimeter) {
                                    tertiary_candidate = &candidate;
                                    break;
                                }
                            }
                            if (tertiary_candidate == nullptr) {
                                owner_ = secondary_scratch;
                                return false;
                            }

                            const long long total_fee_loss =
                                chain_parent_candidate.fee_loss +
                                secondary_candidate->fee_loss +
                                tertiary_candidate->fee_loss;
                            const double value =
                                cost_only_upper3 - static_cast<double>(total_fee_loss);
                            if (value <= required_value || value <= best.value) {
                                owner_ = secondary_scratch;
                                return false;
                            }

                            best.moves = {
                                MoveAction{parent_id,
                                           chain_parent_candidate.placement},
                                MoveAction{chain_victim_id,
                                           secondary_candidate->placement},
                                MoveAction{tertiary_id,
                                           tertiary_candidate->placement},
                            };
                            best.arriving = target.arriving;
                            best.total_move_cost = total_move_cost;
                            best.total_fee_loss = total_fee_loss;
                            best.value = value;
                            best.limited_repack = true;
                            best.limited_repack_depth = 3;
                            owner_ = secondary_scratch;
                            return true;
                        };
                        attempt_depth3();
                        };
                    }
                    for (int c : parent_candidate.placement.cells) {
                        if (owner_[c] == parent_id) owner_[c] = -1;
                    }
                    continue;
                }

                const long long fee_loss =
                    parent_candidate.fee_loss + victim_candidate->fee_loss;
                const double value = cost_only_upper - static_cast<double>(fee_loss);
                if (value <= required_value || value <= best.value) {
                    for (int c : parent_candidate.placement.cells) {
                        if (owner_[c] == parent_id) owner_[c] = -1;
                    }
                    continue;
                }

                best.moves = {
                    MoveAction{parent_id, parent_candidate.placement},
                    MoveAction{victim_id, victim_candidate->placement},
                };
                best.arriving = target.arriving;
                best.total_move_cost = parent_cost + victim.cost;
                best.total_fee_loss = fee_loss;
                best.value = value;
                best.limited_repack = true;
                best.limited_repack_depth = 2;
                // Child loss is zero by construction and later parent candidates cannot
                // have a smaller fee loss because the list is sorted.
                break;
            }
        }

        if (deferred_depth3) {
            const double depth3_started = elapsed();
            const double depth3_budget =
                min({LIMITED_REPACK_DEPTH3_EXTRA_BUDGET,
                     max(0.0, LIMITED_REPACK_DEPTH3_TOTAL_BUDGET -
                                  limited_repack_depth3_cpu_spent_),
                     max(0.0, deadline - depth3_started)});
            if (depth3_budget > LIMITED_REPACK_DEPTH3_FINAL_RESERVE) {
                deferred_depth3(depth3_started + depth3_budget);
                limited_repack_depth3_cpu_spent_ +=
                    max(0.0, elapsed() - depth3_started);
            }
        }

        owner_ = parent_scratch;
        return best;
    }

    MovePlan find_move_plan(int arriving_id, const Group &arriving,
                            double opportunity, double baseline_value,
                            const Placement &ordinary_placement,
                            bool ordinary_accept) {
        MovePlan best;
        (void)ordinary_placement;
        (void)ordinary_accept;
        // The turn-level prepare_free_state() already built these prefixes for arriving.t.
        if (free_count_ < arriving.p || elapsed() >= MOVE_START_DEADLINE) return best;

        long long minimum_move_cost = numeric_limits<long long>::max();
        for (int group_id = 0; group_id < arriving_id; ++group_id) {
            if (groups_[group_id].active) {
                minimum_move_cost = min(minimum_move_cost, move_cost(group_id));
            }
        }
        if (minimum_move_cost == numeric_limits<long long>::max()) return best;

        const int ideal_perimeter = minimum_shape_perimeter(arriving.p);
        const long long ideal_fee =
            round_payment(arriving.v, arriving.p, ideal_perimeter);
        if (static_cast<double>(ideal_fee) - opportunity -
                static_cast<double>(minimum_move_cost) <=
            baseline_value + movement_margin(ideal_fee)) {
            return best;
        }

        const int remaining_turns = m_ - arriving_id - 1;
        const double repair_deadline =
            min(MOVE_REPAIR_DEADLINE,
                MOVE_TAIL_BASE_DEADLINE - INTERACTIVE_TAIL_PER_TURN * remaining_turns);
        const double exact_deadline = min(MOVE_EXACT_DEADLINE, repair_deadline - 0.020);
        const double enumeration_deadline =
            min(MOVE_ENUM_DEADLINE, exact_deadline - 0.025);
        vector<MoveTargetCandidate> targets =
            enumerate_move_targets(arriving, opportunity, baseline_value, minimum_move_cost,
                                   enumeration_deadline, exact_deadline);
        if (targets.empty() ||
            elapsed() > min(MOVE_REPAIR_START_DEADLINE, repair_deadline)) {
            return best;
        }
        const array<int, MAX_CELLS> base_owner = owner_;

        int limited_repack_attempts = 0;
        constexpr int limited_repack_attempt_limit = 1;
        const bool depth3_value_eligible = depth3_high_absolute_value(arriving);
        auto try_limited_repack = [&](const MoveTargetCandidate &target) {
            const bool target_kind_eligible = !target.atlas;
            const bool cheaper_parent =
                movement_cost_tier(groups_[target.blockers.front()]) <
                    movement_cost_tier(arriving);
            const bool eligible =
                target.blockers.size() == 1 && target_kind_eligible && cheaper_parent;
            if (!eligible || limited_repack_attempts >= limited_repack_attempt_limit ||
                limited_repack_cpu_spent_ >= LIMITED_REPACK_TOTAL_BUDGET ||
                LIMITED_REPACK_TOTAL_BUDGET - limited_repack_cpu_spent_ <
                    LIMITED_REPACK_MIN_START_BUDGET ||
                elapsed() + LIMITED_REPACK_MIN_START_BUDGET >= repair_deadline) {
                return;
            }
            const long long arriving_fee =
                round_payment(arriving.v, arriving.p, target.arriving.perimeter);
            const double required =
                max(best.value,
                    baseline_value + movement_margin(arriving_fee));
            // A second moved group costs at least one point and fee losses are nonnegative.
            if (target.upper_value - 1.0 <= required) return;

            ++limited_repack_attempts;
            const double started = elapsed();
            const double remaining_budget =
                LIMITED_REPACK_TOTAL_BUDGET - limited_repack_cpu_spent_;
            const double remaining_depth3_budget =
                LIMITED_REPACK_DEPTH3_TOTAL_BUDGET -
                limited_repack_depth3_cpu_spent_;
            const bool allow_depth3 =
                depth3_value_eligible &&
                remaining_budget >= LIMITED_REPACK_LOCAL_BUDGET &&
                remaining_depth3_budget >= LIMITED_REPACK_DEPTH3_EXTRA_BUDGET &&
                started + LIMITED_REPACK_LOCAL_BUDGET +
                              LIMITED_REPACK_DEPTH3_EXTRA_BUDGET <
                    repair_deadline;
            const double local_budget =
                min(LIMITED_REPACK_LOCAL_BUDGET, remaining_budget) +
                (allow_depth3 ? LIMITED_REPACK_DEPTH3_EXTRA_BUDGET : 0.0);
            const double local_deadline =
                min(repair_deadline, started + local_budget);
            const array<int, MAX_CELLS> parent_scratch = owner_;
            const double depth3_spent_before =
                limited_repack_depth3_cpu_spent_;
            MovePlan limited_plan = solve_limited_repack_chain(
                target, parent_scratch, arriving, opportunity, baseline_value,
                best.value, allow_depth3, local_deadline);
            const double total_spent = max(0.0, elapsed() - started);
            const double depth3_spent =
                max(0.0, limited_repack_depth3_cpu_spent_ -
                             depth3_spent_before);
            // Depth 3 is paid from its own spare-time budget; it must not narrow
            // the proven depth-2 search on later turns.
            limited_repack_cpu_spent_ += max(0.0, total_spent - depth3_spent);
            if (limited_plan.valid() && limited_plan.value > best.value) {
                best = move(limited_plan);
            }
        };
        for (const MoveTargetCandidate &target : targets) {
            if (elapsed() > repair_deadline) break;

            if (target.blockers.size() > 3) {
                if (target.atlas && atlas_repack_enabled(arriving) &&
                    elapsed() + 0.010 < repair_deadline) {
                    const double atlas_deadline =
                        min(repair_deadline, elapsed() + 0.060);
                    MovePlan atlas_plan = solve_atlas_repack(
                        target, base_owner, arriving_id, arriving, opportunity,
                        baseline_value, atlas_deadline);
                    if (atlas_plan.valid() && atlas_plan.value > best.value) {
                        best = move(atlas_plan);
                    }
                }
                continue;
            }
            owner_ = base_owner;
            bool scratch_valid = true;
            for (int group_id : target.blockers) {
                for (int c : groups_[group_id].cells) {
                    if (owner_[c] != group_id) {
                        scratch_valid = false;
                        break;
                    }
                    owner_[c] = -1;
                }
                if (!scratch_valid) break;
            }
            if (!scratch_valid) continue;
            for (int c : target.arriving.cells) {
                if (owner_[c] != -1) {
                    scratch_valid = false;
                    break;
                }
                owner_[c] = arriving_id;
            }
            if (!scratch_valid) continue;

            prepare_free_state(false);
            vector<vector<RelocationCandidate>> candidate_lists;
            candidate_lists.reserve(target.blockers.size());
            for (int group_id : target.blockers) {
                candidate_lists.push_back(
                    relocation_candidates(group_id, repair_deadline));
                if (candidate_lists.back().empty() || elapsed() > repair_deadline) {
                    scratch_valid = false;
                    break;
                }
            }
            if (!scratch_valid) {
                try_limited_repack(target);
                continue;
            }

            long long fixed_move_cost = 0;
            for (int group_id : target.blockers) fixed_move_cost += move_cost(group_id);
            const long long arriving_fee = round_payment(
                arriving.v, arriving.p, target.arriving.perimeter);
            const double margin = movement_margin(arriving_fee);
            array<unsigned char, MAX_CELLS> chosen_cells{};
            array<int, 3> chosen_candidate{};
            size_t dfs_nodes = 0;
            bool timed_out = false;

            auto dfs = [&](auto &&self, size_t index, long long fee_loss) -> void {
                if (((++dfs_nodes) & 255U) == 0U && elapsed() > repair_deadline) {
                    timed_out = true;
                    return;
                }
                const double upper = static_cast<double>(arriving_fee) - opportunity -
                                     static_cast<double>(fixed_move_cost) -
                                     static_cast<double>(fee_loss);
                const double required = max(baseline_value + margin, best.value);
                if (upper <= required) return;
                if (index == target.blockers.size()) {
                    vector<MoveAction> actions;
                    actions.reserve(target.blockers.size());
                    for (size_t move_index = 0; move_index < target.blockers.size();
                         ++move_index) {
                        actions.push_back(
                            {target.blockers[move_index],
                             candidate_lists[move_index]
                                 [chosen_candidate[move_index]]
                                     .placement});
                    }
                    best.moves = move(actions);
                    best.arriving = target.arriving;
                    best.total_move_cost = fixed_move_cost;
                    best.total_fee_loss = fee_loss;
                    best.value = upper;
                    return;
                }

                for (const RelocationCandidate &candidate : candidate_lists[index]) {
                    bool overlaps = false;
                    for (int c : candidate.placement.cells) {
                        if (chosen_cells[c]) {
                            overlaps = true;
                            break;
                        }
                    }
                    if (overlaps) continue;
                    for (int c : candidate.placement.cells) chosen_cells[c] = 1;
                    chosen_candidate[index] = static_cast<int>(
                        &candidate - candidate_lists[index].data());
                    self(self, index + 1, fee_loss + candidate.fee_loss);
                    for (int c : candidate.placement.cells) chosen_cells[c] = 0;
                    if (timed_out) return;
                }
            };
            dfs(dfs, 0, 0);
            if (!timed_out) try_limited_repack(target);
            if (timed_out) break;
        }

        owner_ = base_owner;
        return best;
    }

    bool place_region_on_board(const Placement &placement, int p, int group_id,
                               array<int, MAX_CELLS> &board) const {
        int perimeter = 0;
        uint64_t hash = 0;
        if (!valid_shape(placement.cells, p, perimeter, hash) ||
            perimeter != placement.perimeter) {
            return false;
        }
        for (int c : placement.cells) {
            if (board[c] != -1) return false;
        }
        for (int c : placement.cells) board[c] = group_id;
        return true;
    }

    bool validate_move_plan(const MovePlan &plan, int arriving_id,
                            const Group &arriving) const {
        if (!plan.valid() || plan.moves.size() > ATLAS_REPACK_MAX_MOVES ||
            groups_[arriving_id].active) {
            return false;
        }
        array<int, MAX_CELLS> board = owner_;
        vector<unsigned char> moved(m_, 0);
        long long checked_move_cost = 0;
        long long checked_fee_loss = 0;
        for (const MoveAction &action : plan.moves) {
            const int group_id = action.group_id;
            if (group_id < 0 || group_id >= m_ || !groups_[group_id].active ||
                moved[group_id]) {
                return false;
            }
            moved[group_id] = 1;
            checked_move_cost += move_cost(group_id);
            const Group &group = groups_[group_id];
            const long long old_fee =
                round_payment(group.v, group.p, group.max_perimeter);
            const int new_max_perimeter =
                max(group.max_perimeter, action.placement.perimeter);
            checked_fee_loss +=
                old_fee - round_payment(group.v, group.p, new_max_perimeter);
        }
        for (const MoveAction &action : plan.moves) {
            const int group_id = action.group_id;
            for (int c : groups_[group_id].cells) {
                if (c < 0 || c >= n_ * n_ || board[c] != group_id) return false;
                board[c] = -1;
            }
        }
        for (int c = 0; c < n_ * n_; ++c) {
            if (board[c] >= m_) return false;
            if (board[c] >= 0 && moved[board[c]]) return false;
        }
        if (checked_move_cost != plan.total_move_cost ||
            checked_fee_loss != plan.total_fee_loss) {
            return false;
        }
        for (const MoveAction &action : plan.moves) {
            if (!place_region_on_board(action.placement, groups_[action.group_id].p,
                                       action.group_id, board)) {
                return false;
            }
        }
        return place_region_on_board(plan.arriving, arriving.p, arriving_id, board);
    }

    void commit_move_plan(const MovePlan &plan, int arriving_id, long long departure_time) {
        for (const MoveAction &action : plan.moves) {
            for (int c : groups_[action.group_id].cells) {
                if (owner_[c] == action.group_id) owner_[c] = -1;
            }
        }
        for (const MoveAction &action : plan.moves) {
            Group &group = groups_[action.group_id];
            for (int c : action.placement.cells) owner_[c] = action.group_id;
            group.cells = action.placement.cells;
            group.max_perimeter = max(group.max_perimeter, action.placement.perimeter);
        }
        for (int c : plan.arriving.cells) owner_[c] = arriving_id;
        groups_[arriving_id].active = true;
        groups_[arriving_id].cells = plan.arriving.cells;
        groups_[arriving_id].max_perimeter = plan.arriving.perimeter;
        departures_.push({departure_time, arriving_id});
    }

    double normal_cdf(double x) const { return 0.5 * erfc(-x / SQRT2); }

    double inverse_normal_cdf(double probability) const {
        double lo = -8.0, hi = 8.0;
        for (int iter = 0; iter < 60; ++iter) {
            const double mid = (lo + hi) * 0.5;
            if (normal_cdf(mid) < probability)
                lo = mid;
            else
                hi = mid;
        }
        return (lo + hi) * 0.5;
    }

    double low_value_cutoff() const {
        // A long prior prevents the first few arrivals from producing an aggressive cutoff.
        constexpr long double PRIOR_WEIGHT = 16.0L;
        constexpr long double PRIOR_THETA = 8000.0L;
        const double gate_theta = clamp(
            static_cast<double>((sum_duration_ + PRIOR_WEIGHT * PRIOR_THETA) /
                                (observed_ + PRIOR_WEIGHT)),
            2000.0, 8000.0);
        return LOW_VALUE_BASE_CUTOFF * pow(gate_theta / 8000.0, -0.1);
    }

    bool is_low_value(const Group &g) const {
        const long long duration = g.t - g.s;
        if (duration <= 0 || g.p <= 0) return true;
        const long double offered_density =
            static_cast<long double>(g.v) /
            (static_cast<long double>(g.p) * duration);
        return offered_density < low_value_cutoff();
    }

    double acceptance_threshold(int turn) const {
        if (turn + 1 == m_) return 0.0;
        // Keep the global distribution-derived price, but replace the single current-occupancy
        // multiplier with a duration-weighted price curve over future time buckets.
        constexpr double PRIOR = 16.0;
        const double mean_p_duration =
            static_cast<double>((sum_p_duration_ + PRIOR * 59.5L * 5000.0L) /
                                (observed_ + PRIOR));
        const double theta_hat = clamp(
            static_cast<double>((sum_duration_ + PRIOR * 5000.0L) / (observed_ + PRIOR)),
            2000.0, 8000.0);
        const double progress = static_cast<double>(turn) / max(1, m_ - 1);
        const double target_utilization = 0.86 + 0.03 * progress;
        const double offered_load = m_ * mean_p_duration /
                                    (static_cast<double>(usable_grass_) * 100000.0);
        if (offered_load <= target_utilization) return 0.0;

        const double accepted_resource_fraction =
            clamp(target_utilization / offered_load, 0.001, 0.999);
        const double z = inverse_normal_cdf(1.0 - accepted_resource_fraction);
        const double log2_mean = -0.1 * (log2(theta_hat) + 0.610);
        double threshold = 0.93 * exp2(log2_mean + 0.81 * z);

        const int remaining_arrivals = m_ - turn - 1;
        if (remaining_arrivals < 120) threshold *= remaining_arrivals / 120.0;

        const Group &current = groups_[turn];
        const long long duration = current.t - current.s;
        const long long horizon = 100000 - current.s;
        if (duration <= 0 || horizon <= 0) return threshold;

        constexpr array<long long, 9> BUCKET_END{
            500, 1000, 2000, 4000, 8000, 16000, 32000, 64000, 100000,
        };
        const double arrival_rate =
            static_cast<double>(remaining_arrivals) /
            static_cast<double>(max(1LL, horizon));
        const double mean_p = clamp(mean_p_duration / theta_hat, 4.0, 150.0);
        double weighted_factor = 0.0;
        long long covered_duration = 0;
        long long lo = 0;
        for (long long raw_hi : BUCKET_END) {
            const long long hi = min(raw_hi, horizon);
            if (hi <= lo) continue;

            long double confirmed_cell_days = 0.0L;
            for (int group_id = 0; group_id < turn; ++group_id) {
                const Group &group = groups_[group_id];
                if (!group.active) continue;
                const long long remaining = max(0LL, group.t - current.s);
                const long long overlap = max(0LL, min(hi, remaining) - lo);
                confirmed_cell_days += static_cast<long double>(group.p) * overlap;
            }
            const double width = static_cast<double>(hi - lo);
            const double confirmed_occupancy =
                static_cast<double>(confirmed_cell_days / width);

            // Future starts are approximately uniform over the remaining horizon.  With an
            // exponential stay, the expected active exposure at lag x integrates analytically.
            const double exp_lo = exp(-static_cast<double>(lo) / theta_hat);
            const double exp_hi = exp(-static_cast<double>(hi) / theta_hat);
            const double mean_active_exposure =
                theta_hat - theta_hat * theta_hat * (exp_lo - exp_hi) / width;
            const double future_occupancy =
                arrival_rate * mean_p * accepted_resource_fraction * mean_active_exposure;
            const double projected_ratio =
                (confirmed_occupancy + future_occupancy) / max(1, usable_grass_);
            const double bucket_factor =
                clamp(exp2(2.4 * (projected_ratio - 0.78)), 0.45, 2.0);

            const long long task_overlap = max(0LL, min(hi, duration) - lo);
            weighted_factor += bucket_factor * static_cast<double>(task_overlap);
            covered_duration += task_overlap;
            lo = hi;
            if (lo >= horizon || lo >= duration) break;
        }
        if (covered_duration > 0) {
            threshold *= weighted_factor / static_cast<double>(covered_duration);
        }
        return threshold;
    }

    bool should_accept(const Group &g, const Placement &placement, double threshold) const {
        const long long duration = g.t - g.s;
        if (duration <= 0) return false;
        const double predicted_fee = static_cast<double>(g.v) * placement.compactness;
        const double density =
            predicted_fee / (static_cast<double>(g.p) * static_cast<double>(duration));
        return density >= threshold;
    }

    bool validate_region(const vector<int> &cells, int p) {
        if (static_cast<int>(cells.size()) != p) return false;
        ++region_token_;
        if (region_token_ == numeric_limits<int>::max()) {
            region_mark_.fill(0);
            region_token_ = 1;
        }
        for (int c : cells) {
            if (c < 0 || c >= n_ * n_ || owner_[c] != -1 || region_mark_[c] == region_token_) {
                return false;
            }
            region_mark_[c] = region_token_;
        }

        ++seen_token_;
        if (seen_token_ == numeric_limits<int>::max()) {
            seen_mark_.fill(0);
            seen_token_ = 1;
        }
        vector<int> stack = {cells[0]};
        seen_mark_[cells[0]] = seen_token_;
        int reached = 0;
        while (!stack.empty()) {
            const int c = stack.back();
            stack.pop_back();
            ++reached;
            const int x = c / n_, y = c % n_;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + DX[d], ny = y + DY[d];
                if (!inside(nx, ny)) continue;
                const int nc = id(nx, ny);
                if (region_mark_[nc] == region_token_ && seen_mark_[nc] != seen_token_) {
                    seen_mark_[nc] = seen_token_;
                    stack.push_back(nc);
                }
            }
        }
        return reached == p;
    }
};

int main() {
    Solver solver;
    solver.run();
    return 0;
}
