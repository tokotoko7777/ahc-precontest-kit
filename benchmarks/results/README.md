# 公式スコア比較の生データ

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
