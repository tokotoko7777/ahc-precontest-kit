# MarathonLibraryから取り入れる設計

調査日: 2026-09-15。
[asi1024/MarathonLibrary](https://github.com/asi1024/MarathonLibrary) の
[`13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9`](https://github.com/asi1024/MarathonLibrary/tree/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9)
を固定して、main、探索、container、grid、最短路、profiler、Union-Findを確認しました。
以下は**コードから確認した設計**と、このkitへ適用する際の判断です。
作者の実績だけを根拠に、そのまま速い・高得点になるとは判断しません。

## 良い点とkitの対応

| 観点 | 参考元で確認した点 | kitでの対応 |
|---|---|---|
| 焼きなまし | logの前計算、時計と温度更新の間引き | 時刻・温度のcacheは既存。今回は**採否を近似しない対数区間表**を任意追加 |
| 処理別計測 | スコープを抜ける時に回数・時間を集計し、提出時は無効化 | **`scope-profiler.hpp`を追加**。CPU周波数に依存せず、AHC001で使用例を配置 |
| ビームの候補選抜 | 高価なAction/hash作成前に採用境界を見る。最良・最悪の両方を更新可能なqueue | 採用候補だけStateを作る設計・閾値評価は既存。**生成前の境界を渡すAPI**は次の候補 |
| 差分ビーム | State 1個を往復し、生存経路だけの探索順を配列で保持。共通prefixを確定 | apply/revert・不要枝削除は既存。**共通prefix圧縮**は優先して検討する |
| 層別探索 | 各層の候補queueから少数ずつ取り出し、時間まで複数回巡回する | 世代飛ばしとは別の戦略。既存Runnerの置換ではなく、必要なら独立した層別再訪テンプレートにする |
| 反復初期化 | 世代番号によるO(1)相当のreset | `StampArray`が既存。別名で同じパーツは増やさない |
| 短いqueue | 固定容量のリングbuffer | heap確保を減らせる。採用するなら容量超過を黙って破壊しないAPIにする |
| 最短路 | 最大辺重みが小さい時のDial queue、辺重みの種類が少ない時の複数queue | `Dijkstra`・`RadixHeap`に加える候補。整数・非負・単調性などの条件を明記して実問題で比較する |
| 盤面 | 外周の番兵、移動可能方向のbit mask、bitboardの一括隣接展開 | `FlatGrid`は既存だが番兵・bitboard探索は別機能。到達可能性を何度も計算する問題で検討する |
| rollback | Union-Findの変更履歴を戻す | `RollbackDSU`が既存。既存の戻し方の契約を保つ |

主な一次資料:
[SA](https://github.com/asi1024/MarathonLibrary/blob/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9/snippets/simulated_annealing.h)、
[ビーム](https://github.com/asi1024/MarathonLibrary/blob/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9/snippets/beam_search.h)、
[container](https://github.com/asi1024/MarathonLibrary/blob/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9/snippets/containers.h)、
[最短路](https://github.com/asi1024/MarathonLibrary/blob/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9/snippets/shortest_path.h)、
[grid](https://github.com/asi1024/MarathonLibrary/blob/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9/snippets/grid.h)、
[profiler](https://github.com/asi1024/MarathonLibrary/blob/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9/snippets/profiler.h)、
[rollback](https://github.com/asi1024/MarathonLibrary/blob/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9/snippets/unionfind.h)。

## そのままは取り入れない点

- [mainのtimer](https://github.com/asi1024/MarathonLibrary/blob/13fe8241ac04fcdcfb8da6e84b050bd4b23ad3a9/main.cpp)
  は機種別のcycle/秒を固定している。kitは別PCでも使うため`steady_clock`を維持する。
- 有限の対数値を固定順で巡回する方式は、通常の乱数によるSAと同じ分布・独立性ではない。
  kitの追加方式では新しい乱数を毎回1個取り、曖昧な境界だけ通常計算へ戻す。
- HashMapはprobe上限に達すると別keyのslotへ上書きする。検索時のkey確認はあるが、
  **すべての項目を保持する通常の辞書ではない**。厳密な重複除去へ無条件には入れない。
- gridの大きさやtimerが共有変数の設計は短く書けるが、複数問題・複数instanceを
  同時に扱うkitでは、instanceごとの状態とcache寿命を優先する。
- 参考のheaderは前置きのマクロや標準header、一部はACLやC++20機能を前提にしている。
  kitはC++17の独立したhppと、展開済みmain.cppの形式を維持する。
- 候補を1つずつ入れ替えるqueueと、候補をまとめて上位N件へ絞る方式は用途が違う。
  後者を一律にsegment treeへ替えるのではなく、実問題のスコアで判断する。
- 確認したtreeにLICENSEファイルはなく、GitHubのlicenseメタデータもnullだった。
  第三者のコードは同梱せず、一般的な設計上の着想からkit向けに新規実装した。

## 今回の対数区間表: 元の採否を保つ

通常の受理条件を `delta > T * log(u)` とする。
2のべき乗個の区間へ乱数uを分け、logの下限・上限を前計算する。

1. 差分評価へは、実際の閾値以下である**下限**を渡す。
2. deltaが下限以下なら棄却、上限より大きければ採用できる。
3. 区間内だけ、元の `T * log(u)` を計算して決める。

下限で枝刈りするため、通常方式で採用可能な手を早期棄却しない。
区間内のfallbackにより、有限の表だけで希少な悪化手を切り捨てることもない。
表はinstanceごとに保持し、前計算では乱数を消費しない。
境界は浮動小数点の1 ULP外側へ広げる。通常の浮動小数点演算を使い、
`-ffast-math`/`-Ofast`は対象外とする。

通常log版と区間表版は、同じ温度・近傍列・乱数なら採否が同じになる。
ただし壁時計で止める場合、反復数と温度を更新する時点が変わるため、最終解は変わりうる。
前計算費用、cache負荷、枝刈りの下限を緩める費用もあるため、速度向上は保証しない。

`runner.annealing().set_threshold_table_size(4096);`を探索前に1行足すだけで使える。
0は無効（既定）。通常の`accept()`や`run()`には影響しない。
`run_with_threshold()`、または手書きループの`draw_acceptance_window()`で使う。
問題ごとの近傍・評価関数は書き換えなくてよい。

## 検証方針

- 小さい温度からdouble最大値まで、通常計算と閾値上下・採否を比較する。
- 閾値と同値、その前後1 ULP、無限大、NaN、u=0も確認する。
- 一定温度のRunner 20,000手で現在解・最良解・採用回数の一致を確認する。
- profilerは有効/無効の両ビルドで、ネスト・例外時の集計と無効時の空型・無出力を確認する。
- 速度測定だけで強化と判断せず、**AHC001の公式スコア**を同一入力・時間で比較する。
  生データと条件は[`practice/ahc001/README.md`](practice/ahc001/README.md)へ記録する。

今回のAHC001比較は開発10件で平均+0.0615%、別system20件で−0.0144%。
全出力合法だがスコア改善の確証はなく、区間表の既定は無効のままにした。

次の候補は「共通prefixの確定」「候補生成前の採用境界」「小種類の重み専用queue」。
この3機能は今回実装済みではない。必要な問題へ実際に当てはめ、既存の単純なAPIを
壊さずにスコア上の効果を確認できたものから追加する。
