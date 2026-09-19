// GCC提出用の最適化。診断時は -DAHC_DISABLE_GCC_OPTIMIZE で無効化。
#if defined(__GNUC__) && !defined(__clang__) && !defined(AHC_DISABLE_GCC_OPTIMIZE)
#pragma GCC optimize("O3")
#endif

// 操作列を変更した位置からだけ再計算する焼きなまし。

#include <bits/stdc++.h>
#include "library/prefix-replay.hpp"
#include "library/time-based-simulated-annealing.hpp"
using namespace std;

// TODO: シミュレーション途中の状態を書く。行動1個で変わる量を全部含める。
struct SimulationState { long long value = 0; };
using Action = int;  // TODO: 1行動の型。比較可能なstructでもよい。

struct Problem {
    using Score = long long;  // TODO: 大きいほど良い評価値の型。
    struct State {
        vector<Action> actions; // TODO: 解として最適化する行動列。
        Score score = 0;
    };
    struct Move { vector<Action> actions; }; // TODO: 変更後の行動列。

    // cacheはStateへ入れないので、最良解の保存時にcache全体をコピーしない。
    PrefixReplay<SimulationState, Action> replay{SimulationState{}, 16};
    Score trial_score = 0;

    void advance(SimulationState&, const Action&) const {
        // TODO: 行動1個を反映する。乱数が必要ならActionかSimulationStateへ保存。
    }
    Score evaluate_end(const SimulationState& end) const {
        // TODO: 再生後の状態から「大きいほど良い」評価値を返す。
        return end.value;
    }
    optional<Move> propose_move(const State&, mt19937_64&, double) const {
        // TODO: 現在列をコピーして変更/交換/挿入/削除し、Moveを返す。
        // 作れない試行はnullopt。現在Stateは変更しない。
        return nullopt;
    }
    optional<Score> evaluate_move(const State& state, const Move& move,
                                  double /* threshold */) {
        // TODO: 評価関数はこれ1個。初期形では閾値を使わず最後まで再生する。
        // 途中打ち切りを追加するなら、最終改善量の上限<=thresholdと証明できた時だけnullopt。
        const auto step = [this](SimulationState& simulation, const Action& action) {
            advance(simulation, action);
        };
        // bestから再開した場合など、現在解とcacheが違ったらまず同期する。
        if (replay.actions() != state.actions) {
            replay.evaluate(state.actions, step);
            replay.commit();
        }
        trial_score = evaluate_end(replay.evaluate(move.actions, step));
        return trial_score - state.score; // TODO: 差分を返す。絶対評価ではない。
    }
    void apply_move(State& state, Move& move) {
        replay.commit(); // 採用時だけ仮cacheを確定。不採用時は何もしない。
        state.actions = std::move(move.actions);
        state.score = trial_score;
    }
    State make_initial_state() {
        SimulationState initial_simulation;
        // TODO: 入力を読み、再生開始時の盤面・資源などをinitial_simulationへ置く。
        // 遷移に使う固定の入力はProblemのメンバへ。入力読込後にcacheを初期化する。
        replay = PrefixReplay<SimulationState, Action>(initial_simulation, 16);
        State state;
        // TODO: 合法な初期行動列をstate.actionsへ入れる。
        replay.reserve(state.actions.size());
        const auto step = [this](SimulationState& simulation, const Action& action) {
            advance(simulation, action);
        };
        state.score = evaluate_end(replay.evaluate(state.actions, step));
        replay.commit();
        return state;
    }
};

int main() {
    Problem problem;
    auto initial = problem.make_initial_state();
    // TODO: 制限時間(ms)と、評価値のスケールに合う開始・終了温度を書く。
    TimeBasedAnnealingRunner<Problem> search(problem, initial, initial.score,
                                            1000, 100, 1, 123);
    search.run();
    const auto& answer = search.best_state();
    (void)answer;
    // TODO: answer.actionsを問題の出力形式に変換する。
}
