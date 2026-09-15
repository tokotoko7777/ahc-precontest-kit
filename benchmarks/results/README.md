# 公式スコア比較の生データ

## AHC001の領域再構築SA（2026-09-15）

`ahc001-region-development-10.csv`は公式ツール付属の開発seed 0〜9で、
旧再帰領域分割と新SAを比較した記録です。10勝0分0敗、平均約3.62%増。
この設定を変えずに公開システムseedの先頭100件を採点した記録が
`ahc001-region-system-100.csv`です。開発用seed番号とシステムseed一覧の
行番号は別物なので、`seed`・`suite`・`seed_manifest_md5`も保存しています。
各版は逐次実行し、採点中に他solverや重いビルドを並行していません。
時間ベースなので同じsource・入力でも再実行時の得点は変動します。
source hashは原本.cppのものです。依存ヘッダはこの結果を追加したcommitの
`library/`を使用し、展開済みpracticeとの一致をCIで検査しています。

1位の1,000ケース平均に対して、100ケースの平均は参考比較に留めます。
正確な出典、保存済み得点の再確認ができていない注記、平均点差・1位比・損失倍率、
全1,000件の追試手順は[`practice/ahc001/README.md`](../../practice/ahc001/README.md)です。

## 購入順序SAの追加（2026-09-15）

`ahc058-prefix-dev-10.csv`と`ahc058-prefix-holdout-90.csv`は
`ahc058_annealing_benchmark.py`の新旧比較です。最初の10ケース確認後、
設定を固定して残り90ケースを測定しました。`reference`は旧practice、
`prefix_sa`は新しいSA例です。全100ケースで改善、平均約6.24%増でした。
時間ベースなので再実行時にスコアは変動します。採点中に重いビルド・他solverは並行していません。

## AHC032の公式スコア比較（2026-09-15）

`ahc032-width6000-dev-10.csv`と`ahc032-width6000-holdout-90.csv`は旧practice、
旧幅1,000のRunner、新幅6,000のRunnerの同一公式seed比較です。最初の10ケースを
確認して設定を固定し、残り90ケースを採点しました。主指標はscoreで、秒数は補助値です。
全100ケースで旧practiceへ100勝、旧Runnerへ85勝5分10敗。平均は約7.67%・約0.20%増です。
`ahc032-end7-no-reserve-dev-10.csv`と`ahc032-end7-reserved-dev-10.csv`は開発中の
幅1,000・5/6/7枚を各4,096候補追加した実験の記録で、採用版の結果ではありません。
前者は手数配分を据え置き、後者は最後に7手を予約しています。
後者は現在のコードでも`AHC032_BEAM_WIDTH=1000`と`AHC032_END_COMBOS=4096`で
再現できます。悪化した案を、既定の強化版としては採用していません。

## 旧rolloutの移植比較（2026-09-14）

`rollout_official_benchmark.py`によるAHC058・061の公式配布seed 0〜99の測定です。
1つのseedにつき`reference`（移植前）、`formatted`（Runner例）、
`standalone`（ヘッダ展開済みpractice）の3行があります。

- `score`が主指標。異なる問題のscore同士は合計して評価しません。
- 比較元は`reference_commit`、実際にコンパイルした.cppは`source_sha256`で識別。
  Runner例の依存ヘッダは同じ変更コミットの`library/`を使っています。
- `input_sha256`で公式ケース、`output_sha256`で出力の一致を確認できます。
  AHC061の出力は公式testerが記録した対話の出力です。
- `seconds`は手元で1回測った壁時計です。AHC061ではtesterの起動・対話も含み、
  AHC058では採点器の実行時間を含みません。
- AHC058の後半は全体テスト・sanitizerのビルドも並行したため、時間の小差から
  高速化率や提出環境での制限時間を保証しません。固定探索なのでscoreは負荷に依存しません。

集計と再実行手順は[`REAL_PROBLEM_BENCHMARKS.md`](../../REAL_PROBLEM_BENCHMARKS.md)
を参照してください。入力本体・第三者提出コード・公式ツール本体は同梱していません。
