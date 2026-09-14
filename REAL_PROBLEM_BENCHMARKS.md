# 実問題スコアベンチマーク

探索ライブラリは、合成データの速度だけでなく、その方式が実際に強かったAHCの
得点規則で確認します。AHC001/015/021/032は公式仕様と同じ分布から固定seedで
独自生成した回帰入力です。これとは別にAHC002/011/026/032/038/058/061の公式配布入力も使います。公開順位の得点は
別入力の相対評価を含むため、順位の再現ではありません。

| 方式 | 実問題 | 実行コマンド | 比較するもの |
|---|---|---|---|
| 時間焼きなまし | AHC001 Advertisement | `make benchmark-sa` | 同じ差分近傍の山登りと焼きなまし |
| destroy/repair焼きなまし | AHC002 Walking on Tiles | 下記公式入力用script | 既存直書き版とRunner版の公式score |
| apply/revert木上ビーム | AHC011 Sliding Tree Puzzle | 下記入力用script | 既存直書き版とRunner版の公式score |
| 共通シナリオMonte Carlo | AHC015 Halloween Candy | `make benchmark-monte-carlo` | rollout数による最終公式score |
| apply/revert木上ビーム | AHC021 Pyramid Sorting | `make benchmark-tree-beam` | 幅による操作数と最終公式score |
| 決定的rollout | AHC026 Stack of Boxes | 下記公式入力用script | 既存直書き版とRunner版の公式score |
| Action先行ビーム | AHC032 Mod Stamp | `make benchmark-search` | 幅による最終公式score |
| 世代飛ばし木上ビーム | AHC038 Tree Robot Arm | 下記公式ツール用script | 幅による公式操作ターン数 |
| 決定的rollout | AHC058 Apple Incremental Game | `rollout_official_benchmark.py --task 058` | 3手先読みの移植前後・単一ファイルの公式score |
| 購入順序SA＋途中再生 | AHC058 Apple Incremental Game | `ahc058_annealing_benchmark.py` | 旧先読み解とSA強化版の公式score |
| 共通シナリオMonte Carlo | AHC061 Multi-Player Territory Game | `rollout_official_benchmark.py --task 061` | 対話testerによる移植前後・単一ファイルの公式score |
| Action先行ビーム＋区間LNS | AHC071 Wall Making | 下記公式入力用script | 参考`main3.cpp`との公式score |

4本を続けて実行する場合は`make benchmark-real-search`です。ケース数、制限時間、
幅、sample数は各実行ファイルの引数で変更できます。

## AHC002: destroy/repair焼きなまし

公式配布入力を展開した`in`ディレクトリを渡すと、既存の直書き版と
`TimeBasedAnnealingRunner`版を同じseedで実行します。script自身が全移動を再生し、
盤外移動と同一tileの再訪を検査してから公式scoreを計算します。

```sh
python3 benchmarks/ahc002_official_benchmark.py \
  --inputs /path/to/ahc002/in --cases 10
```

Runner版で人が書くのは、経路とcache、末尾または内部区間のdestroy/repair、
得点差、採用時の反映です。時計、温度、採否、best保存、bestからの再開は
ライブラリ側に分離しています。

公式配布seed 0--9を各1.87秒で実行した今回の手元測定です。

| 実装 | 平均score | 合計 | Runner版から見た勝敗 |
|---|---:|---:|---:|
| 既存の直書き版 | 56,674.10 | 566,741 | - |
| Runner形式版 | **57,302.30** | **573,023** | 6勝0分4敗 |

壁時計で停止するため再実行時の反復数とscoreは変動します。この比較から言えるのは、
フォーマット化したsolverが同じ実問題・同じ時間で合法に完走し、この10ケースでは
探索力を大きく失っていないことまでです。

## AHC001: 時間焼きなまし

問題側の編集箇所は`AdvertisementProblem`に集めています。

