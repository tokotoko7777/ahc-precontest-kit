# 探索の基本ガイド

**ビーム・局所探索（焼きなまし／山登り）・モンテカルロ**のフォルダに分けています。
各フォルダに複数の形式があるので、問題に合うcppを選んでTODOを埋めます。
hpp本体は通常編集しません。

| やりたいこと | フォルダとバリエーション |
|---|---|
| 良い候補を複数残しながら、解を1手ずつ作る | [ビーム](template/search/beam/README.md)：通常・差分評価・木上など |
| 完成解を変更して改善する | [局所探索](template/search/local-search/README.md)：焼きなまし・山登り・部分破壊再構築など |
| 未知の未来を何本か試して、今の手を選ぶ | [モンテカルロ](template/search/monte-carlo/README.md)：rollout・木探索 |

## 共通の使い方

1. 選んだcppを自分のmain.cppへコピーする。
2. 型、候補・近傍、評価、更新、入出力のTODOを埋める。
3. 提出時はライブラリのincludeをhpp全文で置き換える。提出するのはmain.cppだけ。

hppにはinclude直後に出典URLがあります。公開版を固定してコピーする方法は
[PRECONTEST.md](PRECONTEST.md)を参照してください。

## ビームの違い

[beam/](template/search/beam/README.md)には通常・差分評価・木上・世代飛ばし木上に加え、
chokudai、枝刈り設定、多点スタートを置いています。
迷ったら通常版。状態のコピーが重い場合は、差分評価版や木上版を検討します。

評価は「その候補の順位値」を返します。得点差ではありません。
木上版のapply/revertは、盤面だけでなく評価・hash・cacheも元に戻します。

## 焼きなましと山登りは同じ基本形

[local-search/basic.cpp](template/search/local-search/basic.cpp)のUSE_ANNEALINGを
trueにすると焼きなまし、falseにすると山登りです。
State・Move・propose_move・evaluate_move・apply_moveは共通です。
山登りは改善量>0だけ採用し、同点も棄却します。温度を0にして代用はしません。

| 編集する場所 | 書くもの・返す値 |
|---|---|
| State / Move | 現在解・差分cache / 1回の変更 |
| propose_move | 近傍を1つ。作れないときはnullopt |
| evaluate_move | **改善量**。最大化は新得点−旧得点、最小化は旧cost−新cost |
| apply_move | 採用手だけ反映。cacheも更新 |
| 初期評価 | 絶対得点。コスト最小化なら-cost |

評価中に現在Stateを変更しないので、不採用手のundoは不要です。
thresholdは最初は無視して正確に計算すれば動きます。
差分更新の完成例は[AHC006](examples/search/ahc006_sa.cpp)です。

### 部分破壊・再構築も局所探索の中に置く

「一部を壊す→作り直す」を基本形の1つのMoveにまとめても構いません。
[AHC002の例](examples/search/ahc002_destroy_repair_sa.cpp)がこの書き方です。
destroyとrepairを分けて書きたいときは
[destroy-repair.cpp](template/search/local-search/destroy-repair.cpp)を選びます。
初期設定は山登りで、LnsAcceptance::SimulatedAnnealingへ変えると焼きなましです。

この形式のrepairは**完成候補の絶対得点**を返します。基本形の改善量とは異なります。
また、部分破壊・再構築版の山登りは同点も採用します。
各形式の契約は[局所探索フォルダの説明](template/search/local-search/README.md)を確認してください。
近傍の適応選択・ILS・途中再生・前後DPも同じフォルダ内で選べます。

## モンテカルロ

[monte-carlo/](template/search/monte-carlo/README.md)には、全候補を同じ未来で比べるrolloutと、
木の枝に試行を配分するUCTを置いています。
候補・未知の未来・遷移や仮実行の評価を問題側に書きます。
実際に観測する前の情報を確定情報として使わないようにします。
完成例は[AHC015](examples/search/ahc015_common_rollout.cpp)です。

## 必要な形式だけ開く

設定付きの形式も、その手法のフォルダ内で見つかります。
詳しいAPIは[SEARCH_REFERENCE.md](SEARCH_REFERENCE.md)へ。
未知の未来を抽選しない[決定的な先読み](template/advanced/README.md)は別にしています。

この整理では既存ヘッダのAPIやpracticeの解法・既定設定は変えていません。
速く・強くなったという主張ではなく、選び方と編集場所の整理です。
