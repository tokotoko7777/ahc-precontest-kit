# AHC探索コアガイド

まずは、問題の状態をどう保持できるかと、1手で何世代進むかで選びます。

| 探索 | 向いている問題 | 状態管理 |
|---|---|---|
| `time-based-simulated-annealing.hpp` | 1つの解を局所変更し続けられる | 現在解を1つ保持 |
| `simple-beam-search.hpp` | 手数ごとに候補を残したい。`State`が小さい | 子の`State`をコピー |
| `action-beam-search.hpp` | Actionから次状態の順位を安く計算できる | 採用N件だけ`State`をコピー |
| `tree-beam-search.hpp` | 全行動で1世代ずつ進む。`State`のコピーが重い | `apply / revert`と履歴木 |
| `cost-tree-beam-search.hpp` | 行動ごとに1、2、3世代など進み幅が違う | `apply / revert`と到着世代別の履歴木 |
| `common-scenario-average.hpp` | 未知の未来を何本か試して今の1手を選ぶ | 全候補へ同じ未来sampleを使用 |

迷ったら、局所変更が自然なら焼きなまし、手順を1手ずつ作るなら
`SimpleBeamSearch`から始めます。状態コピーがボトルネックになったら、差分評価を
書ける場合は`ActionBeamSearch`、完全な逆操作も書ける場合は木上版へ移します。

各探索ヘッダでは`TODO:`を検索してください。そこに自分の`main.cpp`側へ書く型、
候補生成、評価、更新を記しています。ライブラリの探索エンジン本体を問題ごとに
書き換える必要はありません。実問題ベンチマークの`Problem`にも同じ`TODO:`を置き、
完成した実装では何が入るかをすぐ横で確認できるようにしています。

実際の空関数が並んだ状態から始める場合は
[`template/search/`](template/search/README.md)を使います。hppを読み込む行、
問題ごとの型と関数、探索の呼び出し、出力関数まで、最終的な`main.cpp`と同じ順番に
配置しています。

## 5本を単体で使った完全な例

| 探索コア | 問題例 | 完全な`main.cpp` |
|---|---|---|
| 時間焼きなまし | AHC006の配達経路 | [`ahc006_sa.cpp`](examples/search/ahc006_sa.cpp) |
| 通常ビーム | Introduction to Heuristics Contest A | [`intro_heuristics_simple_beam.cpp`](examples/search/intro_heuristics_simple_beam.cpp) |
| Action先行ビーム | Introduction to Heuristics Contest A | [`intro_heuristics_action_beam.cpp`](examples/search/intro_heuristics_action_beam.cpp) |
| 木上ビーム | AHC021の山崩し | [`ahc021_tree_beam.cpp`](examples/search/ahc021_tree_beam.cpp) |
| 世代飛ばし木上ビーム | 移動時間1〜3の締切付き宝集め | [`variable_cost_beam.cpp`](examples/search/variable_cost_beam.cpp) |
| Action先行ビーム＋行DP | AHC071の壁構築 | [`ahc071_action_beam.cpp`](examples/search/ahc071_action_beam.cpp) |

どれも探索ヘッダを1個だけ読み込む、入力から出力まで揃った例です。
提出時は使用したヘッダの中身を`main.cpp`の先頭へコピーし、`#include`の1行を
削除します。検証内容とコンパイル方法は
[`examples/search/README.md`](examples/search/README.md) にまとめています。

## 焼きなましの最小形

`improvement`は必ず「正なら良い変更」にします。最大化は
`new_score - current_score`、最小化は`current_cost - new_cost`です。

```cpp
TimeBasedSimulatedAnnealing sa(
    1900.0, 100.0, 1.0, 123, 64);

State current = make_initial_state();
long long current_score = calculate_score(current);
State best = current;
long long best_score = current_score;

while (!sa.is_over()) {
  Move move = make_random_move(current);
  long long improvement = calculate_delta(current, move);

  if (sa.accept(improvement)) {
    apply(current, move);
    current_score += improvement;
    if (best_score < current_score) {
      best_score = current_score;
      best = current;
    }
  }
}
```

最後の`64`は時計と温度を更新する間隔です。近傍1回が重いなら1〜8、
軽いなら64〜256を目安にします。焼きなましの最終状態は最良とは限らないため、
`best`は必ず別に保存します。

温度は「典型的な悪化幅を何%で受け入れたいか」から逆算できます。

