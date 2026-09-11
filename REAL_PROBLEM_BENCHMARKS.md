# 実問題スコアベンチマーク

探索ライブラリは、合成データの速度だけでなく、その方式が実際に強かったAHCの
得点規則で確認します。AHC001/015/021/032は公式仕様と同じ分布から固定seedで
独自生成した入力、AHC038は公式ツール同梱のseed 0--99です。公開順位の得点は
別入力の相対評価を含むため、順位の再現ではありません。

| 方式 | 実問題 | 実行コマンド | 比較するもの |
|---|---|---|---|
| 時間焼きなまし | AHC001 Advertisement | `make benchmark-sa` | 同じ差分近傍の山登りと焼きなまし |
| 共通シナリオMonte Carlo | AHC015 Halloween Candy | `make benchmark-monte-carlo` | rollout数による最終公式score |
| apply/revert木上ビーム | AHC021 Pyramid Sorting | `make benchmark-tree-beam` | 幅による操作数と最終公式score |
| Action先行ビーム | AHC032 Mod Stamp | `make benchmark-search` | 幅による最終公式score |
| 世代飛ばし木上ビーム | AHC038 Tree Robot Arm | 下記公式ツール用script | 幅による公式操作ターン数 |

4本を続けて実行する場合は`make benchmark-real-search`です。ケース数、制限時間、
幅、sample数は各実行ファイルの引数で変更できます。

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

## AHC032: Action先行ビーム

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

## 正しさの確認

各ベンチマークは探索中の差分値をそのまま信用せず、完成解を別経路で再生します。
AHC001は長方形の境界・要求点・非重複と全score、AHC015は100ターンと公式連結成分
score、AHC021は全交換の合法性・完成盤面・公式score、AHC032は操作数・盤面・
差分scoreを検査します。AHC038は探索Stateとは別の盤面シミュレータと公式Rust
visualizerの両方で、全命令・最終盤面・操作ターン数を検査します。

## 参考資料

- [AHC001 Advertisement](https://atcoder.jp/contests/ahc001/tasks/ahc001_a)
- [AHC001参加記（TERRYのブログ）](https://blog.terry-u16.net/entry/ahc001)
- [AHC015 Halloween Candy](https://atcoder.jp/contests/ahc015/tasks/ahc015_a)
- [AHC015 4位解法](https://eijirou-kyopro.hatenablog.com/entry/2022/11/03/172820)
- [AHC015 1位相当解法](https://qiita.com/thun-c/items/8e7ae0249f1907854763)
- [AHC021 Pyramid Sorting](https://atcoder.jp/contests/ahc021/tasks/ahc021_a)
- [AHC021 1位相当解法の記録](https://amentorimaru.hatenablog.com/entry/2024/11/30/013751)
- [短期AHCで勝つためのテクニック](https://speakerdeck.com/shun_pi/duan-qi-ahcdesheng-tutamenotekunituku)
- [AHC032 Mod Stamp](https://atcoder.jp/contests/ahc032/tasks/ahc032_a)
- [AHC038 Tree Robot Arm](https://atcoder.jp/contests/ahc038/tasks/ahc038_a)
- [AHC038公式ツール](https://img.atcoder.jp/ahc038/GhBuR36w.zip)