- `State`: 長方形と現在score
- `propose_move`: 長方形の辺を1本動かす近傍
- `evaluate_move`: 変更する長方形1個だけのscore差分
- `apply_move`: 採用された変更の反映

時計、温度、採否、現在解・最良解の保存は
`TimeBasedAnnealingRunner`が担当します。同じ近傍を使い、温度をほぼ0にした山登りと
通常の焼きなましを10ケース・各1秒で比べた手元測定は次の通りです。

| 方法 | 平均score |
|---|---:|
| 山登り | 747,401,361 |
| 焼きなまし | 915,108,673 |

約22.4%の改善でした。TERRYさんの参加記から換算した参考平均は約991,270,000です。
残る差には、破壊近傍、高温、多点スタートなど問題固有の工夫が含まれます。

## AHC011: apply/revert木上ビーム

[`ahc011_tree_beam.cpp`](examples/search/ahc011_tree_beam.cpp)では、盤面全体を
候補ごとに保存せず、`TreeBeamRunner`が共有履歴木をDFSしながらStateを1個だけ
`apply/revert`します。最大4方向は`FixedVector`で返すため、展開ごとのheap確保も
ありません。問題側に残るのはスライド、木評価、差分hash、局面key、終端判定です。

公式generatorで作った`0000.txt`形式の入力ディレクトリを渡すと、既存版とRunner版を
同じ入力で比較し、scriptが全スライド、最大木、公式scoreを独立に検査します。

```sh
python3 benchmarks/ahc011_official_benchmark.py \
  --inputs /path/to/ahc011/in --cases 10
```

公式サンプルでの手元確認では、両方とも最大木33、471,429点でした。Runner版は
約2.51秒・65 MiB、既存版は約2.70秒・158 MiBでした。1ケースだけの速度・品質なので
一般化はできませんが、フォーマット化後も同じ公式scoreへ到達し、共有Stateによる
メモリ削減が実際に働くことを確認する回帰点として記録しています。

## AHC015: 共通シナリオMonte Carlo

問題側の編集箇所は`CandyRolloutProblem`に集めています。

- `generate_actions`: 今このターンに選べる操作
- `generate_scenario`: まだ見えていない飴の配置順
- `evaluate_action`: 最初の1手を固定し、残りをルール方策で終端までsimulation

全Actionに同じ未来sampleを当てる処理、平均、最良Actionの選択は
`CommonScenarioRolloutRunner`が担当します。20ケース・512 sampleの手元測定は
平均801,542、平均約1.91秒/ケースでした。競技参加記で報告された上位相当の
平均806,382に近い水準です。

sample数を増やした10ケースの確認では、16 sampleの平均615,652から、64で
732,102、256で769,914、512で803,206へ改善しました。未来を最後まで完全探索する
のではなく、未知情報だけをsampleし、未来の自分の操作には問題固有のルール方策を
使うのが重要です。

## AHC021: apply/revert木上ビーム

問題側の編集箇所は`PyramidProblem`に集めています。

- `State`: 盤面、逆引き位置、確定済み頂点、差分hash、操作列
- `generate_moves`: 次の小さい球を確定可能な場所へ運ぶ経路候補
- `apply_move` / `revert_move`: 経路上の交換と全cacheの差分更新・完全復元
- `evaluate`: 大きい球を下へ押す経路を優先する順位値
- `make_key`: 同一局面を消す差分Zobrist hash

履歴木のDFS、State 1個の使い回し、上位N個の選抜、重複除去、世代ループは
`TreeBeamRunner`が担当します。30ケースの手元測定は次の通りです。

| 幅 | 平均score | 平均交換回数 |
|---:|---:|---:|
| 1 | 90,271.5 | 1,945.7 |
| 10 | 90,562.5 | 1,887.5 |
| 40 | 90,673.5 | 1,865.3 |

幅40は平均約0.25秒/ケースでした。初期上位解として紹介されている平均89,510.6を
参考値として上回りましたが、入力が異なるため順位相当とは断定しません。

