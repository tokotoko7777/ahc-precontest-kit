# 自作Stateの差分・巻き戻し検査

探索ライブラリ本体ではなく、本番で自分が書く`State`、`Move`、差分評価、
`apply/revert`を検査する小さな完成例です。

## 例

| ファイル | 検査するもの |
|---|---|
| [`route_swap_debug.cpp`](route_swap_debug.cpp) | 経路swapのscore差分、Zobrist風hash、位置cache、順序付き候補集合、apply→revert、複数Moveの逆順revert |
| [`grid_update_debug.cpp`](grid_update_debug.cpp) | 1マス色変更のscore、色数、接触辺、境界、hash、境界候補、色連結性、修復失敗時の復帰 |

どちらも差分式とは別に盤面・経路全体から計算するoracleを持ちます。比較は
`State`の`memcmp`ではなく、論理的に意味のあるfieldを順番に行います。そのため
失敗ログの`first_difference`が、最初に壊れたfieldを示します。

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -I. \
  examples/debug/route_swap_debug.cpp -o route_debug
./route_debug

g++ -std=c++17 -O2 -Wall -Wextra -pedantic -I. \
  examples/debug/grid_update_debug.cpp -o grid_debug
./grid_debug
```

## 自分のStateへ移す場所

1. `full_score`、`full_hash`、合法性判定を、差分式を使わずに書く。
2. `verify`で盤面、得点、hash、cache、候補集合を意味のある順に比較する。
3. 乱数seed、反復番号、その時点までのMove列を`AhcDebugStateCheck`へ渡す。
4. 1手のapply後、不採用手のrevert後、複数手を逆順に戻した後を検査する。
5. 修復が途中で失敗する経路や、同じ場所を複数回変更する経路も固定fixtureにする。

集合の反復順が次の候補生成や乱数との対応へ影響するなら、この例の
`candidate_order`のように順序も契約として比較します。順序に意味がなければ、
ソート済みvectorや集合へ直して要素だけを比較します。

浮動小数点値には`require_close`を使います。判定は絶対誤差と相対誤差の和で行い、
NaNは許容誤差を見る前に失敗させます。整数scoreは`require_equal`で完全一致させます。

## debug処理を提出版から外す

`AhcDebugStateCheck`は、呼ばれた時は`NDEBUG`でも検査します。提出版で検査コストを
消すには、呼び出しだけでなく、検査用のStateコピーと全再計算もまとめて囲みます。

```cpp
#ifndef NDEBUG
State before = state;
#endif

apply(state, move);

#ifndef NDEBUG
verify(state, seed, iteration, moves);
#endif
```

`assert(try_move())`のように、副作用のある処理を`assert`の中だけへ置かないで
ください。`-DNDEBUG`では処理そのものが消えてしまいます。

## 故障fixture

CIでは`route_swap_debug.cpp`を次のmacro付きでも個別にcompileし、「成功しては
いけない実行」として扱います。

- `AHC_DEBUG_INJECT_SCORE_BUG`: score更新漏れ
- `AHC_DEBUG_INJECT_HASH_BUG`: hash更新漏れ
- `AHC_DEBUG_INJECT_REVERT_BUG`: revert後のcache復元漏れ

各fixtureは`seed`、`iteration`、`moves`、`first_difference`を出して終了します。
同じbinaryを再実行すれば固定反復数で同じ不一致を再現できます。
