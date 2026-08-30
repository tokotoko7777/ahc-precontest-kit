#include <bits/stdc++.h>
using namespace std;

constexpr int GRID_SIZE = 30;
constexpr int VERTEX_COUNT = GRID_SIZE * GRID_SIZE;
constexpr int HORIZONTAL_EDGE_COUNT = GRID_SIZE * (GRID_SIZE - 1);
constexpr int EDGE_COUNT = 2 * HORIZONTAL_EDGE_COUNT;

// 29本の辺を前半・後半の2区間に分ける。
// 真の入力も各行・各列で高々2区間なので、1辺ずつ完全に独立に学ぶより安定する。
constexpr int BUCKET_COUNT = 2;
constexpr double EXPLORATION_RATE = 1.65;
constexpr double EDGE_LEARNING_WEIGHT = 2.0;
constexpr double START_LEARNING_RATE = 0.70;
constexpr double END_LEARNING_RATE = 0.30;
constexpr int FEATURE_COUNT = 2 * GRID_SIZE * BUCKET_COUNT;
constexpr int LINE_COUNT = 2 * GRID_SIZE;

int horizontal_edge_id(int row, int left_column) {
  return row * (GRID_SIZE - 1) + left_column;
}

int vertical_edge_id(int upper_row, int column) {
  return HORIZONTAL_EDGE_COUNT + upper_row * GRID_SIZE + column;
}

int edge_feature(int edge) {
  if (edge < HORIZONTAL_EDGE_COUNT) {
    const int row = edge / (GRID_SIZE - 1);
    const int column = edge % (GRID_SIZE - 1);
    const int bucket = min(
        BUCKET_COUNT - 1,
        column * BUCKET_COUNT / (GRID_SIZE - 1));
    return row * BUCKET_COUNT + bucket;
  }

  const int local_edge = edge - HORIZONTAL_EDGE_COUNT;
  const int row = local_edge / GRID_SIZE;
  const int column = local_edge % GRID_SIZE;
  const int bucket = min(
      BUCKET_COUNT - 1,
      row * BUCKET_COUNT / (GRID_SIZE - 1));
  return GRID_SIZE * BUCKET_COUNT + column * BUCKET_COUNT + bucket;
}

int edge_line(int edge) {
  if (edge < HORIZONTAL_EDGE_COUNT) {
    return edge / (GRID_SIZE - 1);
  }
  return GRID_SIZE + (edge - HORIZONTAL_EDGE_COUNT) % GRID_SIZE;
}

struct OnlineEdgeEstimator {
  array<double, LINE_COUNT> line_cost{};
  array<double, LINE_COUNT> line_information{};
  array<double, FEATURE_COUNT> bucket_adjustment{};
  array<double, FEATURE_COUNT> feature_information{};
  array<double, EDGE_COUNT> edge_adjustment{};
  array<int, EDGE_COUNT> edge_use_count{};

  OnlineEdgeEstimator() { line_cost.fill(5000.0); }

  double estimated_cost(int edge) const {
    return clamp(
        line_cost[edge_line(edge)] +
        bucket_adjustment[edge_feature(edge)] + edge_adjustment[edge],
        1000.0,
        9000.0);
  }

  // 未観測の辺を少し安く見積もることで、序盤だけ情報を集める。
  array<int, EDGE_COUNT> planning_costs(int turn) const {
    array<int, EDGE_COUNT> result{};
    const double progress = static_cast<double>(turn) / 1000.0;
    const double exploration = 1.0 - progress;

    for (int edge = 0; edge < EDGE_COUNT; ++edge) {
      const int feature = edge_feature(edge);
      const int line = edge_line(edge);
      const double line_uncertainty =
          650.0 / sqrt(1.0 + line_information[line] / 16.0);
      const double feature_uncertainty =
          450.0 / sqrt(1.0 + feature_information[feature] / 8.0);
      const double edge_uncertainty =
          250.0 / sqrt(1.0 + edge_use_count[edge]);
      const double bonus =
          exploration * EXPLORATION_RATE *
          (line_uncertainty + feature_uncertainty + edge_uncertainty);
      result[edge] = static_cast<int>(
          lround(max(1000.0, estimated_cost(edge) - bonus)));
    }
    return result;
  }

