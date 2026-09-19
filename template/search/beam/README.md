# ビームのバリエーション

各方式を個別のcppとして置いています。使いたい1つをmain.cppへコピーしてください。

| 方式 | ファイル | 選ぶ目安 |
|---|---|---|
| 通常 | [simple.cpp](simple.cpp) | まずはこちら。子のStateをコピーして作る |
| 差分評価 | [action.cpp](action.cpp) | Actionから子の順位を評価し、採用分だけStateへ反映する |
| 差分評価＋枝刈り設定 | [action-options.cpp](action-options.cpp) | 候補の評価・生成を安全な境界で打ち切りたい |
| 木上 | [tree.cpp](tree.cpp) | 状態コピーが重い。apply/revertで盤面1個を動かす |
| 木上・世代飛ばし | [variable-cost-tree.cpp](variable-cost-tree.cpp) | 1手で進む世代数が異なる |
| chokudai | [chokudai.cpp](chokudai.cpp) | 深さごとの候補を残して繰り返し探索する |
| 多点スタート | [multi-start.cpp](multi-start.cpp) | 共通の締切内で初期条件を変えて複数回試す |

基本は候補生成・評価・更新を書く形です。木上版では逆操作revertも書きます。
evaluateは**子Stateの順位値そのもの**を返します。局所探索の改善量とは異なります。
同じ決定的探索を繰り返すだけでは多様化しないので、多点スタートでは
初期解・候補順などを問題側で変えます。

探索順位と正式得点が違う場合の完成解選択などは[APIリファレンス](../../../SEARCH_REFERENCE.md)へ。
完成例は[AHC032の差分評価](../../../examples/search/ahc032_action_beam.cpp)、
[AHC021の木上版](../../../examples/search/ahc021_tree_beam.cpp)、
[AHC038の世代飛ばし](../../../examples/search/ahc038_variable_cost_beam.cpp)です。

[手法一覧へ戻る](../README.md)
