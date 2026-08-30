# 使い方例

コンテスト中は、使いたい `.hpp` の中身を `main.cpp` の `main` 関数より上へ
丸ごと貼り付けます。この文書では貼り付け部分を省略し、使う部分だけを載せます。

## Timer

単位はミリ秒です。AtCoder の制限ぴったりではなく、出力用の余裕を残した値を
指定します。

```cpp
Timer timer;

while (!timer.is_over(1900.0)) {
  double progress = timer.progress(1900.0);  // 0.0 から 1.0
  double remaining = timer.remaining_ms(1900.0);
  // 探索を1回進める
}
```

## Random

同じ seed を渡すと同じ乱数列になり、バグを再現しやすくなります。

```cpp
Random random(123);

int index = random.next_int(0, n);                  // [0, n)
long long large = random.next_int(0LL, 1000000000000LL);
double probability = random.next_real();            // [0, 1)
double temperature = random.next_real(1.0, 100.0);  // [1, 100)

random.shuffle(order);
int value = random.choice(values);

vector<double> weights = {1.0, 2.0, 7.0};
int selected = random.weighted_index(weights);
```

## タイマー内蔵の焼きなまし

最大化では `変更後 - 変更前`、最小化では `変更前 - 変更後` を渡します。
どちらも「正なら良い変更」になるようにするのがポイントです。

```cpp
TimeBasedSimulatedAnnealing sa(1900.0, 100.0, 1.0, 123);

while (!sa.is_over()) {
  // 変更案を作る
  double improvement = new_score - current_score;  // 最大化の場合

  if (sa.accept(improvement)) {
    // 変更を採用する
    current_score = new_score;
  } else {
    // 変更を元に戻す
  }
}
```

温度は `100.0` から `1.0` へ指数的に下がります。得点差の大きさに合わせて
開始温度と終了温度を変えてください。

## 進捗率を外から渡す焼きなまし

探索全体で1個の `Timer` を共有したい時や、複数フェーズに分けたい時はこちらを
使います。

```cpp
Timer timer;
SimulatedAnnealing sa(100.0, 1.0, 123);

while (!timer.is_over(1200.0)) {
  long long improvement = new_score - current_score;
  if (sa.accept(improvement, timer.progress(1200.0))) {
    // 採用
  }
}
```

## BestKeeper

探索中の状態が最後に悪化していても、最良解を出力できます。

```cpp
long long current_score = calculate_score(answer);
BestKeeper<long long, vector<int>> best(current_score, answer);

// 状態を変更した後
best.update(current_score, answer);

// 最後は best.best_state を出力する
cout << best.best_score << '\n';
```

最小化なら第3引数を `false` にします。

```cpp
BestKeeper<double, vector<int>> best(initial_cost, answer, false);
```

## TopK

候補をたくさん作り、良いものだけ詳しく調べたい時に使います。

```cpp
TopK<double, vector<int>> candidates(20);  // 最大化、上位20個

for (const auto& state : many_states) {
  double cheap_score = calculate_cheap_score(state);
  candidates.add(cheap_score, state);
}

for (const auto& [cheap_score, state] : candidates.entries) {
  // この20個だけを重い評価へ回す
}
```

最小化なら `TopK<double, State> candidates(20, false);` です。

## Schedule

進捗率に応じて、近傍の大きさや試行回数を変えられます。

```cpp
double progress = timer.progress(1900.0);

int destroy_size = linear_schedule(100, 5, progress);
int trials = power_schedule(1000, 100, progress, 2.0);
double temperature = geometric_schedule(100.0, 1.0, progress);
```

## chmin / chmax

```cpp
long long best_score = -1;
if (chmax(best_score, new_score)) {
  // best_score が更新された時だけ行う処理
}

double shortest = 1e100;
chmin(shortest, new_distance);
```

## 累積和

```cpp
vector<long long> values = {3, 1, 4, 1, 5};
CumulativeSum<long long> sum(values);
cout << sum.query(1, 4) << '\n';  // values[1] + values[2] + values[3]
```