```cpp
double start_temperature =
    TimeBasedSimulatedAnnealing::temperature_for_acceptance(20.0, 0.8);
double end_temperature =
    TimeBasedSimulatedAnnealing::temperature_for_acceptance(20.0, 0.01);

TimeBasedSimulatedAnnealing sa(
    1900.0, start_temperature, end_temperature, 123, 64);
```

既定は指数冷却です。`sa.use_linear_schedule()`で線形冷却、
`sa.set_cooling_power(2.0)`で高温の時間を長くできます。重い処理へ入る直前など、
間引きを無視して現在時刻を確認したい時は`sa.is_over_now()`を使います。

### 重い差分計算を採用閾値で途中終了する

`draw_acceptance_threshold()`は、その試行が採用されるために必要な最小改善量を
先に乱数で決めます。差分を部分和で計算できる時は、残りを最良に見積もっても
閾値を超えないと分かった時点で打ち切れます。

```cpp
double threshold = sa.draw_acceptance_threshold();
optional<long long> improvement =
    calculate_delta_until_threshold(state, move, threshold);
if (improvement && sa.accept_with_threshold(*improvement, threshold)) {
  apply(state, move);
}
```

`TimeBasedAnnealingRunner`ではProblemに
`evaluate_move_with_threshold(state, move, threshold)`を書き、
`run_with_threshold()`を呼びます。採用不能と証明できた時だけ`nullopt`、それ以外は
正確な改善量を返します。良化手を含め毎試行乱数を1個使うため通常の`accept()`とは
乱数列が変わりますが、各手の採用確率は同じです。

### 焼きなましで人が書く箇所を分ける

普段の編集箇所を1か所に集めたい時は`TimeBasedAnnealingRunner<Problem>`を使います。
次のコメント部分だけを問題に合わせます。

```cpp
struct Problem {
  // TODO: 【問題ごと】現在解1個と差分更新用cacheを書く。
  // 盤面、順列、長方形集合、現在の補助cacheなどを入れる。
  using State = MyState;

  // TODO: 【問題ごと】近傍1回分を小さく書く。
  // 変更位置、新しい値、差分更新に必要な情報だけを持つ。
  using Move = MyMove;

  // TODO: 【問題ごと】評価値の型を選ぶ。Runnerでは大きいほど良くする。
  using Score = long long;

  // TODO: 【問題ごと】近傍を1個作って返す。
  // 合法な近傍を作れない試行はnulloptでよい。
  // progressは開始時0、終了時1。探索前半・後半で近傍の大きさを変えられる。
  optional<Move> propose_move(
      const State& state, mt19937_64& rng, double progress) {
    return make_move(state, rng, progress);
  }

  // TODO: 【問題ごと】move適用後の改善量を差分計算する。
  // 正なら良化、負なら悪化。
  // stateを変更しない。不採用手をrevertせず捨てられるよう差分計算する。
  Score evaluate_move(const State& state, const Move& move) {
    return calculate_score_delta(state, move);
  }

  // TODO: 【問題ごと】採用済みmoveを反映し、全cacheを更新する。
  void apply_move(State& state, const Move& move) {
    apply(state, move);
  }
};

Problem problem;
MyState initial = make_initial_state();
long long initial_score = calculate_score(initial);
TimeBasedAnnealingRunner<Problem> runner(
    problem, initial, initial_score,
    1900.0,       // 時間制限[ms]
    1000.0, 1.0,  // 開始温度、終了温度
    123, 64);     // seed、時計を見る間隔
runner.run();
MyState answer = runner.best_state();
```

| 人が問題に合わせて書く | ライブラリが担当する |
|---|---|
| `State`、`Move`、初期解 | 時計と温度schedule |
| `propose_move` | 乱数engineと採否判定 |
| `evaluate_move`の差分 | 現在score、最良scoreの更新 |
| `apply_move` | 現在解、最良解、反復件数の保存 |

AHC001の長方形配置をこの境界で解き、同じ近傍の山登りと比較する実例は
[`ahc001_annealing_score_benchmark.cpp`](benchmarks/ahc001_annealing_score_benchmark.cpp)です。

## SimpleBeamSearchの最小形

`expand(state)`は子状態の`vector`、`rank_score(state)`はビーム内の
順位を返します。

```cpp
SimpleBeamSearch<State, long long> beam(initial_state, 200);
beam.reserve_candidates(200 * average_branch_count);

for (int turn = 0; turn < max_turn; ++turn) {
  if (!beam.step(expand, rank_score)) break;
}

State answer = beam.best();
```