  // 観測値は「通った全辺の合計」なので、正規化した勾配降下で誤差を分配する。
  void update(const vector<int>& path_edges, int observed_length, int turn) {
    array<int, LINE_COUNT> line_count{};
    array<int, FEATURE_COUNT> feature_count{};
    double predicted_length = 0.0;
    for (int edge : path_edges) {
      ++line_count[edge_line(edge)];
      ++feature_count[edge_feature(edge)];
      predicted_length += estimated_cost(edge);
    }

    double denominator =
        EDGE_LEARNING_WEIGHT * path_edges.size();
    for (int count : line_count) denominator += 1.0 * count * count;
    for (int count : feature_count) denominator += 1.0 * count * count;

    double error = observed_length - predicted_length;
    // 返却値には最大約10%の乗算ノイズがある。極端な1観測で壊さない。
    const double error_limit = max(6000.0, 0.30 * predicted_length);
    error = clamp(error, -error_limit, error_limit);

    const double progress = static_cast<double>(turn) / 1000.0;
    const double learning_rate =
        START_LEARNING_RATE * (1.0 - progress) +
        END_LEARNING_RATE * progress;
    const double step = learning_rate * error / denominator;

    for (int line = 0; line < LINE_COUNT; ++line) {
      const int count = line_count[line];
      if (count == 0) continue;
      line_cost[line] = clamp(
          line_cost[line] + step * count,
          1000.0,
          9000.0);
      line_information[line] += 1.0 * count * count;
    }
    for (int feature = 0; feature < FEATURE_COUNT; ++feature) {
      const int count = feature_count[feature];
      if (count == 0) continue;
      bucket_adjustment[feature] = clamp(
          bucket_adjustment[feature] + step * count,
          -3000.0,
          3000.0);
      feature_information[feature] += 1.0 * count * count;
    }
    for (int edge : path_edges) {
      edge_adjustment[edge] = clamp(
          edge_adjustment[edge] + EDGE_LEARNING_WEIGHT * step,
          -2200.0,
          2200.0);
      ++edge_use_count[edge];
    }
  }
};

struct Path {
  string moves;
  vector<int> edges;
};

Path shortest_path(
    int start_row,
    int start_column,
    int target_row,
    int target_column,
    const array<int, EDGE_COUNT>& edge_cost) {
  const int start = start_row * GRID_SIZE + start_column;
  const int target = target_row * GRID_SIZE + target_column;
  constexpr int INF = numeric_limits<int>::max() / 4;

  array<int, VERTEX_COUNT> distance;
  array<int, VERTEX_COUNT> parent;
  array<int, VERTEX_COUNT> parent_edge;
  array<char, VERTEX_COUNT> parent_move;
  distance.fill(INF);
  parent.fill(-1);

  using QueueEntry = pair<int, int>;
  priority_queue<QueueEntry, vector<QueueEntry>, greater<QueueEntry>> queue;
  distance[start] = 0;
  parent[start] = start;
  queue.push({0, start});

  auto relax = [&](int from, int to, int edge, char move) {
    const int next_distance = distance[from] + edge_cost[edge];
    if (next_distance >= distance[to]) return;
    distance[to] = next_distance;
    parent[to] = from;
    parent_edge[to] = edge;
    parent_move[to] = move;
    queue.push({next_distance, to});
  };

  while (!queue.empty()) {
    const auto [current_distance, vertex] = queue.top();
    queue.pop();
    if (current_distance != distance[vertex]) continue;
    if (vertex == target) break;

    const int row = vertex / GRID_SIZE;
    const int column = vertex % GRID_SIZE;
    if (row > 0) {
      relax(vertex, vertex - GRID_SIZE,
            vertical_edge_id(row - 1, column), 'U');
    }
    if (row + 1 < GRID_SIZE) {
      relax(vertex, vertex + GRID_SIZE,
            vertical_edge_id(row, column), 'D');
    }
    if (column > 0) {
      relax(vertex, vertex - 1,
            horizontal_edge_id(row, column - 1), 'L');
    }
    if (column + 1 < GRID_SIZE) {
      relax(vertex, vertex + 1,
            horizontal_edge_id(row, column), 'R');
    }
  }

  string reversed_moves;
  vector<int> reversed_edges;
  for (int vertex = target; vertex != start; vertex = parent[vertex]) {
    assert(parent[vertex] != -1);
    reversed_moves += parent_move[vertex];
    reversed_edges.push_back(parent_edge[vertex]);
  }
  reverse(reversed_moves.begin(), reversed_moves.end());
  reverse(reversed_edges.begin(), reversed_edges.end());
  return {move(reversed_moves), move(reversed_edges)};
}

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  OnlineEdgeEstimator estimator;

  for (int turn = 0; turn < 1000; ++turn) {
    int start_row, start_column, target_row, target_column;
    cin >> start_row >> start_column >> target_row >> target_column;

    const auto edge_cost = estimator.planning_costs(turn);
    const Path path = shortest_path(
        start_row,
        start_column,
        target_row,
        target_column,
        edge_cost);

    cout << path.moves << endl;  // endlで対話出力をflushする

    int observed_length;
    cin >> observed_length;
    estimator.update(path.edges, observed_length, turn);
  }
}
