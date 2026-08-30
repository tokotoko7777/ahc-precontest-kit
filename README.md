# ahc-precontest-kit

AHC を中心とした競技プログラミングで、自分の `main.cpp` へ直接コピーして使う
C++ パーツ集です。ヒューリスティック探索だけでなく、グラフ・データ構造などの
通常アルゴリズムも同じ形で揃えます。

## 使い方

コンテスト中に編集するファイルは `main.cpp` だけです。

1. 下の一覧から必要なパーツを開く。
2. ファイル全体をコピーする。
3. 自分の `main.cpp` の `main` 関数より上へ貼り付ける。
4. そのまま `main.cpp` だけをコンパイル・提出する。

ローカルファイルを参照する手順、生成スクリプト、実行時ファイルは必要ありません。
各パーツは標準ライブラリの依存も含め、ファイル全体を単独で貼れる形にします。
初心者がそのまま読めるように、カスタム `namespace` や複雑な共通基盤は使わず、
原則として 1 ファイルを 1 機能だけにします。

考え方は [Luzhiled's Library](https://ei1333.github.io/library/) のような、必要な実装を
探して自分のコードへ取り込める競技プログラミング用ライブラリを参考にしています。

## まずはこの4つ

初めて使うなら、次の4ファイルだけ見れば十分です。

- [`timer.hpp`](library/timer.hpp) — 時間切れ判定と進捗率
- [`random.hpp`](library/random.hpp) — 整数・小数・shuffle・重み付き抽選
- [`time-based-simulated-annealing.hpp`](library/time-based-simulated-annealing.hpp) —
  タイマー内蔵の焼きなまし
- [`best-keeper.hpp`](library/best-keeper.hpp) — 今までで一番良い解を保存

各ファイルの先頭にも、短い使い方例を書いてあります。もう少し長い例は
[`USAGE.md`](USAGE.md) にあります。
似たアルゴリズムの使い分けは [`ALGORITHM_SELECTION.md`](ALGORITHM_SELECTION.md) の
早見表から選べます。

## 高速化の方針

探索回数へ直結するため、パーツ追加時は計算量だけでなく、全ソート、動的確保、
間接関数呼び出し、時計取得、メモリ配置も確認します。制約が厳しい高速版は、
使える条件をファイル先頭へ明記します。詳しい判断基準は
[`PERFORMANCE.md`](PERFORMANCE.md) にまとめています。

## パーツ一覧

### 時間・乱数・小道具

| ファイル | できること |
|---|---|
| [`timer.hpp`](library/timer.hpp) | 経過時間・残り時間・進捗率 |
| [`batched-timer.hpp`](library/batched-timer.hpp) | 時計を見る回数を間引くタイマー |
| [`random.hpp`](library/random.hpp) | 型を選べる乱数、ランダム選択、重み付き選択 |
| [`alias-table.hpp`](library/alias-table.hpp) | 固定重み分布から前計算後O(1)で抽選 |
| [`fast-io.hpp`](library/fast-io.hpp) | 大量の整数・文字列用のバッファ入出力 |
| [`chmin-chmax.hpp`](library/chmin-chmax.hpp) | 値が良くなる時だけ更新 |
| [`binary-search-answer.hpp`](library/binary-search-answer.hpp) | 単調な条件の整数・実数境界を二分探索 |
| [`schedule.hpp`](library/schedule.hpp) | 時間経過に合わせて値を変化 |
| [`best-keeper.hpp`](library/best-keeper.hpp) | best score と best state を保存 |
| [`top-k.hpp`](library/top-k.hpp) | 良い候補を上位 K 個だけ保存 |
| [`move-statistics.hpp`](library/move-statistics.hpp) | 近傍ごとの採用率・改善率を集計 |
| [`route-utils.hpp`](library/route-utils.hpp) | 経路長と挿入・削除・区間反転の距離差分 |

### 探索

| ファイル | できること |
|---|---|
| [`simulated-annealing.hpp`](library/simulated-annealing.hpp) | 外部から進捗率を渡す焼きなまし |
| [`time-based-simulated-annealing.hpp`](library/time-based-simulated-annealing.hpp) | タイマー内蔵の焼きなまし |
| [`multi-start.hpp`](library/multi-start.hpp) | 回数または時間指定の多点スタート |
| [`simple-beam-search.hpp`](library/simple-beam-search.hpp) | 状態をコピーする初心者向けビームサーチ |
| [`tree-beam-search.hpp`](library/tree-beam-search.hpp) | apply / revert型ビームサーチ。同じ状態キーの重複除去にも対応 |
| [`common-scenario-average.hpp`](library/common-scenario-average.hpp) | 全候補を同じ未来sampleで比較するrollout補助 |

ビームサーチを初めて使う場合は `simple-beam-search.hpp` から始めてください。
`tree-beam-search.hpp` は状態が大きく、コピーが重い場合の発展版です。

### 探索状態・候補管理

| ファイル | できること |
|---|---|
| [`shared-history.hpp`](library/shared-history.hpp) | 操作履歴の共通部分を親番号で共有・復元 |
| [`best-by-key.hpp`](library/best-by-key.hpp) | 同じ状態キーの候補を一番良いものだけにする |
| [`zobrist-hash.hpp`](library/zobrist-hash.hpp) | 配列状態のhashを差分更新 |
| [`fixed-vector.hpp`](library/fixed-vector.hpp) | allocationなしの固定上限vector |
| [`radix-heap.hpp`](library/radix-heap.hpp) | 単調な非負整数キー用の高速priority queue |
| [`farthest-point-sampling.hpp`](library/farthest-point-sampling.hpp) | 離れた代表点をO(NK)で選ぶ |
| [`greedy-balanced-partition.hpp`](library/greedy-balanced-partition.hpp) | 大きい要素から合計の軽い組へ分ける |

### データ構造

| ファイル | できること |
|---|---|
| [`cumulative-sum.hpp`](library/cumulative-sum.hpp) | 1次元累積和 |
| [`cumulative-sum-2d.hpp`](library/cumulative-sum-2d.hpp) | 2次元累積和 |
| [`difference-array.hpp`](library/difference-array.hpp) | 更新を全部先に処理する区間加算 |
| [`difference-array-2d.hpp`](library/difference-array-2d.hpp) | 矩形加算後に盤面を作る2次元いもす法 |
| [`fenwick-tree.hpp`](library/fenwick-tree.hpp) | 1点加算と区間和 |
| [`segment-tree.hpp`](library/segment-tree.hpp) | 1点変更と区間の和・最小値・最大値など |
| [`range-add-range-sum.hpp`](library/range-add-range-sum.hpp) | 区間加算と区間和 |
| [`range-add-range-minimum.hpp`](library/range-add-range-minimum.hpp) | 区間加算と区間最小値 |
| [`range-add-range-maximum.hpp`](library/range-add-range-maximum.hpp) | 区間加算と区間最大値 |
| [`range-assign-range-sum.hpp`](library/range-assign-range-sum.hpp) | 区間代入と区間和 |
| [`sparse-table.hpp`](library/sparse-table.hpp) | 静的配列の区間min・max・gcdをO(1)取得 |
| [`sliding-window-minimum.hpp`](library/sliding-window-minimum.hpp) | 固定幅区間のmin・maxを全体O(N)計算 |
| [`flat-grid.hpp`](library/flat-grid.hpp) | 連続メモリに置くキャッシュ効率重視の2次元配列 |
| [`coordinate-compression.hpp`](library/coordinate-compression.hpp) | 座標圧縮 |
| [`dsu.hpp`](library/dsu.hpp) | Union-Find。連結成分数とグループ一覧も取得可能 |
| [`weighted-dsu.hpp`](library/weighted-dsu.hpp) | 頂点間の差分制約を管理する重み付きUnion-Find |
| [`rollback-array.hpp`](library/rollback-array.hpp) | 変更を過去の状態へ戻せる配列 |
| [`rollback-dsu.hpp`](library/rollback-dsu.hpp) | 過去の状態へ戻せる Union-Find |
| [`stamp-array.hpp`](library/stamp-array.hpp) | ほぼ O(1) で初期化し直せる配列 |
| [`dense-int-set.hpp`](library/dense-int-set.hpp) | 固定範囲の整数集合。追加・削除・clearがO(1) |
| [`binary-trie.hpp`](library/binary-trie.hpp) | 非負整数集合の最小・最大XOR要素 |
| [`xor-basis.hpp`](library/xor-basis.hpp) | 部分集合XORの表現可能性・最小・最大値 |
| [`axis-aligned-rectangle.hpp`](library/axis-aligned-rectangle.hpp) | 半開矩形の面積・点包含・重なり判定 |
| [`interval-union.hpp`](library/interval-union.hpp) | 半開区間の併合・被覆長・2集合の対称差長 |

### グラフ・グリッド

| ファイル | できること |
|---|---|
| [`dijkstra.hpp`](library/dijkstra.hpp) | 非負辺グラフの最短距離と経路復元 |
| [`bellman-ford.hpp`](library/bellman-ford.hpp) | 負辺を含む最短距離・負閉路の影響範囲 |
| [`graph-bfs.hpp`](library/graph-bfs.hpp) | 重みなしグラフの最短距離と経路復元 |
| [`all-pairs-bfs.hpp`](library/all-pairs-bfs.hpp) | 重みなし全頂点間距離。距離型でメモリを調整 |
| [`zero-one-bfs.hpp`](library/zero-one-bfs.hpp) | コスト0/1の最短距離と経路復元 |
| [`grid-bfs.hpp`](library/grid-bfs.hpp) | 4方向グリッドの最短距離と経路復元 |
| [`topological-sort.hpp`](library/topological-sort.hpp) | DAGの順序と閉路検出 |
| [`bipartite-check.hpp`](library/bipartite-check.hpp) | 無向グラフの2色塗り・二部グラフ判定 |
| [`strongly-connected-components.hpp`](library/strongly-connected-components.hpp) | 有向グラフの強連結成分分解 |
| [`functional-graph.hpp`](library/functional-graph.hpp) | 出辺1本のグラフの周期・入口・大きな回数の遷移 |
| [`floyd-warshall.hpp`](library/floyd-warshall.hpp) | 全頂点間最短距離と負閉路検出 |
| [`kruskal.hpp`](library/kruskal.hpp) | 最小全域木・最小全域森 |
| [`lowest-common-ancestor.hpp`](library/lowest-common-ancestor.hpp) | 木のLCA・距離・パス上の頂点 |
| [`tree-diameter.hpp`](library/tree-diameter.hpp) | 非負重みの木の直径と経路復元 |
| [`max-flow.hpp`](library/max-flow.hpp) | Dinic法の最大流・最小カット |
| [`min-cost-flow.hpp`](library/min-cost-flow.hpp) | 非負辺コスト用の最小費用流 |
| [`bipartite-matching.hpp`](library/bipartite-matching.hpp) | 左右の頂点を1対1対応させる最大マッチング |
| [`hungarian.hpp`](library/hungarian.hpp) | 費用最小の1対1割り当て |
| [`two-sat.hpp`](library/two-sat.hpp) | 「AまたはB」の論理条件を満たす割り当て |

### 数学

| ファイル | できること |
|---|---|
| [`static-mod-int.hpp`](library/static-mod-int.hpp) | コンパイル時modの四則演算・累乗・逆元 |
| [`prime-table.hpp`](library/prime-table.hpp) | 線形篩・素数判定・素因数分解・約数列挙 |
| [`mod-combination.hpp`](library/mod-combination.hpp) | 素数mod上のnCk・nPk・重複組合せ |
| [`extended-gcd.hpp`](library/extended-gcd.hpp) | 拡張Euclid互除法・mod逆元 |
| [`floor-sum.hpp`](library/floor-sum.hpp) | floorを含む等差数列の和 |
| [`integer-square-root.hpp`](library/integer-square-root.hpp) | 整数平方根の切り下げ・切り上げ |
| [`matrix.hpp`](library/matrix.hpp) | 連続メモリ行列の加算・乗算・累乗 |

### 文字列・列

| ファイル | できること |
|---|---|
| [`rolling-hash.hpp`](library/rolling-hash.hpp) | 部分列hash・連結・LCP |
| [`sequence-overlap.hpp`](library/sequence-overlap.hpp) | 2列を末尾・先頭で最大限重ねて連結 |
| [`z-algorithm.hpp`](library/z-algorithm.hpp) | 各位置と先頭の最長共通接頭辞 |
| [`prefix-function.hpp`](library/prefix-function.hpp) | KMP用prefix function・パターン出現位置 |
| [`manacher.hpp`](library/manacher.hpp) | 全中心の最長回文を全体O(N)で計算 |

### 列・DP

| ファイル | できること |
|---|---|
| [`longest-increasing-subsequence.hpp`](library/longest-increasing-subsequence.hpp) | 最長増加部分列の長さと復元 |
| [`inversion-count.hpp`](library/inversion-count.hpp) | 大小関係が逆になった組の個数 |

### 確率DP

| ファイル | できること |
|---|---|
| [`probability-move-dp.hpp`](library/probability-move-dp.hpp) | 成功時に遷移、失敗時に停止する状態確率を1手更新 |

### 幾何

| ファイル | できること |
|---|---|
| [`point-2d.hpp`](library/point-2d.hpp) | 2次元点・内積・外積・距離・向き |
| [`convex-hull.hpp`](library/convex-hull.hpp) | 整数座標の凸包を反時計回りで列挙 |
| [`segment-intersection.hpp`](library/segment-intersection.hpp) | 整数座標の線分交差・線分上判定 |
| [`hilbert-order.hpp`](library/hilbert-order.hpp) | 近い2次元点を近くへ並べやすいHilbert順 |

`int` 固定である必要がないパーツはテンプレートにしています。たとえば、次のように
得点は `double`、解は `vector<int>` のように自由に選べます。

```cpp
BestKeeper<double, vector<int>> best(initial_score, initial_answer);
CumulativeSum<long long> sum(values);
RollbackArray<string> names(initial_names);
```

各 `.hpp` は別のライブラリファイルを参照しません。必要なファイルだけを丸ごと
コピーできます。貼り付け後はカスタム名前空間を付けず、そのまま使えます。

GitHub 上ではファイルを開き、右上のコピーアイコン、または Raw 表示から全体を
コピーしてください。

## 参考資料

- [木上のビームサーチ：高速化編](https://trap.jp/post/2920/) —
  apply / revert、履歴共有、状態コピー削減という考え方を参考にしています。
- [heuristic-library-rs](https://github.com/e1jirou/heuristic-library-rs) —
  ヒューリスティックに限定しない分類と、1機能ずつ取り出せるAPIの粒度を
  参考にしています。コードの移植ではなく、このリポジトリ向けにC++で新規実装します。
- [AC Library Documentation](https://atcoder.github.io/ac-library/production/document_ja/) —
  計算量、境界条件、型をコンパイル時に確定する汎用データ構造の設計を確認しています。
- [Introduction to Heuristics Contest A の実装記録](https://ruthen.hatenablog.com/entry/2024/01/25/000000) —
  差分計算、時計取得の間引き、理論計算量と定数倍の両方を見る実例を参考にしています。
- 手元のAHC001〜AHC068優勝コードレビュー知識から、複数問題で再利用例が
  確認できた時間管理・候補制限・重複除去・差分更新を選んでいます。

`tree-beam-search.hpp` は記事のRust実装の移植ではありません。記事の高度な
帰りがけ順管理より理解しやすい、親をたどって共通祖先まで戻る独自のC++実装です。

## 過去AHCでの実戦例

AHC001〜AHC069を順次追加しています。全問題の状態と次の作業は
[`practice/PROGRESS.md`](practice/PROGRESS.md) で管理します。

| 問題 | 主に使うパーツ |
|---|---|
| [`practice/ahc001`](practice/ahc001/) | 半開矩形、面積比による再帰領域分割 |
| [`practice/ahc002`](practice/ahc002/) | 焼きなまし、多点スタート、destroy/repair |
| [`practice/ahc003`](practice/ahc003/) | オンライン辺重み推定、不確実性付きDijkstra |
| [`practice/ahc004`](practice/ahc004/) | 列の重ね合わせ、巡回窓の差分更新 |
| [`practice/ahc005`](practice/ahc005/) | bitset監視点選択、重み付き最短路、巡回順改善 |
| [`practice/ahc006`](practice/ahc006/) | 焼きなまし、間引きタイマー、経路距離差分 |
| [`practice/ahc007`](practice/ahc007/) | Union-Find、未来辺によるオンライン連結判断 |
| [`practice/ahc008`](practice/ahc008/) | 対話型の安全判定、分担壁建設、段階閉鎖 |
| [`practice/ahc009`](practice/ahc009/) | 確率伝播DP、beam、可変長SA、前後DP |
| [`practice/ahc010`](practice/ahc010/) | 閉路DFS、bitset衝突判定、安全fallback |
| [`practice/ahc011`](practice/ahc011/) | Zobrist重複除去、親履歴付きbeam、短列探索 |
| [`practice/ahc012`](practice/ahc012/) | 分位格子、histogram評価、境界relocate |
| [`practice/ahc013`](practice/ahc013/) | 同種見通し移動、DSU、交差しない貪欲配線 |
| [`practice/ahc014`](practice/ahc014/) | 単位辺占有、候補制限、randomized multi-start |
| [`practice/ahc015`](practice/ahc015/) | 共通乱数rollout、固定長盤面、連結成分評価 |
| [`practice/ahc016`](practice/ahc016/) | 冗長グラフ符号、置換不変特徴、自己生成noise校正 |
| [`practice/ahc017`](practice/ahc017/) | farthest-point sample、日別Dijkstra cache、swap SA |
| [`practice/ahc018`](practice/ahc018/) | 硬さ推定、複数始点Dijkstra、rolling replan |
| [`practice/ahc019`](practice/ahc019/) | 24回転voxel重合、投影bitmask、残余domino化 |
| [`practice/ahc020`](practice/ahc020/) | 重み付きset cover、metric MST、非terminal葉刈り |
| [`practice/ahc021`](practice/ahc021/) | 木型ビームサーチ、Zobrist hash、状態重複除去 |
| [`practice/ahc022`](practice/ahc022/) | 2値温度符号、active measurement、Hungarian復号 |
| [`practice/ahc023`](practice/ahc023/) | 永続通路、区間min-cost flow、区間彩色 |
| [`practice/ahc024`](practice/ahc024/) | 接触辺数差分、局所連結判定、短時間multi-start |
| [`practice/ahc025`](practice/ahc025/) | 順位推定、指数分布prior、保証付き均等化 |
| [`practice/ahc026`](practice/ahc026/) | 単独退避、完走rollout、箱位置の差分更新 |
| [`practice/ahc027`](practice/ahc027/) | 全点間BFS、汚れの緊急度、複数周期の再評価 |
| [`practice/ahc028`](practice/ahc028/) | 文字列overlap、盤面位置DP、挿入位置の再評価 |
| [`practice/ahc029`](practice/ahc029/) | 全カード×案件評価、投資・購入閾値、対話fallback |
| [`practice/ahc030`](practice/ahc030/) | 油田配置仮説bitset、情報量query、確定セル掘削 |
| [`practice/ahc031`](practice/ahc031/) | 共通帯DP、guillotine配置、壁区間の対称差 |
| [`practice/ahc032`](practice/ahc032/) | 可換stamp列挙、beam、全候補座標降下 |
| [`practice/ahc033`](practice/ahc033/) | 入口退避buffer、搬出順制御、安全な単一大型crane |
| [`practice/ahc034`](practice/ahc034/) | 循環蛇行路、積載量分割、区間操作SA |
| [`practice/ahc035`](practice/ahc035/) | 交配期待値、成分極値保存、盤面swap SA |
| [`practice/ahc036`](practice/ahc036/) | 連結都市群、信号優先経路、再利用window辞書 |
| [`practice/ahc037`](practice/ahc037/) | 単調Steiner点、階層merge、局所付け替え |
| [`practice/ahc038`](practice/ahc038/) | 長さ違いの星型arm、同時回転、長さ1 fallback |
| [`practice/ahc039`](practice/ahc039/) | x単調直交多角形、多解像度境界SA、転置探索 |
| [`practice/ahc040`](practice/ahc040/) | Gaussian寸法推定、Kalman更新、bottom-left packing |
| [`practice/ahc041`](practice/ahc041/) | 深さ制限DFS、価値順構築、部分木reparent |
| [`practice/ahc042`](practice/ahc042/) | 福を守るshift、まとめ押し、盤面再生SA |
| [`practice/ahc043`](practice/ahc043/) | 駅候補制限、複数始点BFS、資金推移の正確な再生 |
| [`practice/ahc044`](practice/ahc044/) | rotor-router、流量割当、長期simulation座標降下 |
| [`practice/ahc045`](practice/ahc045/) | 容量付きk-d分割、分割MST query、都市交換 |
| [`practice/ahc046`](practice/ahc046/) | 停止岩候補、bitset重複除去、幅制限beam |
| [`practice/ahc047`](practice/ahc047/) | 12状態Markovモデル、確率近似SA、KMP厳密評価 |
| [`practice/ahc048`](practice/ahc048/) | 離散混色recipe、色cluster、残色reservoir再利用 |
| [`practice/ahc049`](practice/ahc049/) | 耐久保証付き複数箱搬送、経路交換・統合改善 |
| [`practice/ahc050`](practice/ahc050/) | 厳密確率伝播、同率最小riskのmulti-start |
| [`practice/ahc051`](practice/ahc051/) | 平面二分木、確率分類、装置・分別器の局所改善 |
| [`practice/ahc052`](practice/ahc052/) | 多始点BFS、共通ボタンlookahead、操作列短縮 |
| [`practice/ahc053`](practice/ahc053/) | 共有2進カード、禁止bit桁DP、不足修理 |
| [`practice/ahc055`](practice/ahc055/) | 依存順序、攻撃先のO(1)差分焼きなまし |
| [`practice/ahc056`](practice/ahc056/) | BFS経路、時刻を色×状態へ平方根分割 |
| [`practice/ahc057`](practice/ahc057/) | 容量付き時空間cluster、時刻別MST、群間swap |
| [`practice/ahc058`](practice/ahc058/) | rolling horizon、二項係数による将来生産量 |
| [`practice/ahc059`](practice/ahc059/) | 完全入れ子列、2状態DP、境界差分探索 |
| [`practice/ahc062`](practice/ahc062/) | Hamilton閉路、prefix差分2-opt、合法swap |

各フォルダの `main.cpp` はローカルヘッダを参照しない、提出可能な単一ファイルです。

## 最小テンプレート

[`template/main.cpp`](template/main.cpp) は、パーツを貼る位置だけを示す最小構成です。
テンプレートにもローカルファイルへの依存はありません。

```cpp
#include <bits/stdc++.h>
using namespace std;

// 必要な library/*.hpp の中身をここへ貼る。

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  // 問題固有の処理を書く。
}
```

## ライブラリ追加時の約束

- ファイル全体を `main.cpp` の上部へ貼るだけで使えること。
- カスタム `namespace` や難しい共通基盤を使わないこと。
- ローカルファイルへの `#include "..."` を含めないこと。
- 外部パッケージや実行時ファイルに依存しないこと。
- 問題固有の `main` 関数や入出力を含めないこと。
- 使用する標準ヘッダをパーツ自身に記載すること。
- 最小のテストを追加すること。

## リポジトリ側の検証

パーツ集を更新するときだけ、次を実行します。コンテスト中の利用には不要です。

```bash
make verify
```

`make verify` はパーツのテストに加え、`practice/ahc*/main.cpp` をすべて
C++17で構文確認します。実戦例を増やしても、単一ファイル提出形式が壊れていないかを
まとめて確認できます。
