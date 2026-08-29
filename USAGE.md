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
