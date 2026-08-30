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

## 答えの二分探索

「ある値以上なら可能」のように、判定結果が境界の前後で一度だけ
変わる場合に使います。

```cpp
// possible(ng) == false、possible(ok) == true にする
long long answer = binary_search_first_true<long long>(
    ng, ok, [&](long long value) { return possible(value); });

// true...true,false...false の最後のtrue
long long last = binary_search_last_true<long long>(
    ok_value, ng_value, [&](long long value) { return possible(value); });
```

実数は誤差値ではなく、回数を決めて絞ると実行時間が安定します。

```cpp
auto [false_side, true_side] = binary_search_real<double>(
    low, high, 100, [&](double value) { return possible(value); });
```

条件が途中でtrueからfalseへ戻る場合には使えません。

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

## Fenwick Tree

値を途中で足し引きしながら区間和を何度も求める時に使います。累積和と違い、
`add` の後も高速に区間和を求められます。

```cpp
vector<long long> values = {3, 1, 4, 1, 5};
FenwickTree<long long> sum(values);

sum.add(2, 10);                 // values[2] に 10 を足す
cout << sum.query(1, 4) << '\n';  // values[1] + values[2] + values[3]
cout << sum.prefix_sum(3) << '\n';  // values[0] + values[1] + values[2]
```

各値が0以上なら、先頭からの和が指定値に達する位置も探せます。

```cpp
int index = sum.lower_bound(8LL);
// prefix_sum(index + 1) が初めて 8 以上になる index
// 全体の和が 8 未満なら sum.size() が返る
```

## Segment Tree

区間和だけでなく、区間の最小値・最大値などを求めたい時に使います。
第2引数は空の区間を表す値、第3引数は2区間をまとめる処理です。

```cpp
vector<int> values = {3, 1, 4, 1, 5};
auto minimum = make_segment_tree(
    values,
    1000000000,
    [](int a, int b) { return min(a, b); });

cout << minimum.query(1, 4) << '\n';  // min({1, 4, 1}) = 1
minimum.set(1, 8);                    // values[1] = 8
cout << minimum.query(1, 3) << '\n';  // min({8, 4}) = 4
cout << minimum.all() << '\n';        // 全要素の最小値
```

区間和なら、空の区間は `0`、まとめる処理は足し算にします。

```cpp
auto sum = make_segment_tree(
    vector<long long>(n), 0LL,
    [](long long a, long long b) { return a + b; });
```

ラムダ式の型をそのまま保持するため、区間をまとめるたびに `std::function` の
間接呼び出しを行いません。通常は `make_segment_tree` から作ってください。

## DifferenceArray

区間加算をすべて記録してから、最後に各位置の値を一度だけ作る場合に使います。

```cpp
DifferenceArray<long long> values(n);
values.add(left, right, amount);  // [left, right) に加算
values.add(another_left, another_right, another_amount);

vector<long long> result = values.build();
```

途中で区間和を質問する必要がある場合は、Fenwick TreeやRangeAddRangeSumを使います。

## RangeAddRangeSum

区間全体への加算と、区間和の質問が混ざっている場合に使います。

```cpp
RangeAddRangeSum<long long> sum(initial_values);

sum.add(left, right, 10);       // [left, right) の全要素に10を足す
long long answer = sum.query(query_left, query_right);
long long total = sum.all();
```

## RangeAddRangeMinimum

区間全体に値を足しながら、区間最小値を取得する遅延セグメント木です。

```cpp
const long long INF = (1LL << 60);
RangeAddRangeMinimum<long long> minimum(values, INF);

minimum.add(left, right, amount);       // [left, right) に加算
long long answer = minimum.query(ql, qr);  // [ql, qr) の最小値
long long one_value = minimum.get(index);
long long all = minimum.all_minimum();
```

空区間の `query` は構築時に渡した `INF` を返します。`INF + amount` が
オーバーフローしない値を選びます。

## SparseTable

変更されない配列の区間最小値・最大値・gcdを何度も求める場合に使います。

```cpp
auto minimum = make_sparse_table(
    values, [](int a, int b) { return min(a, b); });

int answer = minimum.query(left, right);  // [left, right)、空区間不可
```

`min(x,x)=x` のように同じ値を重ねても変わらない演算専用です。区間和には
使えません。

## Sliding Window Minimum

固定幅の全区間について、最小値・最大値をまとめて `O(N)` で求めます。