## AHC026: 決定的rollout

[`ahc026_deterministic_rollout.cpp`](examples/search/ahc026_deterministic_rollout.cpp)
では、近い番号の箱を大きな塊から分離する範囲を`Action`にしています。全ての範囲を
最後まで同じ貪欲で仮実行し、消費energyが最小のActionだけを実Stateへ1段反映します。
未知情報がないためScenarioや乱数は不要です。

```sh
python3 benchmarks/ahc026_official_benchmark.py \
  --inputs /path/to/ahc026/tools/in --cases 10
```

公式ツールseed 0--9で既存直書き版と比較した手元測定です。両版とも全ケース合法で、
全seedの出力操作・energy・scoreが一致しました。

| 実装 | 平均score | 合計 | 平均実行時間 |
|---|---:|---:|---:|
| 既存の直書き版 | 9,334.00 | 93,340 | 317.2 ms |
| Runner形式版 | 9,334.00 | 93,340 | 308.1 ms |

1回ずつの壁時計測定なので小さな速度差は一般化しません。少なくとも、候補列と得点bufferを
Runnerへ分け、`location`を仮実行へコピーする構成で品質と速度を失っていないことを確認しました。

## AHC058・061: 既存rolloutのフォーマット化（2026-09-14時点）

AHC058は`DeterministicRolloutRunner`、AHC061は`CommonScenarioRolloutRunner`へ
候補比較を分離しました。`examples/search`にヘッダ分離版、`practice/ahc058/main.cpp`と
`practice/ahc061/main.cpp`にヘッダ展開済みの提出用1ファイルを置いています。
どちらも`TODO(AHC...)`から問題側の編集箇所をたどれます。

```sh
python3 benchmarks/rollout_official_benchmark.py --task 058 \
  --inputs /path/to/ahc058/tools/in --tool /path/to/ahc058/tools/target/release/vis \
  --cases 100 --output build/ahc058-rollout.csv
python3 benchmarks/rollout_official_benchmark.py --task 061 \
  --inputs /path/to/ahc061/in --tool /path/to/ahc061/target/release/tester \
  --cases 100 --output build/ahc061-rollout.csv
```

比較元の既定は移植前コミット`48e905a1b61eb70051d9fb7af18d34e13390cb69`です。
`--reference-ref`で変更できます。現行`practice`を旧版として比較してしまうことを避け、
コミットから取り出した旧版・Runner例・提出用1ファイルを同じ入力で順番に実行します。
主指標は公式の絶対scoreの平均・合計、補助指標は勝分敗と壁時計です。
CSVにはseedごとのscore、時間、入力・出力・ソースのSHA-256を残します。
既存CSVは上書きしません。公式ツールがないCIで独自の値を公式scoreとして代用しません。

AHC061は相手のモデルと将来乱数を共有し、旧版のSplitMix64、`double`加算順、
1e-12以内の同点処理を保ちます。盤面操作と粒子学習は問題依存であり、
Runner自身が相手モデルを推測するわけではありません。AHC058は状態を固定長配列に
してコピー時の動的確保をなくしました。探索深さ・候補数を増やす変更ではありません。

2026-09-14、公式配布seed 0〜99・各版`g++ -O2 -DNDEBUG`での結果です。

| 問題 | 移植前の平均score | Runner例の平均score | 単一ファイルの平均score | 勝/分/敗 |
|---|---:|---:|---:|---|
| AHC058 | 5,169,306.27 | 5,169,306.27 | 5,169,306.27 | 0/100/0 |
| AHC061 | 168,377.35 | 168,377.35 | 168,377.35 | 0/100/0 |

