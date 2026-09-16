# 探索ライブラリの穴埋めテンプレート

各ファイルは、ライブラリを読み込んだ直後に問題依存コードをまとめた`main.cpp`の
雛形です。コメントだけのAPI一覧ではなく、実際に埋める型と関数がコンパイル可能な
位置に置かれています。

1. 使いたい方式のファイルを`main.cpp`へコピーする。
2. `TODO:`を上から順に埋める。
3. 開発中はリポジトリ直下で`#include "library/....hpp"`を使用する。
4. 提出時はその`#include`を、使用したhppの全文へ置き換える。

| ファイル | 方式 | 主に埋める関数 |
|---|---|---|
| [`time-based-annealing.cpp`](time-based-annealing.cpp) | 時間焼きなまし | `propose_move`、`evaluate_move`、`apply_move` |
| [`prefix-replay-annealing.cpp`](prefix-replay-annealing.cpp) | 行動列の途中から再生する焼きなまし | `advance`、`evaluate_end`、`propose_move`。仮cacheの確定まで配置済み |
| [`simple-beam.cpp`](simple-beam.cpp) | 通常ビーム | `expand`、`evaluate` |
| [`action-beam.cpp`](action-beam.cpp) | Action差分ビーム | `generate_actions`、`evaluate_action`、`apply_action` |
| [`tree-beam.cpp`](tree-beam.cpp) | apply/revert木上ビーム | `generate_moves`、`apply_move`、`revert_move`、`evaluate` |
| [`variable-cost-tree-beam.cpp`](variable-cost-tree-beam.cpp) | 世代飛ばし木上ビーム | 上記に加えて`get_advance` |
| [`monte-carlo-rollout.cpp`](monte-carlo-rollout.cpp) | 共通シナリオMonte Carlo | `generate_scenario`、`evaluate_action`、`apply_real_action` |
| [`coalesced-monte-carlo.cpp`](coalesced-monte-carlo.cpp) | 同一状態以降を共有するMonte Carlo | `start_rollout`、`advance_rollout`、`evaluate_rollout`、仮状態の等価判定 |
| [`deterministic-rollout.cpp`](deterministic-rollout.cpp) | 決定的な完走・先読み評価 | `generate_actions`、`evaluate_action`、`apply_real_action` |

`TODO: 【重複除去する場合だけ】`のように書かれた項目は任意です。まずhashなしで
動かし、同一局面が多いと確認できてから追加できます。

時間焼きなましは、先頭の`PRECOMPUTE_ACCEPTANCE`（対数表）と
`STOP_SCORE_EARLY`（スコア途中打ち切り）を独立に`true / false`で選べます。
前計算OFFでも途中打ち切り可能です。評価関数は`evaluate_move(state, move, threshold)`の
1個だけです。何を計算し、いつ`nullopt`を返してよいかの例をコメントで置いています。
安全な上限が作れない場合は閾値を無視して全差分を計算してください。
途中打ち切りOFFでは、別関数へ切り替えず同じ関数に`-∞`を渡します。

この配置は、変更しない探索ライブラリと、問題ごとに実装する`Action`・`State`・
状態遷移関数を視覚的に分離する
[thun-cさんの差分更新ビームサーチライブラリ](https://qiita.com/thun-c/items/a29c80f7ba54b271a6c7)
の考え方を参考にしています。このkitでは同じ見た目を焼きなまし、通常ビーム、
Action差分ビーム、2種類の木上ビーム、Monte Carlo、決定的rolloutへ揃えています。