```cpp
vector<int> values = {4, 2, 2, 5, 1};
vector<int> minimums = sliding_window_minimum(values, 3);  // {2, 2, 1}
vector<int> maximums = sliding_window_maximum(values, 3);  // {4, 5, 5}

vector<int> positions =
    sliding_window_best_indices(values, 3, less<int>());  // 最小値の位置
```

## FlatGrid

`vector<vector<T>>` と同じ感覚で使える、1本の連続した `vector` です。
大きな盤面を行方向へ何度も全走査する時に向きます。

```cpp
FlatGrid<int> distance(height, width, -1);
distance(start_row, start_column) = 0;

for (int row = 0; row < height; ++row) {
  for (int column = 0; column < width; ++column) {
    int value = distance(row, column);
  }
}

distance.fill(-1);  // 全マスを再初期化
```

マスを整数1個で持ちたい場合は `index(row, column)`、元へ戻す場合は
`position(index)` を使えます。

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
cout << dsu.component_count() << '\n';

for (const vector<int>& group : dsu.groups()) {
  // 同じ連結成分の頂点が group に入る
}
```

## Graph BFS

すべての辺を1回の移動として扱う、重みなしグラフの最短距離です。

```cpp
vector<vector<int>> graph(n);
graph[a].push_back(b);
graph[b].push_back(a);  // 無向辺なら逆向きも追加

auto shortest = graph_bfs(graph, start);
cout << shortest.distance[goal] << '\n';
vector<int> path = shortest.path_to(goal);
```

到達不能な頂点の距離は `-1`、経路は空になります。

## Dijkstra

辺のコストが0以上の重み付きグラフで、始点からの最短距離を求めます。
無向辺には `add_undirected_edge`、有向辺には `add_directed_edge` を使います。

```cpp
vector<vector<pair<int, long long>>> graph(n);
add_undirected_edge(graph, 0, 1, 5LL);
add_undirected_edge(graph, 1, 2, 3LL);
add_directed_edge(graph, 0, 2, 20LL);

const long long INF = (1LL << 60);
auto shortest = dijkstra(graph, 0, INF);

cout << shortest.distance[2] << '\n';  // 8
vector<int> path = shortest.path_to(2);  // {0, 1, 2}

if (!shortest.reachable(3)) {
  // 始点0から頂点3へは到達できない
}
```

`INF + 辺コスト` が型の上限を超えないよう、`INF` には少し余裕を持たせます。
負の辺があるグラフには使えません。

## Bellman–Ford

負の辺を含む有向グラフの最短路と、始点から辿り着ける負閉路の
影響を求めます。

```cpp
vector<BellmanFordEdge<long long>> edges;
edges.push_back({from, to, cost});

const long long INF = (1LL << 60);
auto shortest = bellman_ford(n, edges, start, INF);

if (shortest.shortest_path_exists(goal)) {
  cout << shortest.distance[goal] << '\n';
  vector<int> path = shortest.path_to(goal);
} else if (shortest.affected_by_negative_cycle[goal]) {
  // 負閉路を何度も回って距離を下げられる
}
```

計算量は `O(NM)` です。辺コストがすべて0以上なら、Dijkstraを使う方が高速です。

## 0-1 BFS

辺のコストが0か1だけなら、Dijkstraより単純なdequeで最短距離を求められます。

```cpp
vector<vector<pair<int, int>>> graph(n);
graph[from].push_back({to, 0});
graph[from].push_back({another, 1});

auto shortest = zero_one_bfs(graph, start);
cout << shortest.distance[goal] << '\n';
vector<int> path = shortest.path_to(goal);
```

コスト2以上や負のコストを入れてはいけません。

## RadixHeap

最後に取り出したキー以上のキーだけを追加できる、高速な優先度付きキューです。
Dijkstraの非負整数距離など、条件が合う場面で `priority_queue` の代わりに使えます。

```cpp
RadixHeap<int> queue;
queue.push(0, start);

while (!queue.empty()) {
  auto [distance, vertex] = queue.pop();
  // 新しくpushするキーは distance 以上にする
}
```

キーは `uint64_t` です。負数、小数、取り出したキーより小さいキーを後から
追加する用途には使えません。その場合は通常の `priority_queue` を使います。

## Grid BFS

文字グリッドなら `'#'` を壁として、上下左右の最短距離を求められます。

```cpp
vector<string> grid = {
    "...#",
    ".#..",
    "....",
};

auto shortest = grid_bfs(grid, {0, 0});
cout << shortest.distance[2][3] << '\n';