2問とも全100ケースで出力とscoreが完全一致し、公式ツールで合法に完走しました。
得点向上ではなく、既存の探索力を保った問題依存部分の分離を確認した結果です。
合計scoreはAHC058が516,930,627、AHC061が16,837,735でした。
生データは[`benchmarks/results/`](benchmarks/results/README.md)に保存しています。
AHC058の後半は他の検証も並行したため、壁時計から小さな高速化率は主張しません。
両例のseed 0はASan/UBSanでも検出0件でした（リーク検査のみ無効）。

## AHC058: 購入順序SA＋差分再生（2026-09-15）

短期4時間問題で、`TimeBasedAnnealingRunner`と新しい`PrefixReplay`を組み合わせました。
初期解は従来の3手先読み。人が書く部分は購入列・近傍・1購入の実行・最終score・
出力で、採否と時間管理はSA、途中状態の保存と仮cacheの確定はPrefixReplayの担当です。
資金不足の待機区間も閉形式と二分探索でまとめて進めます。

| 同じ公式入力 | 従来平均 | 新SA平均 | 勝/分/敗 |
|---|---:|---:|---:|
| seed 0〜9 | 5,192,506.70 | 5,481,894.70 | 10/0/0 |
| 設定固定後のseed 10〜99 | 5,166,728.44 | 5,492,770.44 | 90/0/0 |
| 全100ケース | 5,169,306.27 | **5,491,682.87** | **100/0/0** |

全体平均は約6.24%増、新版合計549,168,287点。全出力を公式visで採点し、
最長壁時計は1.852秒でした（設定は初期解構築込み1,850ms）。時間ベースなので
再測定で反復数と得点が変わります。公式順位・提出環境での時間保証は主張しません。
測定は各1回で、重いビルド・他solverは並行していません。

```sh
python3 benchmarks/ahc058_annealing_benchmark.py \
  --inputs /path/to/ahc058/tools/in --tool /path/to/ahc058/tools/target/release/vis \
  --cases 100 --output build/ahc058-prefix.csv
```

