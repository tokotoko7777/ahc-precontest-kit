# モンテカルロのバリエーション

| 方式 | ファイル | 選ぶ目安 |
|---|---|---|
| 共通未来のrollout | [rollout.cpp](rollout.cpp) | 全候補を同じ未来sampleで比べ、今の手を選ぶ |
| モンテカルロ木探索（UCT） | [tree-search.cpp](tree-search.cpp) | 試行を木の枝へ配分し、有望な手を深く調べる |

rolloutでは候補・未知の未来の抽選・仮実行の評価を書きます。
木探索では状態・合法手・遷移・報酬などを書きます。
細かい契約は各cppのTODOに書いてあり、両方を同時に覚える必要はありません。
実際に観測する前の情報を確定情報として使わないようにしてください。

完成例：[AHC015の共通未来rollout](../../../examples/search/ahc015_common_rollout.cpp) /
[AHC015の木探索](../../../examples/search/ahc015_uct.cpp)

未来が既知で乱数を使わず仮実行する場合は、関連する
[決定的rollout](../../advanced/deterministic-rollout.cpp)があります。
こちらはモンテカルロとは区別して置いています。

[手法一覧へ戻る](../README.md)
