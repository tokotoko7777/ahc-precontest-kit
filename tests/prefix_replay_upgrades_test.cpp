#include <cassert>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>
#include "library/prefix-replay.hpp"

struct ReplayState {
    int turn;
    long long sum;
    std::uint64_t hash;
    explicit ReplayState(int start) : turn(start), sum(0), hash(42) {}
    bool operator==(const ReplayState& other) const {
        return turn == other.turn && sum == other.sum && hash == other.hash;
    }
};

void advance(ReplayState& state, int action) {
    state.sum += static_cast<long long>(++state.turn) * action;
    state.hash = state.hash * 131 + static_cast<std::uint64_t>(action + 100);
}

ReplayState full(const std::vector<int>& actions) {
    ReplayState state(3);
    for (int action : actions) advance(state, action);
    return state;
}

int main() {
    std::mt19937 random(20260915);
    for (int interval : {1, 2, 8, 16, 128}) {
        PrefixReplay<ReplayState, int> cache(ReplayState(3), interval);
        cache.reserve(100);
        std::vector<int> current;
        assert(cache.current_end() == full(current));
        for (int iteration = 0; iteration < 600; ++iteration) {
            auto trial = current;
            if (iteration % 4 == 0) trial.resize(random() % 90);
            else if (!trial.empty()) trial.resize(random() % (trial.size() + 1));
            if (iteration % 7 != 0) {
                const int count = 1 + static_cast<int>(random() % 10);
                for (int i = 0; i < count; ++i) trial.push_back(static_cast<int>(random() % 21) - 10);
            }
            if (!trial.empty()) trial[random() % trial.size()] = static_cast<int>(random() % 21) - 10;
            std::size_t common = 0;
            while (common < std::min(current.size(), trial.size()) && current[common] == trial[common]) ++common;
            const std::size_t stride = static_cast<std::size_t>(interval);
            assert(cache.evaluate(trial, advance) == full(trial));
            assert(cache.last_replayed_actions() == trial.size() - common / stride * stride);
            assert(cache.current_end() == full(current));
            assert(cache.actions() == current);
            if (iteration % 3 == 0) {
                cache.commit();
                current = trial;
            } else if (iteration % 3 == 1) cache.discard();
            // もう1種類はdiscardせず、次のevaluateで置き換える。
            assert(cache.current_end() == full(current));
            assert(cache.checkpoint_count() == current.size() / stride + 1);
        }
        cache.evaluate({}, advance);
        cache.commit();
        assert(cache.current_end() == full({}));
        assert(cache.checkpoint_count() == 1);
        cache.evaluate({1, 2, 3}, advance);
        cache.commit();
        bool threw = false;
        try {
            cache.evaluate({4, 5}, [](ReplayState&, int) { throw std::runtime_error("test"); });
        } catch (const std::runtime_error&) { threw = true; }
        assert(threw && cache.current_end() == full({1, 2, 3}));
        threw = false;
        try { cache.commit(); } catch (const std::logic_error&) { threw = true; }
        assert(threw);
        assert(cache.evaluate({1, 2}, advance) == full({1, 2}));
        cache.commit();
    }
    bool threw = false;
    try { PrefixReplay<ReplayState, int> invalid(ReplayState(0), 0); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}