生データは[`開発10ケース`](benchmarks/results/ahc058-prefix-dev-10.csv)と
[`追加90ケース`](benchmarks/results/ahc058-prefix-holdout-90.csv)。旧rollout移植比較用scriptは
AHC058の単一ファイルだけ旧commitへ固定しているので、上のSA比較とは区別してください。
[公式解説](https://img.atcoder.jp/ahc058/editorial.pdf)の購入順序SA・構築法との組合せを
参考にしました。強いビームで初期解を作る方法や段階的な購入先絞り込みは未実装です。

## AHC032: Action先行ビーム

2026-09-15には公式配布seed 0〜9を旧practice・旧Runner例・幅6,000の新版で比較しました。
平均scoreは順に73,487,381,446.50、78,996,169,390.50、79,159,150,255.20。
新版は旧practiceへ10勝、旧Runnerへ9勝1分でした。旧practice比は約7.72%増です。
最長壁時計は新版1.115秒。生データは
[`ahc032-width6000-dev-10.csv`](benchmarks/results/ahc032-width6000-dev-10.csv)です。

設定固定後の公式seed 10〜99も全出力が合法でした。全100ケースの平均scoreは
旧practice 73,494,359,900.33、旧Runner 78,973,301,951.56、新版 **79,132,304,281.58**。
旧practiceへ100勝、旧Runnerへ85勝5分10敗で、平均はそれぞれ約7.67%・約0.20%増です。
幅拡大で悪化する10ケースも隠さず記録しています。追加測定の最長壁時計は1.394秒でした。
生データは[`追加90ケース`](benchmarks/results/ahc032-width6000-holdout-90.csv)。

```sh
python3 benchmarks/ahc032_official_benchmark.py \
  --inputs /path/to/ahc032/tools/in --tool /path/to/ahc032/tools/target/release/vis \
  --cases 100 --output build/ahc032-official.csv
```

問題実装の重複をなくし、下の独自生成ベンチマークも`examples/search`の実装を直接使う
ようにしました。`practice`には同じコードをヘッダ展開して置き、一致をCIで検査します。
[公式解説](https://img.atcoder.jp/ahc032/editorial.pdf)を参考にした終盤7枚の候補追加も
試しましたが、手数予約込みの開発10ケースでは幅1,000従来版へ10敗でした。
この実験は既定で無効です。詳しい編集箇所・結果は[`practice/ahc032`](practice/ahc032/README.md)。

### 独自生成の回帰ベンチマーク

AHC032は`ActionBeamRunner`で、全候補のStateを作る前に軽いActionと差分順位値を
上位N件へ絞ります。固定5ケースでは幅1のscore合計382,455,918,414から、幅10,000の
395,715,164,556へ改善しました。詳細な順位評価、操作上限検査、公開得点との比較は
[`PERFORMANCE.md`](PERFORMANCE.md)にまとめています。

## AHC038: 世代飛ばしapply/revertビーム

[`ahc038_variable_cost_beam.cpp`](examples/search/ahc038_variable_cost_beam.cpp)
は、次に対象とする指・マス・向きを1個の`Move`にします。根の移動と指の回転を
同時に進めるため、1手の消費ターンは候補ごとに異なります。

- `State`: 根、各指の向き、保持bit、残る供給・需要マスのbitset
- `generate_moves`: 各指から上位2候補、全体から上位16候補
- `apply_move`: 対象マスまでの複数命令を作り、通過中も他の指で拾う・置く
- `revert_move`: 命令を逆順に`P -> 回転 -> 根移動`と戻す
- `evaluate`: 未実行の`P`数を最優先し、保持数と根位置で同点を分ける
- `make_key`: 差分Zobrist hashで同じ到着ターン・同じ局面を1個にまとめる

公式ツール同梱seed 0--99、公式visualizerで全解を再生した結果です。scoreは完成時の
操作ターン数なので小さいほど良い値です。

| 方法 | 平均score | 合計 | 既存星型貪欲との勝敗 |
|---|---:|---:|---:|
| 既存星型貪欲 | 313.95 | 31,395 | - |
| 新候補生成の貪欲 | 320.42 | 32,042 | 41勝59敗 |
| ビーム幅1 | 250.10 | 25,010 | 97勝3敗 |
| ビーム幅3 | 237.72 | 23,772 | 99勝1敗 |
| ビーム幅6 | 234.75 | 23,475 | 100勝0敗 |
| ビーム幅7 | **234.43** | **23,443** | **99勝1敗** |
| ビーム幅8 | 239.67 | 23,967 | 98勝2敗 |

幅7は既存貪欲から平均ターン数を約25.3%削減しました。幅を広げれば単調に良くなる
とは限りません。同じ2.6秒上限では、幅24は探索の深い完了状態へ到達しにくく、
先頭10ケース平均297.30でした。この問題では「1世代の候補数」だけでなく、制限時間
内に完成解まで届く探索速度も評価対象です。重複除去を入れる前の幅3は244.78、
導入後は237.72で、同じ候補へ探索量を重ねない効果も実得点で確認しました。

公式zipを展開し、Rust visualizerを一度buildした`tools`ディレクトリを渡すと再現
できます。未buildならscriptが`cargo build -r --bin vis`を実行します。

```sh
benchmarks/ahc038_official_score_benchmark.sh /path/to/ahc038/tools 100
```

コンテスト1位のnikajさんは、複数の腕設計、到達姿勢の前計算、手先候補制限、
bitset、状態hashを組み合わせた探索です。この例にもbitsetと候補制限は入っていますが、
腕は固定星型で、姿勢前計算と重複除去も未導入です。したがって上位解との残差は
ライブラリの上位N個選択より、主にこの`TODO(AHC038)`側にあります。

## AHC071: Action先行ビーム＋区間LNS

[`ahc071_action_beam.cpp`](examples/search/ahc071_action_beam.cpp)は、上から下へ1段ずつ
行DPの候補を作り、次段へ必要な中心bitsetだけをStateに持ちます。候補Actionの費用を
先に比較してから採用分だけStateを作り、同じ中心bitsetは最安の1件へまとめます。
完成解の費用を超える候補は`evaluate_action_with_threshold`で途中打ち切りします。

提出用の自己完結した1ファイルは[`practice/ahc071`](practice/ahc071/)です。

```sh
python3 benchmarks/ahc071_official_benchmark.py \
  --tools /path/to/AHC071 \
  --solver practice/ahc071/main.cpp \
  --reference /path/to/AHC071/main3.cpp \
  --ported-score --cases 10
```

公式seed 0000--0009、各1.8秒の再測定では全出力が合法でした。

| 実装 | 平均score | 合計 | kit版から見た勝敗 |
|---|---:|---:|---:|
| kitフォーマット版 | 10,652.30 | 106,523 | - |
| 参考`main3.cpp` | 10,652.50 | 106,525 | 2勝4分4敗 |

両方とも壁時計で探索を停止するため数点変動します。この比較は、上位者一般との順位比較では
なく、ユーザー指定の参考実装と同じ公式入力・時間でフォーマット版の探索力を確認したものです。

## 正しさの確認

各ベンチマークは探索中の差分値をそのまま信用せず、完成解を別経路で再生します。
AHC001は長方形の境界・要求点・非重複と全score、AHC015は100ターンと公式連結成分
score、AHC021は全交換の合法性・完成盤面・公式score、AHC026は全箱操作・energy・
公式score、AHC032は操作数・盤面・
差分scoreを検査します。AHC038は探索Stateとは別の盤面シミュレータと公式Rust
visualizerの両方で、全命令・最終盤面・操作ターン数を検査します。AHC071は全レンガの
範囲・重複・支持条件・穴の被覆と公式scoreを別の採点処理で検査します。

## 参考資料

- [AHC001 Advertisement](https://atcoder.jp/contests/ahc001/tasks/ahc001_a)
- [AHC001参加記（TERRYのブログ）](https://blog.terry-u16.net/entry/ahc001)
- [AHC002 Walking on Tiles](https://atcoder.jp/contests/ahc002/tasks/ahc002_a)
- [AHC011 Sliding Tree Puzzle](https://atcoder.jp/contests/ahc011/tasks/ahc011_a)
- [AHC015 Halloween Candy](https://atcoder.jp/contests/ahc015/tasks/ahc015_a)
- [AHC015 4位解法](https://eijirou-kyopro.hatenablog.com/entry/2022/11/03/172820)
- [AHC015 1位相当解法](https://qiita.com/thun-c/items/8e7ae0249f1907854763)
- [AHC021 Pyramid Sorting](https://atcoder.jp/contests/ahc021/tasks/ahc021_a)
- [AHC021 1位相当解法の記録](https://amentorimaru.hatenablog.com/entry/2024/11/30/013751)
- [短期AHCで勝つためのテクニック](https://speakerdeck.com/shun_pi/duan-qi-ahcdesheng-tutamenotekunituku)
- [AHC026 Stack of Boxes](https://atcoder.jp/contests/ahc026/tasks/ahc026_a)
- [AHC032 Mod Stamp](https://atcoder.jp/contests/ahc032/tasks/ahc032_a)
- [AHC038 Tree Robot Arm](https://atcoder.jp/contests/ahc038/tasks/ahc038_a)
- [AHC038公式ツール](https://img.atcoder.jp/ahc038/GhBuR36w.zip)
- [AHC058 Apple Incremental Game](https://atcoder.jp/contests/ahc058/tasks/ahc058_a)
- [AHC058公式ツール](https://img.atcoder.jp/ahc058/UpvAVdx6.zip)
- [AHC061 Multi-Player Territory Game](https://atcoder.jp/contests/ahc061/tasks/ahc061_a)
- [AHC071 Wall Making](https://atcoder.jp/contests/ahc071/tasks/ahc071_a)