`State`の中に盤面、得点、操作履歴を入れられるので、最初に試すのに
向いています。履歴や盤面が大きくなり、子ごとのコピーが重くなったら
`TreeBeamSearch`へ移します。

子を並べる一時`vector`を作りたくない場合は、生成した状態を直接渡せます。

```cpp
beam.step_each(
    [&](const State& parent, auto&& emit) {
      for (Move move : make_moves(parent)) {
        State child = parent;
        apply(child, move);
        emit(std::move(child));
      }
    },
    rank_score);
```

`last_generated_count()`、`last_unique_count()`、`last_kept_count()`で、直近層の
生成数・key重複除去後の数・採用数を確認できます。

## ActionBeamSearchの最小形

全候補について`State child = parent`を行わず、Actionと順位だけを先に計算します。
`apply`は選ばれた最大`beam_width`件にしか呼ばれません。

問題に合わせて書くものは、次の`Problem` structへ集めます。

```cpp
struct Problem {
  // TODO: 【問題ごと】探索途中の解1個をStateへ書く。
  // 盤面、現在ターン、使用回数、得点、操作履歴などを入れる。
  // 入力のような全候補で共通の読み取り専用データはProblem本体へ置く。
  using State = MyState;

  // TODO: 【問題ごと】1手を表す軽いActionを書く。
  // 次の盤面全体ではなく、番号・場所・向きなどだけを入れる。
  using Action = MyMove;

  // TODO: 【問題ごと】候補順位Scoreの型を選ぶ。既定では大きい値ほど良い。
  using Score = long long;

  // TODO: 【問題ごと】stateから合法なActionを全て返す。
  // vector/arrayを値で返しても、Problemが持つコンテナをconst参照で返してもよい。
  vector<Action> generate_actions(const State& state) {
    return make_moves(state);
  }

  // TODO: 【問題ごと】action適用後の子Stateの順位値そのものを返す。
  // 全候補に呼ばれるのでstateを変更せず、できれば差分計算で軽くする。
  Score evaluate_action(const State& state, const Action& action) {
    return state.rank_score + calculate_rank_delta(state, action);
  }

  // TODO: 【問題ごと】採用されたactionをコピー済みstateへ反映する。
  // 盤面、得点、ターン、hash、使用回数、答えの履歴を漏れなく更新する。
  void apply_action(State& state, Action& action) {
    apply(state, action);
  }
};

Problem problem;
// initial_rank_scoreはinitial_stateを候補として比較する時の順位値。
// 小さいScoreを良いものとして残す場合は第5引数へfalseを渡す。
ActionBeamRunner<Problem> beam(
    problem, initial_state, initial_rank_score, 200);
beam.run(max_turn);
// best()は最後の世代に残った中で最も順位値が良いState。
MyState answer = beam.best();
```

`evaluate_action`が返すものを迷ったら、まず「Actionを適用した後のState全体を
採点する関数」をそのまま書けば正しく動きます。動作確認後、その計算を
`現在の順位値 + このActionによる変化量`へ置き換えると高速になります。
`evaluate_action`が返した順位値と、`apply_action`後のStateが表す局面が食い違うと、
ライブラリは意図と違う候補を残すため、この2関数は必ず対にして考えます。

境界は次の通りです。

| 人が問題に合わせて書く | ライブラリが担当する |
|---|---|
| 入力・出力、`State`、`Action` | 候補bufferと上位N件選抜 |
| `generate_actions` | `2N → N`のcutoff |
| `evaluate_action` | 親Stateの管理と採用N件だけのコピー |
| `apply_action` | ターンループ、幅変更、件数統計 |
| 必要なら`make_key`・`make_bucket` | 重複除去・bucket上限の適用 |

通常は`Problem`と`main`だけを書き、`ActionBeamSearch`や`ActionBeamRunner`本体は
変更しません。入力から出力まで分離した実例は
[`intro_heuristics_action_beam.cpp`](examples/search/intro_heuristics_action_beam.cpp)です。
各項目のさらに詳しいコメントは、この実例と
[`action-beam-search.hpp`](library/action-beam-search.hpp)先頭の雛形に入っています。
実問題で幅による解の質を比較する時は`make benchmark-search`で、AHC032
「Mod Stamp」相当の固定ケースに対する最終得点を測れます。