2次元版の範囲は `[top, bottom) × [left, right)` です。

```cpp
vector<vector<int>> grid(h, vector<int>(w));
CumulativeSum2D<int> sum(grid);
int rectangle_sum = sum.query(top, left, bottom, right);
```

## 座標圧縮

```cpp
vector<long long> xs = {100, 20, 100, 50};
CoordinateCompression<long long> compression(xs);

int compressed = compression.index(50);  // 1
long long original = compression.value(compressed);  // 50
vector<int> all = compression.compress(xs);  // {2, 0, 2, 1}
```

## DSU

```cpp
Dsu dsu(n);
dsu.unite(a, b);

if (dsu.same(a, b)) {
  cout << "connected\n";
}
cout << dsu.size(a) << '\n';
```

## RollbackArray

変更案を実際に配列へ反映して評価し、不採用なら元へ戻せます。

```cpp
RollbackArray<int> state(initial_state);

int snapshot = state.snapshot();
state.set(i, new_value_i);
state.set(j, new_value_j);

if (accept_move) {
  // 何もしない。変更後の状態が残る
} else {
  state.rollback(snapshot);
}
```

## RollbackDsu

辺を仮に追加して連結性を調べ、追加前へ戻せます。

```cpp
RollbackDsu dsu(n);

int snapshot = dsu.snapshot();
dsu.unite(a, b);
dsu.unite(c, d);

bool connected = dsu.same(x, y);
dsu.rollback(snapshot);
```

## StampArray

BFSごとに大きな配列全体を `fill` したくない時に使います。

```cpp
StampArray<int> distance(n, -1);

for (int start : starts) {
  distance.clear();
  queue<int> que;
  distance[start] = 0;
  que.push(start);

  while (!que.empty()) {
    int v = que.front();
    que.pop();
    // distance[to] は未訪問なら -1
  }
}
```

## BatchedTimer

1反復が非常に軽い時、毎回時計を見るコストを減らします。指定回数の間は時間切れを
確認しないので、1反復が重い処理には通常の `Timer` を使ってください。

```cpp
BatchedTimer timer(1900.0, 256);

while (!timer.is_over()) {
  // 軽い探索を1回進める
  double progress = timer.cached_progress();
}
```

## MultiStart

ランダム初期解を複数作り、一番良いものを選びます。

```cpp
auto generate = [&]() {
  State state;
  // random を使って state を作る
  return state;
};

auto evaluate = [](const State& state) {
  return state.score;
};

State best = multi_start<State>(100, generate, evaluate);  // 100回、最大化
State best_by_time =
    time_based_multi_start<State>(500.0, generate, evaluate);  // 500ms
```

最小化では最後の引数へ `false` を渡します。

## SimpleBeamSearch

最初に使うための、状態を丸ごとコピーするビームサーチです。`State` の中へ操作履歴も
入れておけば、返された状態からそのまま答えを取り出せます。

```cpp
struct State {
  int position = 0;
  long long score = 0;
  vector<int> answer;
};

auto expand = [](const State& state) {
  vector<State> next_states;
  for (int direction : {-1, 1}) {
    State next = state;
    next.position += direction;
    next.score = next.position;
    next.answer.push_back(direction);
    next_states.push_back(std::move(next));
  }
  return next_states;
};

auto evaluate = [](const State& state) { return state.score; };

State answer = simple_beam_search(State{}, 100, 200, expand, evaluate);
```

## TreeBeamSearch

状態が大きくコピーが重い時の発展版です。1手進める `apply` と、その1手だけを
正確に戻す `revert` を用意します。

```cpp
struct State {
  int position = 0;
  long long score = 0;
};

struct Move {
  int difference;
};

auto expand = [](const State&) {
  return vector<Move>{{-1}, {1}};
};

auto apply = [](State& state, const Move& move) {
  state.position += move.difference;
  state.score += move.difference;
};

auto revert = [](State& state, const Move& move) {
  state.position -= move.difference;
  state.score -= move.difference;
};

auto evaluate = [](const State& state) { return state.score; };

State initial;
TreeBeamSearch<State, Move, long long> beam(initial, initial.score, 200);

for (int turn = 0; turn < 100; ++turn) {
  if (!beam.step(expand, apply, revert, evaluate)) break;
}

vector<Move> answer = beam.restore();
```

