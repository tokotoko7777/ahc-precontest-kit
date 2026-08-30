#include <bits/stdc++.h>
using namespace std;

struct Timer {
  chrono::steady_clock::time_point start = chrono::steady_clock::now();
  double elapsed_ms() const {
    return chrono::duration<double, milli>(chrono::steady_clock::now() - start)
        .count();
  }
};

struct Random {
  uint64_t state;
  explicit Random(uint64_t seed = 1) : state(seed) {}
  uint64_t next_u64() {
    state += 0x9e3779b97f4a7c15ULL;
    uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
  }
  int next_int(int limit) { return static_cast<int>(next_u64() % limit); }
};

struct Point {
  int x;
  int y;
  bool operator==(const Point& other) const {
    return x == other.x && y == other.y;
  }
};

struct RectangleMove {
  array<Point, 4> point;
  int perimeter = 0;
  int gain = 0;
};

struct Board {
  int n = 0;
  vector<char> has_point;
  vector<array<unsigned char, 8>> used_edge;
  vector<RectangleMove> moves;
  long long added_weight = 0;

  explicit Board(int size = 0)
      : n(size), has_point(size * size, false), used_edge(size * size) {}

  // 2次元座標を1次元vectorの添字に変換する。
  int id(Point p) const { return p.x * n + p.y; }
};