内部ではAction候補が`2 * width`件たまるたび上位`width`件へ縮め、以後は既知の
境界以下を保存しません。これは近似選抜ではなく、同点の生成順も含めて厳密です。
`last_buffered_peak_count()`で、通常の`step`が同時に持った候補数を確認できます。
生成順に候補が改善し続ける場合は中間選抜が増えるため、
`set_batched_selection(false)`の「最後に`nth_element`を1回」も同じ入力で測れます。
結果は同じなので、速い方を選びます。

順位計算自体が重い時は、Problemに
`evaluate_action_with_threshold(state, action, threshold)`を書き、
`step_with_threshold()`または`run_with_threshold()`を使います。戻り値は
`optional<Score>`です。最初のN件がそろうまで`threshold`は`nullptr`、その後は現在の
採用境界を指します。候補が境界を厳密に超えないと証明できた時だけ`nullopt`を返し、
不明なら最後まで計算して正確なScoreを返します。これは近似枝刈りではないため、
正しく上界・下界を書けば通常の`step()`と同じ上位N件になります。

現在は動的閾値と`make_key`の重複除去を同時には使いません。重複除去が重要なら
`run_with_key()`を選び、AHC071例のように完成済み解の費用を固定閾値として
`generate_actions`側でも枝刈りします。

同一状態を消す時は、次状態を作らず計算できるhashを
Problemの`make_key`に書き、`step_with_key()`または`run_with_key()`を使います。
似た候補ばかりになる時は`make_bucket`を書き、粗い特徴ごとの上限を
`step_with_bucket_limit(max_per_bucket)`で設定できます。keyやbucketを
使う経路は正しさのため全Action候補を一度保存するので、候補数も測って選びます。

早期terminalを全候補から拾う時は`step_and_observe`を使います。ただしStateを
全候補ぶん作らない設計なので、observerが受け取るのは
`(parent_rank, parent, action, rank_score)`です。親の履歴へactionを1個足せば
候補の手順を保存できます。

## 世代が飛ばないTreeBeamSearch

全行動がちょうど1世代進む時に使います。`State`は1個だけ持ち、
生き残った履歴木をDFSしながら状態を変更・復元します。

```cpp
TreeBeamSearch<State, Move, long long> beam(
    initial_state, rank_score(initial_state), 200);

for (int turn = 0; turn < max_turn; ++turn) {
  if (!beam.step(expand, apply, revert, rank_score)) break;
}

vector<Move> answer = beam.restore();
```

`apply(state, move)`で変えたスコア、hash、個数表、集合などは、
`revert(state, move)`で全て元へ戻します。`Move`には、上書き前の値など
復元に必要な情報も入れます。

問題依存部分を1か所へ集める場合は`TreeBeamRunner<Problem>`を使います。

```cpp
struct Problem {
  // TODO: 【問題ごと】全状態、1手+undo、候補順位の型を書く。
  using State = MyState;       // DFS中に1個だけ持つ全状態。
  using Move = MyMove;         // 1手とundoに必要な情報。
  using Score = long long;     // 候補順位。大きいほど良い。

  // TODO: 【問題ごと】このstateから試す合法手を返す。
  vector<Move> generate_moves(const State& state) {
    return make_moves(state);
  }

  // TODO: 【問題ごと】盤面、score、hashなどを1手分だけ差分更新する。
  // 復元に必要な旧値が生成時に不明ならmoveへここで書き込んでよい。
  void apply_move(State& state, Move& move) {
    apply(state, move);
  }

  // TODO: 【問題ごと】apply_move直前と完全に同じ状態へ戻す。
  void revert_move(State& state, const Move& move) {
    revert(state, move);
  }

  // TODO: 【問題ごと】現在stateの順位値そのものを返す。差分ではない。
  Score evaluate(const State& state) {
    return state.rank_score;
  }

  // TODO: 【必要な問題だけ】同じ未来を持つ局面を同じkeyにする。
  uint64_t make_key(const State& state) {
    return state.hash;
  }
};

Problem problem;
TreeBeamRunner<Problem> beam(
    problem, initial_state, initial_rank_score, 200);
beam.run_with_key(max_turn);
vector<MyMove> answer = beam.restore();
```

| 人が問題に合わせて書く | ライブラリが担当する |
|---|---|
| `State`、`Move`、順位値 | 生存履歴木とDFS巡回 |
| `generate_moves` | 候補bufferと上位N件選抜 |
| `apply_move` / `revert_move` | State 1個の使い回し |
| 必要なら`make_key` | 世代ごとの重複除去 |

