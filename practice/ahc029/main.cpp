#include <bits/stdc++.h>
using namespace std;

constexpr int INVESTMENT_MIN_REMAINING = 150;
constexpr int PAID_CARD_MIN_REMAINING = 0;
constexpr int INVESTMENT_RESERVE_START = 300;
constexpr long double INVESTMENT_MAX_MONEY_RATE = 0.80L;
constexpr long double COMPLETION_BONUS_RATE = 0.08L;
constexpr long double CANCEL_EFFICIENCY_LIMIT = 0.82L;
constexpr long double PRICE_WEIGHT = 1.90L;

struct Card {
    int type;
    long long power;
    long long price;
};

struct Project {
    long long work;
    long long value;
};

struct Action {
    int card_index = 0;
    int project_index = 0;
    long double score = -1e100L;
};

long double project_efficiency(const Project& project) {
    return static_cast<long double>(project.value) / project.work;
}

long double regular_work_value(
    long long power,
    const Project& project
) {
    const long long useful_work = min(power, project.work);
    long double value = useful_work * project_efficiency(project);
    if (power >= project.work) {
        // Receiving money and a fresh project immediately is slightly better
        // than leaving the same amount of estimated value unfinished.
        value += project.value * COMPLETION_BONUS_RATE;
    }
    return value;
}

long double hard_work_value(
    long long power,
    const vector<Project>& projects
) {
    long double value = 0;
    for (const Project& project : projects) {
        value += regular_work_value(power, project);
    }
    return value;
}

int worst_project(const vector<Project>& projects) {
    int worst = 0;
    for (int index = 1;
         index < static_cast<int>(projects.size());
         ++index) {
        if (project_efficiency(projects[index]) <
            project_efficiency(projects[worst])) {
            worst = index;
        }
    }
    return worst;
}

long double cancel_value(
    const Project& project,
    long long scale
) {
    // A newly generated project has value/work around 1 on average.  Only a
    // clearly inefficient project is worth abandoning.
    const long double shortage =
        CANCEL_EFFICIENCY_LIMIT - project_efficiency(project);
    if (shortage <= 0) return -scale * 2.0L;
    return shortage * min(project.work, 80LL * scale);
}

long double restructure_value(
    const vector<Project>& projects,
    long long scale
) {
    long double value = 0;
    int bad_count = 0;
    for (const Project& project : projects) {
        const long double part = cancel_value(project, scale);
        if (part > 0) {
            value += part;
            ++bad_count;
        } else {
            value += part * 0.20L;
        }
    }
    if (bad_count * 2 < static_cast<int>(projects.size())) {
        value -= scale * 5.0L;
    }
    return value;
}

Action choose_action(
    const vector<Card>& hand,
    const vector<Project>& projects,
    int turn,
    int total_turns,
    int investment_level
) {
#ifdef BASELINE_POLICY
    (void)hand;
    (void)projects;
    (void)turn;
    (void)total_turns;
    (void)investment_level;
    return Action{0, 0, 0};
#else
    Action best;
    const long long scale = 1LL << investment_level;
    const int remaining_turns = total_turns - turn;

    for (int card_index = 0;
         card_index < static_cast<int>(hand.size());
         ++card_index) {
        const Card& card = hand[card_index];

        if (card.type == 0) {
            for (int project_index = 0;
                 project_index < static_cast<int>(projects.size());
                 ++project_index) {
                const long double score = regular_work_value(
                    card.power,
                    projects[project_index]
                );
                if (score > best.score) {
                    best = {card_index, project_index, score};
                }
            }
        } else if (card.type == 1) {
            const long double score = hard_work_value(card.power, projects);
            if (score > best.score) best = {card_index, 0, score};
        } else if (card.type == 2) {
            const int project_index = worst_project(projects);
            const long double score = cancel_value(
                projects[project_index],
                scale
            );
            if (score > best.score) {
                best = {card_index, project_index, score};
            }
        } else if (card.type == 3) {
            const long double score = restructure_value(projects, scale);
            if (score > best.score) best = {card_index, 0, score};
        } else if (
            card.type == 4 && investment_level < 20 &&
            remaining_turns > INVESTMENT_MIN_REMAINING
        ) {
            // Investment changes every future project and offer.  When enough
            // turns remain, its compounding effect dominates one work action.
            return {card_index, 0, 1e90L};
        }
    }
    return best;
#endif
}

