#define main ahc058_example_main
#include "../examples/search/ahc058_deterministic_rollout.cpp"
#undef main

int main() {
    Problem problem;
    for (int id = 0; id < Problem::IDS; ++id) {
        problem.production[id] = 1 + id;
        for (int level = 0; level < Problem::LEVELS; ++level) {
            problem.cost[level][id] = 10 + id + level;
        }
    }
    // 閉形式を独立した1ターンずつの再生と比較。0手/最終手も含める。
    for (int turns = 0; turns <= 12; ++turns) {
        Problem::State state;
        state.turns = turns;
        state.apples = 100;
        for (int level = 0; level < Problem::LEVELS; ++level) {
            for (int id = 0; id < Problem::IDS; ++id) {
                state.count[level][id] = 1 + level + id;
                state.power[level][id] = (id + 2 * level) % 4;
            }
        }
        i128 estimate = state.apples;
        for (int id = 0; id < Problem::IDS; ++id) {
            estimate += problem.future_production(id, state);
        }
        auto replay = state;
        for (int step = 0; step < turns; ++step) {
            for (int id = 0; id < Problem::IDS; ++id) {
                replay.apples += replay.count[0][id] * replay.power[0][id] * problem.production[id];
            }
            for (int level = 1; level < Problem::LEVELS; ++level) {
                for (int id = 0; id < Problem::IDS; ++id) {
                    replay.count[level - 1][id] += replay.count[level][id] * replay.power[level][id];
                }
            }
        }
        assert(estimate == replay.apples);
        if (turns == 0) {
            assert(problem.value_with_one_upgrade(state) == state.apples);
            continue;
        }
        auto actions = problem.generate_actions(state);
        assert(actions.front() == -1);
        auto best = i128{-1};
        for (int action : actions) {
            auto next = state;
            problem.advance(next, action);
            while (next.turns > 0) problem.advance(next, -1);
            best = std::max(best, next.apples);
        }
        assert(problem.value_with_one_upgrade(state) == best);
        const auto saved = state;
        DeterministicRolloutRunner<Problem> runner(problem);
        const int chosen = runner.choose_action(state);
        assert(find(actions.begin(), actions.end(), chosen) != actions.end());
        assert(state.count == saved.count && state.power == saved.power);
        assert(state.apples == saved.apples && state.turns == saved.turns);
    }
    assert(Problem::add(Problem::VALUE_LIMIT - 1, 2) == Problem::VALUE_LIMIT);
    assert(Problem::multiply(Problem::VALUE_LIMIT - 1, 2) == Problem::VALUE_LIMIT);
}
