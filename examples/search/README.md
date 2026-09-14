# 探索コアを単体で使う完全な例

探索Runnerを実問題へ当てはめ、問題を最初から最後まで解く`main.cpp`です。
小さなAPI例ではなく、入力、状態、近傍または遷移、得点計算、
解の保存、出力まで含みます。差分更新が有効な例では、その実装も確認できます。

| ファイル | 使う探索 | 題材 | 確認したこと |
|---|---|---|---|
| [`ahc006_sa.cpp`](ahc006_sa.cpp) | `TimeBasedAnnealingRunner` | AHC006 | `DeliveryProblem`へState・Move・近傍・差分・反映を分離。固定長Routeで毎試行のvector確保を避ける |
| [`ahc015_common_rollout.cpp`](ahc015_common_rollout.cpp) | `CommonScenarioRolloutRunner` | AHC015 | 4方向を同じ未来配置で比較。盤面操作・Scenario・rollout評価と共通乱数処理の境界を明示 |
| [`intro_heuristics_simple_beam.cpp`](intro_heuristics_simple_beam.cpp) | `SimpleBeamSearch` | Introduction to Heuristics Contest A | 365日入力を最後まで構築し、出力日数・番号範囲・得点計算を確認した |
| [`intro_heuristics_action_beam.cpp`](intro_heuristics_action_beam.cpp) | `ActionBeamRunner` | Introduction to Heuristics Contest A | 問題依存コードを1 structへ分離。State・Action・Scoreと3関数へ何を書き何を返すか、行ごとのコメント付き |
| [`ahc021_tree_beam.cpp`](ahc021_tree_beam.cpp) | `TreeBeamRunner` | AHC021 | `PyramidProblem`へ候補生成・`apply/revert`・差分評価・hashを分離。履歴木と上位選抜はRunner側 |
| [`ahc032_action_beam.cpp`](ahc032_action_beam.cpp) | `ActionBeamRunner` | AHC032 | 3×3スタンプ多重集合を2-byte Action化。採用候補だけ盤面をコピーし、49位置を順に確定 |
| [`variable_cost_beam.cpp`](variable_cost_beam.cpp) | `CostTreeBeamSearch` | 締切付き宝集め | 1、2、3世代進む行動と再訪を扱う。200ランダムケースを厳密DPと照合した |
| [`ahc038_variable_cost_beam.cpp`](ahc038_variable_cost_beam.cpp) | `CostTreeBeamSearch` | AHC038 | 「次の把持・解放」まで1手で世代を飛ばす。盤面bitsetと姿勢をapply/revertし、公式seed 0--99を全て合法に完了した |
| [`ahc071_action_beam.cpp`](ahc071_action_beam.cpp) | `ActionBeamRunner` | AHC071 | 上段から必要な支持位置を渡す行DP。全体構築と区間再構築を同じProblem型で行い、同じ次段条件をkeyでまとめる |

数値はライブラリの適用確認用で、AtCoder上の順位やスコアを主張するものでは
ありません。乱数seedは固定ですが、壁時計で終了するAHC006例の反復回数と結果は
実行負荷によって多少変わります。

AHC006・015・021・032のRunner形式4本は、決定的な生成入力を使った短時間
スモークテストで、出力行数・値域・操作数を確認しています。AHC006は経路、
AHC021は操作再生後の差分score、AHC032は盤面と最終scoreも全再計算と照合します。
これは入出力と実装整合性の確認であり、公式seedの得点比較ではありません。

ビーム幅による問題本来の得点差は、`make benchmark-search`でAHC032
「Mod Stamp」相当の固定5ケースを解いて確認できます。これは公式入力ではなく、
公式仕様と同じ値域・分布で独自生成した回帰ベンチマークです。
`ActionBeamRunner`が上位N個の選択・Stateの生成・探索ループを担当し、
問題側には確定順、候補生成、差分評価、反映だけを書いています。

焼きなまし、Monte Carlo、apply/revert木上ビームにも同じ境界のRunnerと実問題
ベンチマークがあります。各方式で人が書く箇所、ライブラリが担当する箇所、得点は
[`REAL_PROBLEM_BENCHMARKS.md`](../../REAL_PROBLEM_BENCHMARKS.md)を参照してください。

## コンパイル

リポジトリのルートで実行します。

```sh
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc006_sa.cpp -o /tmp/ahc006_sa
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc015_common_rollout.cpp -o /tmp/ahc015_rollout
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/intro_heuristics_simple_beam.cpp -o /tmp/intro_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/intro_heuristics_action_beam.cpp -o /tmp/action_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc021_tree_beam.cpp -o /tmp/ahc021_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc032_action_beam.cpp -o /tmp/ahc032_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/variable_cost_beam.cpp -o /tmp/cost_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc038_variable_cost_beam.cpp -o /tmp/ahc038_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc071_action_beam.cpp -o /tmp/ahc071_beam
```

世代飛ばし版には小さい厳密DPとの自己テストも同じファイルに入っています。

```sh
g++ -std=c++17 -O2 -Wall -Wextra -pedantic \
  -DVARIABLE_COST_BEAM_SELF_TEST examples/search/variable_cost_beam.cpp \
  -o /tmp/cost_beam_test
/tmp/cost_beam_test
```

## 提出用の1ファイルにする

例では読みやすさのため、次のようにリポジトリ内のヘッダを参照しています。

```cpp
#include "../../library/simple-beam-search.hpp"
```

実際に提出する時は、そのヘッダの中身をこの行の位置へ丸ごと貼り、`#include`の
1行を削除します。それ以外の補助ファイルや生成処理は必要ありません。公開済みcommit
から自動で1ファイル化する場合は、`copy_part.py`がこの相対includeも除去します。

```sh
python3 tools/copy_part.py --ref <公開済みSHA> \
  --main examples/search/ahc038_variable_cost_beam.cpp \
  library/cost-tree-beam-search.hpp -o submission.cpp
```

各例は仕組みを見通せることを優先しています。問題に合わせて最初に変える場所は、
焼きなましなら近傍と温度、ビームなら`evaluate`と幅です。

AHC038例は、特に触る場所へ`TODO(AHC038)`を付けています。`State`、`Move`、
候補生成、差分適用・復元、評価関数、腕形状、ビーム幅の順に読めます。幅だけを
比較する時は`-DAHC038_BEAM_WIDTH=1`のようにコンパイル時指定できます。

AHC071例も同様に`TODO(AHC071)`を検索できます。1行の最小費用DP、その上位候補列挙、
将来費用、`State / Action / Score`、候補生成、評価、反映、同一状態key、完成済み解を
使う費用閾値の順です。公式入力とvisualizerがある場合は、次で別solverとも同一seedを
比較できます。

```sh
python3 benchmarks/ahc071_official_benchmark.py \
  --tools /path/to/AHC071 --reference /path/to/AHC071/main3.cpp --cases 10
```

Rust toolchainを使えない環境では`--ported-score`を付けると、公式`src/lib.rs`の
合法性検査とscore式を移植したPython版で採点します。