vector<pair<int, int>> path = shortest.path_to({2, 3});
for (auto [row, column] : path) {
  grid[row][column] = 'o';
}
```

通れる条件が複雑なら、判定関数を直接渡します。

```cpp
auto shortest = grid_bfs(height, width, start, [&](int row, int column) {
  return height_map[row][column] <= limit;
});
```

## Topological Sort

仕事の依存関係などを、必ず前提が先に来る順番へ並べます。

```cpp
vector<vector<int>> graph(n);
graph[before].push_back(after);

auto result = topological_sort(graph);
if (result.has_cycle) {
  // 循環する依存関係がある
} else {
  for (int vertex : result.order) {
    // この順番なら、すべての辺は前から後ろへ向く
  }
}
```

## Strongly Connected Components

有向グラフを、互いに行き来できる頂点のグループへ分けます。再帰を使わないので、
長い一本道でも再帰の深さを気にせず使えます。

```cpp
auto components = strongly_connected_components(graph);

cout << components.component_count() << '\n';
if (components.same(a, b)) {
  // aからbへも、bからaへも到達できる
}

int id = components.component_id[vertex];
for (int member : components.groups[id]) {
  // vertex と同じ強連結成分
}
```

`groups` は成分間の辺が前から後ろへ向く順番です。

## Floyd–Warshall

小さなグラフの全頂点間最短距離をまとめて求めます。`O(N^3)` なので、頂点数が
大きい場合はBFSやDijkstraを必要な始点からだけ実行します。

```cpp
const long long INF = (1LL << 60);
vector<vector<long long>> distance(n, vector<long long>(n, INF));
for (int i = 0; i < n; ++i) distance[i][i] = 0;

distance[from][to] = cost;
floyd_warshall(distance, INF);

cout << distance[start][goal] << '\n';
if (has_negative_cycle(distance)) {
  // 負閉路が存在する
}
```

`INF + 有限距離` がオーバーフローしないよう、`INF` は型の最大値より余裕を
持たせてください。

## Kruskal

無向グラフを最小コストで連結する辺を選びます。

```cpp
vector<KruskalEdge<long long>> edges;
edges.push_back({a, b, cost});

auto tree = kruskal(n, edges);
if (tree.connected()) {
  cout << tree.total_cost << '\n';
  for (auto edge : tree.edges) {
    // edge.from, edge.to, edge.cost
  }
}
```

元のグラフが連結でなければ、各成分の最小全域木を合わせた森を返します。

## Lowest Common Ancestor

木を根付き木として前計算し、LCA・頂点間距離・パス上の頂点を求めます。

```cpp
LowestCommonAncestor tree_lca(tree, root);

int common = tree_lca.lca(a, b);
int edge_count = tree_lca.distance(a, b);
int parent = tree_lca.kth_ancestor(vertex, steps);
int on_path = tree_lca.jump(a, b, steps_from_a);
```

木は連結な無向隣接リストで渡します。再帰を使わないため、一本道でも
再帰スタックを消費しません。

## Tree Diameter

木の中で最も距離が長い2頂点と、その間の経路を `O(N)` で求めます。
辺の重みは0以上です。

```cpp
vector<vector<pair<int, long long>>> tree(n);
tree[a].push_back({b, cost});
tree[b].push_back({a, cost});

auto diameter = tree_diameter(tree);
cout << diameter.length << '\n';
for (int vertex : diameter.path) {
  // endpoint_a から endpoint_b までの頂点列
}
```

重みなしの木では、各辺の `cost` を1にします。再帰は使いません。

## Max Flow

辺ごとの容量を超えないように、始点から終点まで運べる最大量を
Dinic法で求めます。容量が大きい時は `long long` を使います。

```cpp
MaxFlow<long long> flow(n);
flow.add_edge(0, 1, 10);
flow.add_edge(0, 2, 5);
flow.add_edge(1, 3, 7);
flow.add_edge(2, 3, 8);

long long maximum = flow.flow(0, 3);
```

最大流を流した後、`min_cut(source)` が `true` の頂点と `false` の頂点の
間に最小カットがあります。`get_edge(index)` で元の容量と実際の流量も読めます。

## Min-Cost Flow

流す量だけでなく、合計コストも最小にしたい場合に使います。
このパーツで `add_edge` に渡せる辺コストは0以上です。

```cpp
MinCostFlow<int, long long> flow(n);
flow.add_edge(0, 1, 2, 5);  // 容量2、1単位あたりコスト5
flow.add_edge(1, 3, 2, 4);
flow.add_edge(0, 2, 1, 1);
flow.add_edge(2, 3, 1, 2);