long double purchase_value(
    const Card& card,
    const vector<Project>& projects,
    int turn,
    int total_turns,
    int investment_level
) {
    const long long scale = 1LL << investment_level;
    const int remaining_turns = total_turns - turn - 1;
    long double use_value = 0;

    if (card.type == 0) {
        for (const Project& project : projects) {
            use_value = max(
                use_value,
                regular_work_value(card.power, project)
            );
        }
    } else if (card.type == 1) {
        use_value = hard_work_value(card.power, projects);
    } else if (card.type == 2) {
        use_value = cancel_value(projects[worst_project(projects)], scale);
    } else if (card.type == 3) {
        use_value = restructure_value(projects, scale);
    } else if (
        investment_level < 20 &&
        remaining_turns > INVESTMENT_MIN_REMAINING
    ) {
        // The main loop separately prioritizes affordable investments.  This
        // value is only used for completeness near that decision boundary.
        use_value = scale * remaining_turns * 4.0L;
    } else {
        use_value = -1e90L;
    }

    // A card near the end may never be used.  Gradually reduce what we are
    // willing to pay when there are fewer turns than cards in hand.
    const long double usability = min(1.0L, remaining_turns / 8.0L);
    return use_value * usability - card.price * PRICE_WEIGHT;
}

int choose_purchase(
    const vector<Card>& offers,
    const vector<Project>& projects,
    long long money,
    int turn,
    int total_turns,
    int investment_level
) {
#ifdef BASELINE_POLICY
    (void)offers;
    (void)projects;
    (void)money;
    (void)turn;
    (void)total_turns;
    (void)investment_level;
    return 0;
#else
    const int remaining_turns = total_turns - turn - 1;

    if (remaining_turns <= PAID_CARD_MIN_REMAINING) return 0;

    if (
        investment_level < 20 &&
        remaining_turns > INVESTMENT_MIN_REMAINING
    ) {
        int cheapest_investment = -1;
        for (int index = 0;
             index < static_cast<int>(offers.size());
             ++index) {
            if (offers[index].type != 4 || offers[index].price > money) {
                continue;
            }
            if (
                remaining_turns <= INVESTMENT_RESERVE_START &&
                offers[index].price >
                    money * INVESTMENT_MAX_MONEY_RATE
            ) {
                continue;
            }
            if (
                cheapest_investment == -1 ||
                offers[index].price < offers[cheapest_investment].price
            ) {
                cheapest_investment = index;
            }
        }
        if (cheapest_investment != -1) return cheapest_investment;
    }

    int best_index = 0;
    long double best_value = purchase_value(
        offers[0],
        projects,
        turn,
        total_turns,
        investment_level
    );
    for (int index = 1;
         index < static_cast<int>(offers.size());
         ++index) {
        if (offers[index].price > money) continue;
        const long double value = purchase_value(
            offers[index],
            projects,
            turn,
            total_turns,
            investment_level
        );
        if (value > best_value) {
            best_value = value;
            best_index = index;
        }
    }
    return best_index;
#endif
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int hand_size, project_count, offer_count, total_turns;
    cin >> hand_size >> project_count >> offer_count >> total_turns;

    vector<Card> hand(hand_size);
    for (Card& card : hand) {
        cin >> card.type >> card.power;
        card.price = 0;
    }

    vector<Project> projects(project_count);
    for (Project& project : projects) {
        cin >> project.work >> project.value;
    }

    long long money = 0;
    int investment_level = 0;
    for (int turn = 0; turn < total_turns; ++turn) {
        const Action action = choose_action(
            hand,
            projects,
            turn,
            total_turns,
            investment_level
        );
#ifdef LOCAL
        cerr << "turn=" << turn << " money=" << money
             << " level=" << investment_level
             << " use=" << action.card_index
             << " type=" << hand[action.card_index].type
             << " target=" << action.project_index << '\n';
#endif
        cout << action.card_index << ' ' << action.project_index << endl;

        if (hand[action.card_index].type == 4) ++investment_level;

        for (Project& project : projects) {
            cin >> project.work >> project.value;
        }
        cin >> money;

        vector<Card> offers(offer_count);
        for (Card& card : offers) {
            cin >> card.type >> card.power >> card.price;
        }

        const int bought = choose_purchase(
            offers,
            projects,
            money,
            turn,
            total_turns,
            investment_level
        );
#ifdef LOCAL
        cerr << "  after=" << money << " buy=" << bought
             << " type=" << offers[bought].type
             << " power=" << offers[bought].power
             << " price=" << offers[bought].price << '\n';
#endif
        cout << bought << endl;
        money -= offers[bought].price;
        hand[action.card_index] = offers[bought];
    }
    return 0;
}