struct Solver {
  // 制限は5秒。出力と実行環境の揺れのために0.4秒残す。
  static constexpr double END_MS = 4600.0;
  // 右、右上、上、左上、左、左下、下、右下の順。
  // 反対向きは direction ^ 4、直角左向きは direction + 2 になる。
  const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
  const int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};

  int n = 0;
  int initial_count = 0;
  Board initial;
  Timer timer;
  Random random;

  bool inside(Point p) const {
    return 0 <= p.x && p.x < n && 0 <= p.y && p.y < n;
  }

  int weight(Point p) const {
    const int center = (n - 1) / 2;
    const int x = p.x - center;
    const int y = p.y - center;
    return x * x + y * y + 1;
  }

  void read_input() {
    cin >> n >> initial_count;
    initial = Board(n);
    uint64_t seed = 1469598103934665603ULL;
    for (int index = 0; index < initial_count; ++index) {
      Point p;
      cin >> p.x >> p.y;
      initial.has_point[initial.id(p)] = true;
      seed ^= static_cast<uint64_t>(p.x * 64 + p.y);
      seed *= 1099511628211ULL;
    }
    random = Random(seed);
  }

  int direction_of(Point from, Point to) const {
    const int sx = (to.x - from.x > 0) - (to.x - from.x < 0);
    const int sy = (to.y - from.y > 0) - (to.y - from.y < 0);
    for (int direction = 0; direction < 8; ++direction) {
      if (dx[direction] == sx && dy[direction] == sy) return direction;
    }
    return -1;
  }

  Point nearest_point(const Board& board, Point start,
                      int direction) const {
    Point current{start.x + dx[direction], start.y + dy[direction]};
    while (inside(current)) {
      if (board.has_point[board.id(current)]) return current;
      current.x += dx[direction];
      current.y += dy[direction];
    }
    return {-1, -1};
  }

  bool can_apply(const Board& board, const RectangleMove& move) const {
    const auto& p = move.point;
    if (!inside(p[0]) || board.has_point[board.id(p[0])]) return false;
    for (int index = 1; index < 4; ++index) {
      if (!inside(p[index]) || !board.has_point[board.id(p[index])]) {
        return false;
      }
    }

    const int ax = p[1].x - p[0].x;
    const int ay = p[1].y - p[0].y;
    const int bx = p[3].x - p[0].x;
    const int by = p[3].y - p[0].y;
    if (ax * bx + ay * by != 0) return false;
    if (ax != 0 && ay != 0 && abs(ax) != abs(ay)) return false;
    if (!(Point{p[1].x + bx, p[1].y + by} == p[2])) return false;

    // 4辺を単位線分に分けて調べる。
    // 途中の点は不可。既存の線分と同じ単位線分を使うのも不可。
    // 線分が一点で交差すること自体は許される。
    for (int side = 0; side < 4; ++side) {
      Point current = p[side];
      const Point target = p[(side + 1) % 4];
      const int direction = direction_of(current, target);
      if (direction == -1) return false;
      while (!(current == target)) {
        if (!(current == p[side]) &&
            board.has_point[board.id(current)]) {
          return false;
        }
        if (board.used_edge[board.id(current)][direction]) return false;
        current.x += dx[direction];
        current.y += dy[direction];
        if (board.used_edge[board.id(current)][direction ^ 4]) return false;
      }
    }
    return true;
  }

  void apply(Board& board, const RectangleMove& move) const {
    board.has_point[board.id(move.point[0])] = true;
    board.moves.push_back(move);
    board.added_weight += move.gain;
    for (int side = 0; side < 4; ++side) {
      Point current = move.point[side];
      const Point target = move.point[(side + 1) % 4];
      const int direction = direction_of(current, target);
      while (!(current == target)) {
        board.used_edge[board.id(current)][direction] = true;
        current.x += dx[direction];
        current.y += dy[direction];
        board.used_edge[board.id(current)][direction ^ 4] = true;
      }
    }
  }

  vector<RectangleMove> generate_candidates(const Board& board) const {
    vector<RectangleMove> candidates;
    // 新しい点の対角に来る既存点を opposite とする。
    // 合法な辺の途中には点がないため、残り2頂点は opposite から
    // 直角2方向に見た「最初の点」でなければならない。
    // よって各既存点×8方向だけを調べれば、合法候補を漏らさない。
    for (int x = 0; x < n; ++x) {
      for (int y = 0; y < n; ++y) {
        const Point opposite{x, y};
        if (!board.has_point[board.id(opposite)]) continue;
        for (int direction = 0; direction < 8; ++direction) {
          const Point second = nearest_point(board, opposite, direction);
          const Point fourth =
              nearest_point(board, opposite, (direction + 2) % 8);
          if (second.x == -1 || fourth.x == -1) continue;
          const Point added{second.x + fourth.x - opposite.x,
                            second.y + fourth.y - opposite.y};
          if (!inside(added)) continue;

          RectangleMove move;
          move.point = {added, second, opposite, fourth};
          move.gain = weight(added);
          move.perimeter = 2 * (max(abs(second.x - opposite.x),
                                    abs(second.y - opposite.y)) +
                                max(abs(fourth.x - opposite.x),
                                    abs(fourth.y - opposite.y)));
          if (can_apply(board, move)) candidates.push_back(move);
        }
      }
    }
    return candidates;
  }

  long long priority(const RectangleMove& move, int mode,
                     int noise_size) {
    // 外側の高得点を重視する式と、短い辺を重視する式を混ぜる。
    // どれか1種類だけでは入力ごとの得手不得手が大きかった。
    long long value = 0;
    if (mode == 0) value = 10000LL * move.gain - 150LL * move.perimeter;
    if (mode == 1) value = 100000LL * move.gain / move.perimeter;
    if (mode == 2) value = 3000LL * move.gain - 1000LL * move.perimeter;
    if (mode == 3) value = 100000LL * move.gain / (move.perimeter + 8);
    if (mode == 4) {
      value = 1000LL * move.gain * move.gain / move.perimeter;
    }
    if (mode == 5) {
      value = 1000000LL * move.gain /
              (move.perimeter * move.perimeter);
    }
    if (noise_size > 0) value += random.next_int(noise_size);
    return value;
  }

  Board build_greedily(int mode, int top_k, int noise_size) {
    Board board = initial;
    while (timer.elapsed_ms() < END_MS) {
      vector<RectangleMove> candidates = generate_candidates(board);
      if (candidates.empty()) break;
      vector<pair<long long, int>> order;
      order.reserve(candidates.size());
      for (int index = 0; index < static_cast<int>(candidates.size());
           ++index) {
        order.push_back({priority(candidates[index], mode, noise_size), index});
      }
      const int take = min(top_k, static_cast<int>(order.size()));
      partial_sort(order.begin(), order.begin() + take, order.end(),
                   greater<pair<long long, int>>());
      // 決定的構築では1位、ランダム構築では上位候補から選ぶ。
      int rank = 0;
      if (take > 1) {
        rank = min(random.next_int(take), random.next_int(take));
      }
      apply(board, candidates[order[rank].second]);
    }
    return board;
  }

  bool validate(const Board& answer) const {
    // 念のため、初期盤から全手をもう一度適用して合法性を確認する。
    Board replay = initial;
    for (const RectangleMove& move : answer.moves) {
      if (!can_apply(replay, move)) return false;
      apply(replay, move);
    }
    return true;
  }

  Board solve() {
    Board best = initial;
    // まず6種類の決定的貪欲を作り、その中の最良を必ず保持する。
    for (int mode = 0; mode < 6; ++mode) {
      Board candidate = build_greedily(mode, 1, 0);
      if (candidate.added_weight > best.added_weight) best = move(candidate);
    }

    // 残り時間では選び方を少しずつ変えて、最初から何度も構築する。
    int attempt = 0;
    while (timer.elapsed_ms() < END_MS) {
      const int mode = attempt % 6;
      const int top_k = 2 + random.next_int(7);
      const int noise = random.next_int(50000);
      Board candidate = build_greedily(mode, top_k, noise);
      if (candidate.added_weight > best.added_weight) best = move(candidate);
      ++attempt;
    }
    if (!validate(best)) return initial;
    return best;
  }
};

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  Solver solver;
  solver.read_input();
  const Board answer = solver.solve();
  cout << answer.moves.size() << '\n';
  for (const RectangleMove& move : answer.moves) {
    for (int index = 0; index < 4; ++index) {
      if (index) cout << ' ';
      cout << move.point[index].x << ' ' << move.point[index].y;
    }
    cout << '\n';
  }
}
