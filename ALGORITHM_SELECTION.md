# アルゴリズム早見表

同じ目的のパーツが複数ある場合は、入力の条件が一番狭く、その条件を確実に
満たしているものを選ぶと高速です。

## 最短距離

| 辺の条件 | 始点 | パーツ | 計算量 |
|---|---:|---|---:|
| 全辺のコストが同じ | 1個 | `graph-bfs.hpp` | `O(N + M)` |
| コストが0か1 | 1個 | `zero-one-bfs.hpp` | `O(N + M)` |
| コストが0以上 | 1個 | `dijkstra.hpp` | `O((N + M) log N)` |
| 負辺を含む | 1個 | `bellman-ford.hpp` | `O(NM)` |
| 負辺を含んでもよい、小さなグラフ | 全頂点間 | `floyd-warshall.hpp` | `O(N^3)` |
| 4方向の文字盤面 | 1マス | `grid-bfs.hpp` | `O(HW)` |

負辺がある時にDijkstraを使ってはいけません。Bellman–Fordは負閉路から
到達できる頂点も判定しますが、辺数と頂点数が多い場合は間に合わないことがあります。

## 連結性と順序

| 知りたいこと | パーツ | 計算量 |
|---|---|---:|
| 無向辺を追加しながら同じ成分か判定 | `dsu.hpp` | ほぼ `O(1)` / 回 |
| 頂点間の差分制約を追加・質問 | `weighted-dsu.hpp` | ほぼ `O(1)` / 回 |
| 無向グラフを2色に塗れるか | `bipartite-check.hpp` | `O(N + M)` |
| 無向グラフを最小コストで連結 | `kruskal.hpp` | `O(M log M)` |
| 無向グラフの切れやすい辺・橋で分かれる領域 | `bridge-tree.hpp` | `O(N + M)` |
| 有向グラフで互いに行き来できる集合 | `strongly-connected-components.hpp` | `O(N + M)` |
| 有向グラフを依存関係順に並べる | `topological-sort.hpp` | `O(N + M)` |
| 各頂点の出辺が1本で、周期と大きな回数後の頂点を知る | `functional-graph.hpp` | 構築 `O(64N)`、質問 `O(64)` |

## 区間の値

| 更新 | 問い合わせ | パーツ | 計算量 |
|---|---|---|---:|
| なし | 区間和 | `cumulative-sum.hpp` | 構築 `O(N)`、質問 `O(1)` |
| 1点加算 | 区間和 | `fenwick-tree.hpp` | `O(log N)` |
| 1点代入 | 和・min・maxなど | `segment-tree.hpp` | `O(log N)` |
| 区間加算 | 区間和 | `range-add-range-sum.hpp` | `O(log N)` |
| 区間加算 | 区間最小値 | `range-add-range-minimum.hpp` | `O(log N)` |
| 区間加算 | 区間最大値 | `range-add-range-maximum.hpp` | `O(log N)` |
| 区間代入 | 区間和 | `range-assign-range-sum.hpp` | `O(log N)` |
| 更新を全部先に処理 | 各点の値 | `difference-array.hpp` | 更新 `O(1)`、構築 `O(N)` |
| なし | min・max・gcd | `sparse-table.hpp` | 構築 `O(N log N)`、質問 `O(1)` |
| なし、幅が固定 | 全区間のmin・max | `sliding-window-minimum.hpp` | 全体 `O(N)` |

表にない更新・質問の組み合わせは、別のLazy Segment Tree部品が必要です。

2次元盤面に対し、矩形加算をすべて先に行ってから各マスを使う場合は
`difference-array-2d.hpp` を使います。更新1回 `O(1)`、最終構築 `O(HW)` です。

## 木

| 知りたいこと | パーツ | 計算量 |
|---|---|---:|
| 2頂点の共通祖先・距離・パス上の頂点 | `lowest-common-ancestor.hpp` | 構築 `O(N log N)`、質問 `O(log N)` |
| 最も離れた2頂点とその経路 | `tree-diameter.hpp` | `O(N)` |
| 辺を追加しながら連結性を管理 | `dsu.hpp` | ほぼ `O(1)` / 回 |

## 文字列・列の照合

| 知りたいこと | パーツ | 計算量 |
|---|---|---:|
| 各位置と列の先頭が一致する長さ | `z-algorithm.hpp` | `O(N)` |
| パターンが出現する全位置 | `prefix-function.hpp` | `O(N + M)` |
| あらゆる中心の最長回文 | `manacher.hpp` | `O(N)` |
| 部分列比較とLCPを何度も行う | `rolling-hash.hpp` | 構築 `O(N)`、比較 `O(1)` |
| 2つの短い列を末尾・先頭で最大限重ねる | `sequence-overlap.hpp` | `O(min(N,M)^2)` |

