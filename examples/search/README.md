# 探索コアを単体で使う完全な例

4種類の探索ヘッダを、それぞれ1個だけ使って問題を最初から最後まで解く
`main.cpp`です。小さなAPI例ではなく、入力、状態、近傍または遷移、得点計算、
解の保存、出力まで含みます。差分更新が有効な例では、その実装も確認できます。

| ファイル | 使う探索 | 題材 | 確認したこと |
|---|---|---|---|
| [`ahc006_sa.cpp`](ahc006_sa.cpp) | `TimeBasedSimulatedAnnealing` | AHC006 | 4近傍、合法性確認、最良解保存。ランダム1000注文の100ms確認で距離7136から5856へ改善し、50注文・集荷前配達の条件を満たした |
| [`intro_heuristics_simple_beam.cpp`](intro_heuristics_simple_beam.cpp) | `SimpleBeamSearch` | Introduction to Heuristics Contest A | 365日入力を最後まで構築し、出力日数・番号範囲・得点計算を確認した |
| [`ahc021_tree_beam.cpp`](ahc021_tree_beam.cpp) | `TreeBeamSearch` | AHC021 | 交換の`apply / revert`と差分評価。ランダム13ケースを全て合法な完成状態まで解いた |
| [`variable_cost_beam.cpp`](variable_cost_beam.cpp) | `CostTreeBeamSearch` | 締切付き宝集め | 1、2、3世代進む行動と再訪を扱う。200ランダムケースを厳密DPと照合した |

数値はライブラリの適用確認用で、AtCoder上の順位やスコアを主張するものでは
ありません。乱数seedは固定ですが、壁時計で終了するAHC006例の反復回数と結果は
実行負荷によって多少変わります。

## コンパイル

リポジトリのルートで実行します。

```sh
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc006_sa.cpp -o /tmp/ahc006_sa
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/intro_heuristics_simple_beam.cpp -o /tmp/intro_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/ahc021_tree_beam.cpp -o /tmp/ahc021_beam
g++ -std=c++17 -O2 -Wall -Wextra -pedantic examples/search/variable_cost_beam.cpp -o /tmp/cost_beam
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
1行を削除します。それ以外の補助ファイルや生成処理は必要ありません。

各例は仕組みを見通せることを優先しています。問題に合わせて最初に変える場所は、
焼きなましなら近傍と温度、ビームなら`evaluate`と幅です。
