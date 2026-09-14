#define AHC032_END_COMBOS 4096
#define main ahc032_example_main
#include "../examples/search/ahc032_action_beam.cpp"
#undef main

int main() {
    mt19937 random(32);
    ostringstream input;
    input << "9 20 81\n";
    for (int i = 0; i < 81 + 20 * 9; ++i) input << random() % MODULO << ' ';
    istringstream stream(input.str());
    auto* original = cin.rdbuf(stream.rdbuf());
    ModStampProblem problem;
    problem.read_input();
    cin.rdbuf(original);
    // 0～4押しを全て保持し、5～7押しは各4096個。Actionの2-byte内に収まる。
    assert(problem.combinations.size() == 10626 + 3 * AHC032_END_COMBOS);
    array<int, 8> counts{};
    for (const auto& combo : problem.combinations) {
        ++counts[combo.count];
        array<uint32_t, 9> rebuilt{};
        for (int i = 0; i < combo.count; ++i) {
            assert(combo.stamp_ids[i] < 20);
            if (i) assert(combo.stamp_ids[i - 1] <= combo.stamp_ids[i]);
            for (int j = 0; j < 9; ++j) {
                rebuilt[j] = (rebuilt[j] + problem.stamps[combo.stamp_ids[i]][j]) % MODULO;
            }
        }
        assert(rebuilt == combo.add);
    }
    assert(counts[0] == 1 && counts[1] == 20 && counts[2] == 210);
    assert(counts[3] == 1540 && counts[4] == 8855);
    // 最後以外は最大3押し、最後のために7手を予約する。
    assert(problem.placements.back().max_actions == 7);
    assert(problem.placements[47].cumulative_limit == 74);
    for (int p = 0; p < 48; ++p) assert(problem.placements[p].max_actions <= 3);
    for (int limit = 0; limit <= 7; ++limit) {
        for (auto id : problem.allowed_actions[limit]) {
            assert(problem.combinations[id].count <= limit);
        }
    }
    ActionBeamRunner<ModStampProblem> beam(problem, problem.make_initial_state(),
                                         problem.initial_score(), 3);
    assert(beam.run(48) == 48);
    const auto old_limits = problem.placements.back().max_actions;
    problem.placements.back().max_actions = 4;
    long long old_best = -1;
    // 同じ親からの旧候補は必ず新候補内にもある。
    const auto parent = beam.best();
    for (auto action : problem.generate_actions(parent)) {
        old_best = max(old_best, problem.evaluate_action(parent, action));
    }
    problem.placements.back().max_actions = old_limits;
    assert(beam.run(1) == 1);
    assert(beam.best().finalized_score * RANK_SCALE >= old_best);
    problem.validate(beam.best());
}