auto [sent, minimum_cost] = flow.flow(0, 3, 3);
// 3単位すべて流せなければ sent < 3
```

`Cost` には逆向き残余辺の負コストを保存するため、`long long` など符号付き型を
使います。`flow` は1つのインスタンスに対し1回だけ呼びます。

## Bipartite Matching

人と仕事のような左右の集合から、可能な組を1対1で最大数選びます。

```cpp
BipartiteMatching matching(person_count, job_count);
matching.add_edge(person, job);  // この組は可能

auto result = matching.solve();
cout << result.size() << '\n';
for (auto [person, job] : result.pairs()) {
  // 選ばれた組
}
```

コストのない最大マッチングなら、最大流に変換するよりこちらが簡単です。

## Two-SAT

各変数がtrue/falseのどちらかで、すべての条件を「AまたはB」で書ける時に
使います。変数番号は0からです。

```cpp
TwoSat sat(variable_count);
sat.add_clause(0, true, 1, false);  // x0 == true OR x1 == false
sat.add_implication(2, true, 0, true);  // x2 == true なら x0 == true
sat.set_value(3, false);                // x3をfalseに固定

if (sat.solve()) {
  const vector<bool>& answer = sat.answer();
} else {
  // すべてを満たす割り当てはない
}
```

## StaticModInt

modをコンパイル時に固定し、剰余の四則演算を普通の数のように書けます。

```cpp
using Mint = StaticModInt<998244353>;

Mint a = 10;
Mint b = 3;
Mint answer = (a + b) * b;
answer /= 2;

cout << answer.value() << '\n';
cout << Mint(2).pow(100).value() << '\n';
```

割り算では、割る数とmodが互いに素である必要があります。値がすでに
`[0, mod)` と保証できる熱いループでは `Mint::raw(value)` を使うと剰余計算を
省けますが、範囲が不明なら通常のコンストラクタを使ってください。

## PrimeTable

上限までの素数判定と最小素因数を線形時間で前計算します。

```cpp
PrimeTable primes(1000000);

if (primes.is_prime(x)) {
  // xは素数
}

vector<pair<int, int>> factors = primes.factorize(x);
vector<int> divisors = primes.divisors(x);
```

質問する値は構築時の上限以下である必要があります。1の素因数分解は空です。

## ModCombination

素数mod上の `nCk`、`nPk`、重複組合せを事前計算後 `O(1)` で取得します。
このファイル単体で使えるため、`static-mod-int.hpp` は必要ありません。

```cpp
ModCombination<998244353> combination(max_n);

int choose = combination.choose(n, k);          // nCk
int arrange = combination.permutation(n, k);    // nPk
int repeated = combination.multichoose(n, k);   // 重複を許してk個
```

`MOD` は素数、扱う `n` は `MOD` 未満にします。後からより大きい `n` を渡すと、
必要なところまで自動的に事前計算を延長します。

## Rolling Hash

文字列や整数列の部分列が同じかを高速に比べます。

```cpp
RollingHash hash(text);

if (hash.hash(first_left, first_right) ==
    hash.hash(second_left, second_right)) {
  // 2つの部分文字列は高確率で同じ
}

RollingHash another(other_text);
int common = hash.longest_common_prefix(
    0, hash.size(), another, 0, another.size());
```

64bit hashなので衝突可能性は0ではありません。誤判定が絶対に許されない最終確認では、
元の文字列も比較してください。異なる `base` で作った2個を比較してはいけません。

## Z Algorithm

列の各位置について、「その位置からの列」と列全体の先頭が何要素一致するかを
`O(N)` で求めます。

```cpp
vector<int> z = z_algorithm(text);
// z[i] = text[0...] と text[i...] の最長共通接頭辞
```

`string` だけでなく、`vector<int>` など `==` で比較できる列にも使えます。

## Prefix Function / KMP

パターンの出現位置をすべて、文字列の長さに比例する時間で探します。

```cpp
vector<int> positions = find_pattern_occurrences(text, pattern);

vector<int> prefix = prefix_function(pattern);
// prefix[i] = pattern[0..i]の、接頭辞でも接尾辞でもある最長長さ
```

こちらはhash衝突がなく、完全に正確な一致検索です。`string` と `vector<int>` などに
使えます。空のpatternは0から `text.size()` までの全位置に一致します。

## Manacher

列の各中心に対する奇数長・偶数長の最長回文を、全体 `O(N)` で求めます。

```cpp
auto palindromes = palindrome_radii(text);

bool yes = palindromes.is_palindrome(left, right);  // [left, right)
auto [longest_left, longest_right] = palindromes.longest_interval();

