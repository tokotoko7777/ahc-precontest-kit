# 補助的な先読み

ビーム・局所探索・モンテカルロの各バリエーションは、
[手法別フォルダ](../search/README.md)にまとめています。
複雑な形式を一律にこの場所へ分離する構成ではありません。

| ファイル | 使う場面 |
|---|---|
| [deterministic-rollout.cpp](deterministic-rollout.cpp) | 未来が既知で、各候補を決定的な方策で最後まで試したい |

乱数で未来を抽選しないため、モンテカルロとは区別します。
完成例は[AHC026](../../examples/search/ahc026_deterministic_rollout.cpp)と
[AHC058](../../examples/search/ahc058_deterministic_rollout.cpp)です。

部分破壊・再構築（LNS）、近傍の適応選択、ILS、途中再生、前後DPは
[局所探索](../search/local-search/README.md)へ、
枝刈り設定や多点スタートは[ビーム](../search/beam/README.md)へ、
UCTは[モンテカルロ](../search/monte-carlo/README.md)へ移しています。
既存のhppの場所とAPIは変えていません。