`apply` でscore、hash、補助配列も変更したなら、`revert` ではそれらを全部戻します。

同じ盤面へ別の手順で到達する問題では、`step_with_key` を使えます。
`state.hash` が同じ候補は、評価値が一番良い1件だけ残ります。

```cpp
for (int turn = 0; turn < 100; ++turn) {
  bool advanced = beam.step_with_key(
      expand,
      apply,
      revert,
      evaluate,
      [](const State& state) { return state.hash; });
  if (!advanced) break;
}
```

`hash` は自分で更新します。盤面のswapなら `zobrist-hash.hpp` と組み合わせると
差分更新しやすいです。64bit hashにはごく小さい衝突可能性があります。

## SharedHistory

候補ごとに長い操作列をコピーせず、最後の操作と親番号だけを保存します。

```cpp
SharedHistory<Move> history;
int root = history.root();

int first = history.add(root, first_move);
int second = history.add(first, second_move);
int another = history.add(first, another_move);

vector<Move> answer = history.restore(second);
```

## BestByKey

複数の操作列が同じ状態に到達する時、同じ状態キーの中で一番良い候補だけ残します。

```cpp
BestByKey<uint64_t, long long, State> unique_candidates;

for (State state : candidates) {
  unique_candidates.add(state.hash, state.score, std::move(state));
}

for (auto& entry : unique_candidates.entries) {
  // entry.key, entry.score, entry.state を使う
}
```

最小化なら `BestByKey<uint64_t, long long, State> unique(false);` です。

## ZobristHash

配列の1か所を変更した時、配列全体を見直さずhashを更新できます。

```cpp
vector<int> values(n);  // 各値は [0, value_kinds)
ZobristHash zobrist(n, value_kinds, 123);
uint64_t hash = zobrist.build(values);

int old_value = values[position];
int new_value = 3;
zobrist.change(hash, position, old_value, new_value);
values[position] = new_value;
```

64bit hashにも衝突の可能性はあります。デバッグ中はhashが同じ候補について、可能なら
元の状態も比較してください。

## FixedVector

要素数の最大値が分かっている小さな配列で、動的メモリ確保を避けたい時に使います。

```cpp
FixedVector<int, 100> values;
values.push_back(10);
values.push_back(20);

for (int value : values) {
  cout << value << '\n';
}
```

100個を超えて追加するとassertで停止します。

## MoveStatistics

近傍の種類ごとに用意すると、どの近傍が働いているか確認できます。

```cpp
vector<MoveStatistics> statistics(number_of_move_types);

statistics[type].add(accepted, improvement > 0);

cerr << "accept = " << statistics[type].acceptance_rate() << '\n';
cerr << "improve = " << statistics[type].improvement_rate() << '\n';
```

## RouteUtils

巡回路や配送順の距離を計算します。距離関数を渡すので、点や距離の型は自由です。

```cpp
vector<pair<int, int>> route{{0, 0}, {10, 5}, {20, 5}, {0, 0}};

auto manhattan = [](auto a, auto b) {
  return abs(a.first - b.first) + abs(a.second - b.second);
};

int cost = route_length(route, manhattan);

// route[2] の直前へ (12, 8) を挿入した場合の距離変化
int insert_delta =
    route_insertion_delta(route, 2, pair{12, 8}, manhattan);

// route[1] を削除した場合の距離変化
int remove_delta = route_removal_delta(route, 1, manhattan);

// route[1]〜route[2] をreverseした場合の距離変化
int reverse_delta = route_reverse_delta(route, 1, 2, manhattan);
```

差分はすべて `変更後 - 変更前` です。`route_reverse_delta` はManhattan距離など、
行きと帰りの距離が同じ場合にだけ使えます。