int odd_length = 2 * palindromes.odd[center] - 1;
int even_length = 2 * palindromes.even[center];
```

`string` だけでなく、`vector<int>` など `==` で比較できる列に使えます。

## Longest Increasing Subsequence

最長増加部分列の長さだけでなく、選んだ位置と値も復元します。

```cpp
auto strict = longest_increasing_subsequence(values);        // a < b
auto non_strict = longest_increasing_subsequence(values, false);  // a <= b

cout << strict.length() << '\n';
for (int index : strict.indices) cout << index << ' ';
for (auto value : strict.values) cout << value << ' ';
```

## Fast I/O

入力が非常に大きく、通常の `cin` がボトルネックになった場合だけ使います。

```cpp
FastInput input;
FastOutput output;

int n = input.next<int>();
long long x = input.next<long long>();
string name = input.next<string>();

output.write_integer(n, ' ');
output.write_integer(x, '\n');
output.write_string(name);
output.write_character('\n');
```

整数、空白区切り文字列、空白以外の1文字に対応します。小数や1行全体の入力には
通常の入出力を使ってください。`FastOutput` は終了時に残りをまとめて出力します。

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

## DenseIntSet

値の範囲が `0 <= value < universe_size` と分かっている整数集合です。
`unordered_set` と違い、hash計算や実行中のメモリ確保がありません。

```cpp
DenseIntSet active(vertex_count);
active.insert(vertex);
active.erase(vertex);

if (active.contains(vertex)) {
  // 集合に入っている
}

for (int i = 0; i < active.size(); ++i) {
  int vertex = active[i];
}

active.clear();  // O(1)
```

削除で要素の順番は変わります。乱数で `[0, active.size())` のindexを選べば、
要素を `O(1)` でランダム選択できます。

## BinaryTrie

非負整数の多重集合から、指定値とのXORが最小・最大になる要素を探します。

```cpp
BinaryTrie<unsigned> values;
values.insert(x);
values.insert(x);  // 重複も保存できる
values.erase(x);   // 1個だけ削除

unsigned nearest = values.minimum_xor_element(query);
unsigned farthest = values.maximum_xor_element(query);
unsigned minimum_xor = values.minimum_xor_value(query);
```

値が24bit未満と分かっている時は `BinaryTrie<unsigned, 24>` のようにbit数を
狭めると、時間とメモリを削減できます。空の時にXOR検索は呼べません。

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

## AxisAlignedRectangle

軸に平行な長方形です。座標は半開区間 `[left, right) × [bottom, top)` で扱います。

```cpp
AxisAlignedRectangle<int> first{0, 0, 10, 20};
AxisAlignedRectangle<int> second{10, 5, 30, 15};

cout << first.width() << ' ' << first.height() << '\n';
cout << first.area() << '\n';

bool contains = first.contains(3, 7);
bool overlap = first.overlaps(second);  // 辺が接するだけなので false
```

`contains` も半開区間の判定です。整数格子のセルや、AHC001の `(x+0.5, y+0.5)` を
含む矩形の判定にそのまま使えます。

## Point2D

整数2次元座標の足し引き、内積、外積、距離をまとめて使えます。

```cpp
Point2D<long long> a{x1, y1};
Point2D<long long> b{x2, y2};

auto difference = b - a;
long long distance2 = squared_distance(a, b);
long long manhattan = manhattan_distance(a, b);
long long cross_value = cross(origin, a, b);
int turn = orientation(origin, a, b);  // 左1、一直線0、右-1
```

乗算の結果も座標型になるため、座標が大きい時は `long long` を使います。

## Convex Hull

`x`, `y` メンバを持つ整数座標の点から凸包を求めます。`point-2d.hpp` の
`Point2D<long long>` のほか、同じメンバを持つ自作structでも使えます。

```cpp
vector<Point2D<long long>> points;
vector<Point2D<long long>> hull = convex_hull(points);
// 辞書順最小の点から反時計回り。先頭点の重複はない
```

辺の途中にある一直線上の点も残したい場合は `convex_hull(points, true)` です。
`convex-hull.hpp` 自体は `point-2d.hpp` をincludeしないため、自作の点型と組み合わせても
単独で貼り付けられます。

## Segment Intersection

閉線分同士が交わるかを、整数の外積だけで正確に判定します。

```cpp
bool touch_or_cross = segments_intersect(a, b, c, d);
bool cross_inside = segments_properly_intersect(a, b, c, d);
bool on_segment = point_on_segment(a, b, point);
```

`segments_intersect` は端点の接触や線分の重なりもtrueです。
`segments_properly_intersect` は両線分の内部で交差する場合だけtrueです。