AHC021のピラミッドをこの境界で解く実例は
[`ahc021_tree_beam_score_benchmark.cpp`](benchmarks/ahc021_tree_beam_score_benchmark.cpp)です。

## 共通シナリオMonte Carlo

現在の1手を、複数の未知の未来で最後まで試して選ぶ方法です。Actionごとに別の
未来を引くと、Action差と乱数の当たり外れが混ざります。全Actionを同じScenario
集合で評価する`CommonScenarioRolloutRunner<Problem>`を使います。

```cpp
struct Problem {
  // TODO: 【問題ごと】現在情報、今の1手、未知の未来、評価値の型を書く。
  using State = MyState;          // 現在までに確定している情報。
  using Action = MyAction;        // 今選ぶ1手。
  using Scenario = MyScenario;    // 未知の未来1本。
  using Score = long long;        // 1 rolloutの最終評価値。

  // TODO: 【問題ごと】今選べるActionを全て返す。
  vector<Action> generate_actions(const State& state) {
    return legal_actions(state);
  }

  // TODO: 【問題ごと】未知情報だけを1本sampleする。
  Scenario generate_scenario(const State& state, mt19937_64& rng) {
    return sample_unknown_future(state, rng);
  }

  // TODO: 【問題ごと】最初のaction後を終端まで進めて評価する。
  // その後は問題固有のルール方策などで終端まで進め、
  // 最終評価値を返す。元のstateは変更しない。
  Score evaluate_action(
      const State& state, const Action& action, const Scenario& scenario) {
    State simulation = state;
    apply(simulation, action);
    play_to_end_with_rule_policy(simulation, scenario);
    return official_score(simulation);
  }
};

Problem problem;
CommonScenarioRolloutRunner<Problem> rollout(problem, 123);
rollout.reserve(max_action_count, sample_count);
MyAction action = rollout.choose_action(state, sample_count);
apply_real_state(state, action);  // 実状態の更新は呼び出し側。
```

| 人が問題に合わせて書く | ライブラリが担当する |
|---|---|
| `State`、`Action`、`Scenario` | 全Actionで共通のScenario生成 |
| 未知情報のsample方法 | 各Actionの平均計算 |
| rollout中の方策と終端評価 | 最大・最小の最良Action選択 |
| 選択後の実状態更新 | sample・平均値のbuffer再利用 |

AHC015の飴配置を終端までrolloutする実例は
[`ahc015_monte_carlo_score_benchmark.cpp`](benchmarks/ahc015_monte_carlo_score_benchmark.cpp)です。
sample数だけを増やしても、未来の自分の行動が弱ければ評価も弱いままです。
未知情報のsample、未来のルール方策、1 rolloutの軽さを問題ごとに設計します。

## 世代が飛ぶCostTreeBeamSearch

1行動の消費手数が異なる場合は、同じ到着世代の候補だけを比較します。
`get_advance(move)`は正の整数を返します。

```cpp
CostTreeBeamSearch<State, Move, long long> beam(
    initial_state, rank_score(initial_state), 200, max_generation);

while (beam.step(
    expand, apply, revert, rank_score,
    [](const Move& move) { return move.advance; })) {
}

vector<Move> answer = beam.restore();
```

`advance <= 0`は不正です。`max_generation`を超える行動は自動で候補から外れます。
`step()`は、候補が存在する最小の到着世代へ進みます。
時間が減ったら`beam.set_beam_width(smaller_width)`で、現在層と予約済みの
未来層をまとめて縮められます。後から幅を広げても、既に落とした候補は戻りません。

## 順位評価と最終目的を分ける

`evaluate`または`rank_score`は、限られた幅に残す候補を決めるための値です。
現在得点に、残り手数の余力、未達成罰則、将来価値などを加えても構いません。

一方、提出解を比べる時は必ず問題本来の`official_score`を使います。
木上ビームの`best_score()`は順位評価値であり、提出得点とは限りません。

```cpp
long long rank_score(const State& state) {
  return state.current_score + estimate_future_gain(state);
}

long long official_score(const State& state) {
  return calculate_problem_score(state);
}
```

## terminalを消さない

一部の状態だけ早く終端に着く問題では、生成した終端候補が順位幅から落ちると、
次の層には一度も現れません。`step`の前後で現在層を見るだけでは不十分です。
全生成候補を選抜前に受け取るobserverで、本来の得点と答えを保存します。