## 整数集合

| 条件 | パーツ | 計算量 |
|---|---|---:|
| 値が0以上の固定範囲で、集合の順序は不要 | `dense-int-set.hpp` | 追加・削除・検索・clear `O(1)` |
| 値とのXORが最小・最大の要素を探す | `binary-trie.hpp` | `O(bit数)` |
| 追加値の部分集合XORを組み合わせる | `xor-basis.hpp` | 追加・質問 `O(bit数)` |

`dense-int-set.hpp` は予め最大値までの配列を確保する代わりに、ハッシュ計算や
動的メモリ確保を避けます。要素の列挙順は削除で変わります。

## 重み付きランダム選択

| 重みの変化 | パーツ | 1回の抽選 |
|---|---|---:|
| 毎回変わる、または抽選回数が少ない | `random.hpp` の `weighted_index` | `O(N)` |
| 同じ重みから何度も抽選 | `alias-table.hpp` | 前計算 `O(N)`、抽選 `O(1)` |

Alias Tableの作成自体に `O(N)` かかるため、重みを毎回作り直す場合は
`weighted_index` の方が単純です。

## ランダムな移動の評価

| 状況 | パーツ | 1手の計算量 |
|---|---|---:|
| 成功なら決めた遷移先、失敗なら現在地に残る | `probability-move-dp.hpp` | `O(状態数)` |
| 分岐が多数、複数主体が相関する、状態が指数的に増える | 問題ごとのDP・Monte Carlo | 問題依存 |

`probability-move-dp.hpp` は乱数を振らず、全状態の確率をそのまま足すため、同じ行動列の
評価は毎回同じです。到達済み確率を二重に数えたくない場合は吸収状態を指定します。

未来を厳密列挙できずsampleで比べる場合は `common-scenario-average.hpp` を使えます。
全候補へ同じ未来sampleを渡し、候補ごとに別の偶然を引くことによる比較のぶれを減らします。

## 流量・対応付け・論理条件

| 問題の形 | パーツ | 主な計算量 |
|---|---|---:|
| 有向辺の容量を守って最大量を運ぶ | `max-flow.hpp` | Dinic法 `O(N^2 M)` |
| 決めた量を最小コストで運ぶ | `min-cost-flow.hpp` | `O(F M log N)` |
| 左集合と右集合から1対1の組を最大数選ぶ | `bipartite-matching.hpp` | `O(M sqrt(N))` |
| 左右の順番を保ち、交差しない組の重みを最大化 | `non-crossing-matching.hpp` | `O(LR)` |
| true/falseの「AまたはB」条件をすべて満たす | `two-sat.hpp` | `O(N + M)` |

二部グラフの単純な最大マッチングは、最大流でも解けますが
`bipartite-matching.hpp` の方がグラフを作りやすく高速です。
`min-cost-flow.hpp` に追加できるのはコスト0以上の辺だけで、同じインスタンスの
`flow` は1回だけ呼びます。

## AHC探索コア

| 状況 | パーツ | 必要な条件 |
|---|---|---|
| 1つの解の局所変更を繰り返す | `time-based-simulated-annealing.hpp` | 得点差を正しく計算できる |
| 手数ごとに複数候補を残し、状態が小さい | `simple-beam-search.hpp` | 子の`State`コピーが十分軽い |
| 全行動で1世代ずつ進み、状態が大きい | `tree-beam-search.hpp` | `apply / revert`が完全に逆操作 |
| 行動ごとに到着世代が異なる | `cost-tree-beam-search.hpp` | `apply / revert`に加え、`advance > 0` |
| Kが小さく、候補を1件ずつ追加 | `top-k.hpp` | 局所的な上位K件だけ必要 |
| ビーム以外で同keyの最良候補だけ保存 | `best-by-key.hpp` | keyが将来に必要な状態を区別できる |

木上版は状態コピーを避けられますが、`apply / revert`の実装ミスは探索全体を
壊します。まず`SimpleBeamSearch`で形を作り、コピーが実際に重い時に移行すると
安全です。

`evaluate`はビーム内の順位用であり、問題本来の最終得点とは分けます。
早くterminalに到達する状態は`step_and_observe`で生成直後に別保存します。
同一世代の重複が多い時だけ`step_with_key`を使い、keyには残り資源など
将来に必要な情報も含めます。最小例は
[`SEARCH_GUIDE.md`](SEARCH_GUIDE.md) にあります。
