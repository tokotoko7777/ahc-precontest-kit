# 局所探索：焼きなまし・山登り

焼きなましと山登りは「近傍を作る→評価する→採用したら更新する」が共通です。
採用判定が違うだけなので、同じフォルダ・同じ基本形にまとめています。
部分破壊・再構築（LNS）も、独立した探索方式ではなく近傍のバリエーションとしてここに置きます。

| バリエーション | ファイル | 選ぶ目安 |
|---|---|---|
| 焼きなまし・山登り共通 | [basic.cpp](basic.cpp) | 通常はこれ。差分評価と採用時だけの更新 |
| 焼きなましの任意設定 | [annealing-options.cpp](annealing-options.cpp) | 受理判定の前計算・評価打ち切りを個別にON/OFFする |
| 部分破壊・再構築 | [destroy-repair.cpp](destroy-repair.cpp) | destroyとrepairを分けて書く。初期設定は山登り、SAにも切替可能 |
| 部分破壊・再構築＋近傍選択 | [adaptive-destroy-repair.cpp](adaptive-destroy-repair.cpp) | 複数の壊し方を成果に応じて選ぶ。採否はSA/山登りを選べる |
| 反復局所探索（ILS） | [iterated-local-search.cpp](iterated-local-search.cpp) | 大きな変更と小さい局所改善を分けて繰り返す |
| 操作列の途中再生 | [prefix-replay.cpp](prefix-replay.cpp) | 変更した位置からだけ再計算する焼きなまし |
| 前後DP | [forward-backward.cpp](forward-backward.cpp) | 有限状態の確率DPで、1操作変更を前後の表から評価する焼きなまし |

## 基本形の切り替え

basic.cppのUSE_ANNEALINGだけを変えます。

- true：焼きなまし。悪化も温度に応じて採用する。
- false：山登り。改善量>0だけ採用する。同点も棄却する。

State・Move・propose_move・evaluate_move・apply_moveは共通です。
evaluate_moveは**正なら良化する改善量**を返します。
最大化なら新得点−旧得点、最小化なら旧cost−新costです。
採用前に現在Stateを書き換えないため、不採用手のundoは不要です。
山登りでは温度を採否に使いません。温度を0にして代用する必要はありません。

## 部分破壊・再構築も同じ仲間

basic.cppのMoveを「一部を壊して作り直す変更」にしても構いません。
[AHC002の例](../../../examples/search/ahc002_destroy_repair_sa.cpp)がこの書き方です。

destroyとrepairを別関数にしたいときはdestroy-repair.cppを使います。
LnsAcceptance::HillClimbingが山登り、SimulatedAnnealingが焼きなましです。
こちらの山登りは**同点も採用**します。基本形と同点の扱いが異なる点に注意してください。
候補の修復中に現在解は変更せず、失敗はnulloptで返します。

## 戻り値を混同しない

| 形式・関数 | 返すもの |
|---|---|
| 基本形・設定付きのevaluate_move | 改善量 |
| 部分破壊・再構築のrepair | 修復した完成候補の絶対得点。失敗はnullopt |
| ILSのlocal_search | 局所改善後の完成候補の絶対得点 |
| 途中再生・前後DP | 表や再生状態を更新する関数を実装。テンプレート側が改善量へ変換 |

部分破壊・再構築のthresholdも絶対得点の境界です。改善量の境界と取り違えないでください。
各cppのTODOに引数・戻り値・変更してよい状態を説明しています。
すべてを同じProblem型へ無理に統一せず、必要な形式だけ選んで書けます。

前計算と評価打ち切りは独立です。安全な上限・下限を証明できない場合は最後まで評価します。
前後DPは有限状態・加算報酬などの条件が必要です。

完成例：[AHC006の差分評価](../../../examples/search/ahc006_sa.cpp) /
[AHC059の部分再構築](../../../examples/search/ahc059_lns.cpp) /
[AHC059のILS](../../../examples/search/ahc059_ils.cpp) /
[AHC058の途中再生](../../../examples/search/ahc058_prefix_sa.cpp)

[手法一覧へ戻る](../README.md) / [APIリファレンス](../../../SEARCH_REFERENCE.md)
