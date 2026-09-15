#define main ahc058_prefix_sa_example_main
#include "../examples/search/ahc058_prefix_sa.cpp"
#undef main

bool equal(const ProductionProblem::State& a, const ProductionProblem::State& b) {
    return a.count == b.count && a.power == b.power && a.turns == b.turns && a.apples == b.apples;
}

int main() {
    ProductionProblem game;
    for (int id = 0; id < 10; ++id) {
        game.production[id] = 1 + id;
        for (int level = 0; level < 4; ++level) game.cost[level][id] = (1 + id) * (1 + 30 * level);
    }
    ProductionProblem::State initial;
    for (auto& row : initial.count) row.fill(1);
    PurchaseSequenceProblem problem(game, initial);
    std::mt19937_64 engine(12345);
    // 閉形式の待機・待機時間の二分探索を、1ターンずつの更新と比較する。
    for (int test = 0; test < 400; ++test) {
        auto state = initial;
        state.turns = static_cast<int>(engine() % 501);
        state.apples = 1 + static_cast<int>(engine() % 100);
        for (int level = 0; level < 4; ++level) {
            for (int id = 0; id < 10; ++id) {
                state.count[level][id] = 1 + static_cast<int>(engine() % 30);
                state.power[level][id] = static_cast<int>(engine() % 6);
            }
        }
        const int turns = static_cast<int>(engine() % static_cast<uint64_t>(state.turns + 1));
        auto slow = state, fast = state;
        for (int i = 0; i < turns; ++i) game.advance(slow, -1);
        problem.wait_turns(fast, turns, problem.income_coefficients(fast));
        assert(equal(fast, slow));
        const int action = static_cast<int>(engine() % 40);
        fast = slow = state;
        const auto payment = game.upgrade_cost(slow, action);
        while (slow.turns > 0 && slow.apples < payment) game.advance(slow, -1);
        if (slow.turns > 0) game.advance(slow, action);
        problem.advance_purchase(fast, action);
        assert(equal(fast, slow));
    }
    // 生産できない場合、購入不能なまま最終ターンになる場合も確認する。
    auto unable = initial;
    problem.advance_purchase(unable, 39);
    assert(unable.turns == 0 && unable.apples == 1);

    auto state = problem.make_state({0, 10, 20, 30, 0, 10, 20, 30});
    for (int i = 0; i < 300; ++i) {
        auto move = problem.propose_move(state, engine, 0.5);
        if (!move) continue;
        const auto saved = state;
        const auto delta = problem.evaluate_move(state, *move, -numeric_limits<double>::infinity());
        auto slow = initial;
        for (int action : move->actions) problem.advance_purchase(slow, action);
        assert(delta && *delta == problem.score(slow) - state.score);
        assert(state.actions == saved.actions && state.score == saved.score);
        if (i % 2 == 0) {
            problem.apply_move(state, *move);
            assert(state.score == problem.score(slow));
        }
        if (i % 31 == 0) state = saved; // bestへ戻す場合のcache同期も通す。
    }
    i128 apples = 0;
    const auto output = problem.decode(state.actions, apples);
    assert(output.size() == 500);
    auto replay = initial;
    for (int action : output) game.advance(replay, action);
    assert(apples == replay.apples);
    assert(llround(100000.0 * log2(static_cast<double>(apples))) == state.score);
}
