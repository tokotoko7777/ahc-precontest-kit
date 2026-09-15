# AHC021 — 前線確定型の木上ビーム

[公式問題 Pyramid Sorting](https://atcoder.jp/contests/ahc021/tasks/ahc021_a)を、
`TreeBeamRunner`と`RadixHeap`を使って解く例です。
`main.cpp`にはヘッダを展開済みなので、その1ファイルだけで提出できます。
[使用例の原本](../../examples/search/ahc021_tree_beam.cpp)、
[幅別ベンチマーク](../../benchmarks/ahc021_tree_beam_score_benchmark.cpp)、
この提出ファイルは同じProblemを使い、展開の一致をCIで検査します。

## 問題ごとに書くところ

`BEGIN LIBRARY`〜`END LIBRARY`は共通処理です。
`PyramidProblem`と入出力を問題に合わせて編集します。

| 場所 | 書く内容・返す値 |
|---|---|
| `State` | 盤面、値から位置への逆引き、確定済みマス、次に運ぶ値、交換数、順位値、hash、前線のbit mask |
| `Move` | 次の小さい球を運ぶ経路と、その重み付き費用。ビーム1世代は「球を1個確定」で、経路内のswap数とは別 |
| `generate_moves` | その球を運べる前線マスへの経路候補。合法かつ到達可能な候補だけ返す |
| `apply_move` | 経路に沿ったswap、逆引き・hash・前線・確定数・交換数・費用を更新 |
| `revert_move` | 上記の全変更を戻す。盤面だけでなく前線のmaskも完全に復元 |
| `evaluate` | 子Stateの順位値そのもの。今回は費用なので、Runner構築時に最小化を指定 |
| `make_key` | 同じ盤面で同じ値になるhash。同じ世代なら確定領域も盤面から決まる |
| 完成候補の比較 | `for_each_state`で各候補の交換数を確認し、最少のrankを`restore(rank)`へ渡す |
| `validate`・出力 | 全操作の再生、盤面条件・交換数・逆引き・hash・前線の整合性を確認し、公式形式で出力 |

候補の共有履歴、DFS、上位N件、重複除去、候補巡回のapply/revertはライブラリ側です。
`for_each_state`のStateは借用参照なので保存しません。保存するのは候補のrankです。

## 解法と高速化

小さい数字から順に、「親が全て確定している未確定マス」へ運びます。
固定済みマスを通らない最短路を列挙し、大きな球を下へ押せる経路を優先します。
これを465世代繰り返すと、全ての親が子より小さい完成形になります。

前線をStateのmaskで差分管理し、必要な到達先の最短路が全て確定したらDijkstraを打ち切ります。
一部の前線が別の連結成分にある場合は、キューが空になるまで探索します。
同距離の頂点順も整数キーへ含めて従来の経路を保ち、RadixHeapの容量を再利用します。
経路配列は長さを数えて1回で確保し、候補ごとの繰り返し再確保を省きます。

途中の重み付き費用と正式scoreは異なります。
完成したビームをもう一度巡回し、正式scoreが最良になる交換数最少の候補を出力します。
これは探索候補を増やす処理ではなく、生成済みの完成解から選び直す処理です。
探索評価のrank 0より得点は下がりませんが、巡回の時間は必要です。

## 実行

```sh
g++ -std=c++17 -O2 -DNDEBUG main.cpp -o main
./main < input.txt > output.txt
# 幅を変える
g++ -std=c++17 -O2 -DNDEBUG -DAHC021_BEAM_WIDTH=100 main.cpp -o main-small
# 比較用：完成候補を選び直さず、探索順位のrank 0を返す
g++ -std=c++17 -O2 -DNDEBUG -DAHC021_FINAL_SCORE_SELECTION=0 main.cpp -o main-ranked
```

既定幅は300です。固定幅・固定世代数なので、同じ入力には同じ出力を返します。
手元の実行時間はAtCoderでの制限時間内完走を保証しません。
64bit hashには衝突可能性があります。

## スコア検証

2026-09-16の[公式入力による比較レポート](../../benchmarks/AHC021_CORE_REPORT.md)に、
旧practiceとの移行差、以前の構築ビームとの比較、完成候補の選び直し、
高速化だけの効果を分けて記録しています。

以前のpracticeは「違反辺swap・幅4」の別解法でした。旧実装は比較元commit
`9c0252c95ab4767dba538d47cdfaf45be5796ee1`に残しています。
大きな移行得点差をライブラリ単体の高速化率とは扱いません。
共通経路保持の試作や時間超過した幅も不採用として記録し、公式順位は主張しません。