```cpp
// SimpleBeamSearch
beam.step_and_observe(
    expand, rank_score,
    [&](const State& child, const long long&) {
      if (is_terminal(child)) {
        save_if_better(official_score(child), make_answer(child));
      }
    });
```

全経路が必ず同じ最終世代へ着くなら、探索後の`best()`または`restore()`で
十分です。早く終わるterminalがある時は、現在のビームだけを見て答えにしては
いけません。

木上版のobserverは、親rankと生成した行動も受け取ります。

```cpp
beam.step_and_observe(
    expand, apply, revert, rank_score,
    [&](int parent_rank, const Move& move,
        const State& child, const long long&) {
      if (!is_terminal(child)) return;
      vector<Move> answer = beam.restore_candidate(parent_rank, move);
      save_if_better(official_score(child), answer);
    });
```

`CostTreeBeamSearch`では末尾に到着世代も渡されます。

```cpp
beam.step_and_observe(
    expand, apply, revert, rank_score, get_advance,
    [&](int parent_rank, const Move& move,
        const State& child, const long long&, int next_generation) {
      if (is_terminal(child, next_generation)) {
        save_if_better(official_score(child),
                       beam.restore_candidate(parent_rank, move));
      }
    });
```

key版は`step_with_key_and_observe`、Simpleの直接生成版は
`step_each_and_observe`を使います。observerは全候補へ呼ばれるため、内部では
terminal判定と必要な保存だけを行い、重い処理は避けます。

## keyで重複を消す

同じ世代の同じ状態へ多くの経路から到達するなら、`step_with_key`を使います。

```cpp
// SimpleBeamSearch
beam.step_with_key(expand, rank_score, make_key);

// TreeBeamSearch
beam.step_with_key(expand, apply, revert, rank_score, make_key);

// CostTreeBeamSearch
beam.step_with_key(
    expand, apply, revert, rank_score, get_advance, make_key);
```

keyは「今後の行動候補と最終得点の比較に必要な状態」を区別します。
盤面が同じでも、残り資源や将来の選択肢が違うなら別keyです。世代はライブラリ側で
分けるため、通常はkeyに入れる必要がありません。

64bit Zobrist hashは高速ですが、衝突可能性は0ではありません。正確性を必ず保ちたい時は
状態そのものを表す値をkeyにします。重複が少ない場合はhash表の費用が増えるだけなので、
通常の`step`を使います。

## apply / revertの確認

`TreeBeamSearch`と`CostTreeBeamSearch`で最も重要な条件です。

```cpp
State before = state;
apply(state, move);
revert(state, move);
assert(state == before);
```

盤面だけでなく、得点、hash、統計量、候補集合も確認します。`apply`内で
乱数を振ると復元しにくいため、ランダムな選択結果は先に`Move`へ入れます。

## 速度の最終チェック

- 得点とhashはできるだけ差分更新する。
- `rank_score`は全候補に呼ばれるため、安い評価を先に使う。
- `expand`の`vector`と候補bufferは容量を再利用する。
- `SimpleBeamSearch::step_each`なら、子を並べる一時コンテナ自体を省ける。
- `SimpleBeamSearch::reserve_candidates`、`TreeBeamSearch::reserve_nodes`、
  `TreeBeamSearch::reserve_candidates`、`CostTreeBeamSearch`の同名関数で
  上限が分かる領域を予約する。
- 分岐数の上限が小さいなら、`expand`から`vector`ではなく
  `FixedVector<State, N>`または`FixedVector<Move, N>`を返して層ごとの
  heap確保を避ける。
- 上限を固定できない場合は、外に`vector`を1個用意して`reserve`し、
  `expand`からその非const参照を返すと容量を層間で再利用できる。
- `expand`が返した非constコンテナの要素は探索コアがmoveして消費する。
  再利用する場合は、次の`expand`呼び出しで`clear`して中身を作り直す。
  読み取り専用の固定行動表は`const vector<Move>&`で返せばコピーして使える。
- `State`と`Move`を小さく保ち、文字列や全履歴を候補ごとに持たない。
- key重複除去は、同じ世代の重複が十分多い時だけ使う。
- 時間に応じて幅を変える場合は`set_width`または`set_beam_width`を使う。
- ビーム幅だけでなく、1秒当たりの候補評価数と複数seedの最終得点で比べる。
