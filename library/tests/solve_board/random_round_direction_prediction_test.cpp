/// @file random_round_direction_prediction_test.cpp
/// @brief Randomized round/declarer-direction prediction consistency test.
/// @details Generates random deals, tries different opening directions, and
/// validates DDS AnalysePlayPBN predictions at selected round checkpoints from
/// "after first card" to "only last trick remaining".

#include <algorithm>
#include <array>
#include <cstring>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

#include <gtest/gtest.h>

#include <api/dds.h>

namespace {

constexpr int kStrainNT = 4;

using Card = std::pair<int, int>;                // (suit 0..3, rank 2..14)
using Hands = std::array<std::vector<Card>, 4>;  // N,E,S,W

const char kSuitChar[4] = {'S', 'H', 'D', 'C'};

auto rank_char(int r) -> char
{
    static const char* kRanks = "23456789TJQKA";
    return kRanks[r - 2];
}

auto suit_index(char c) -> int
{
    switch (c) {
        case 'S': return 0;
        case 'H': return 1;
        case 'D': return 2;
        default:  return 3;
    }
}

auto rank_value(char c) -> int
{
    switch (c) {
        case 'A': return 14;
        case 'K': return 13;
        case 'Q': return 12;
        case 'J': return 11;
        case 'T': return 10;
        default:  return c - '0';
    }
}

auto hands_to_pbn(const Hands& hands) -> std::string
{
    std::string out = "N:";
    for (int h = 0; h < 4; ++h) {
        if (h) out += ' ';
        std::array<std::vector<int>, 4> by_suit;
        for (const auto& [s, r] : hands[static_cast<size_t>(h)]) {
            by_suit[static_cast<size_t>(s)].push_back(r);
        }
        for (int s = 0; s < 4; ++s) {
            if (s) out += '.';
            auto& v = by_suit[static_cast<size_t>(s)];
            std::sort(v.rbegin(), v.rend());
            for (int r : v) out += rank_char(r);
        }
    }
    return out;
}

auto trick_winner(const std::array<Card, 4>& cards, int trump, int leader)
    -> int
{
    const int lead_suit = cards[0].first;
    int best = 0;
    for (int i = 1; i < 4; ++i) {
        const auto [si, ri] = cards[static_cast<size_t>(i)];
        const auto [sb, rb] = cards[static_cast<size_t>(best)];
        const bool i_trump = (trump != kStrainNT && si == trump);
        const bool b_trump = (trump != kStrainNT && sb == trump);
        if (i_trump && !b_trump) {
            best = i;
        } else if (i_trump && b_trump) {
            if (ri > rb) best = i;
        } else if (!i_trump && !b_trump) {
            if (si == lead_suit && (sb != lead_suit || ri > rb)) best = i;
        }
    }
    return (leader + best) % 4;
}

auto make_deal(std::mt19937& rng) -> Hands
{
    std::vector<Card> deck;
    for (int s = 0; s < 4; ++s) {
        for (int r = 2; r <= 14; ++r) {
            deck.emplace_back(s, r);
        }
    }
    std::shuffle(deck.begin(), deck.end(), rng);

    Hands hands;
    for (int h = 0; h < 4; ++h) {
        for (int i = 0; i < 13; ++i) {
            hands[static_cast<size_t>(h)].push_back(
                deck[static_cast<size_t>(h * 13 + i)]);
        }
    }
    return hands;
}

auto make_play(const Hands& deal, int leader, int trump, std::mt19937& rng)
    -> std::vector<Card>
{
    Hands hands = deal;
    std::vector<Card> out;
    std::vector<Card> cur;
    int cur_leader = leader;
    int player = leader;

    for (int ply = 0; ply < 52; ++ply) {
        auto& hand = hands[static_cast<size_t>(player)];
        Card card;
        if (cur.empty()) {
            card = hand[rng() % hand.size()];
        } else {
            const int lead_suit = cur[0].first;
            std::vector<Card> follow;
            for (const auto& c : hand) {
                if (c.first == lead_suit) follow.push_back(c);
            }
            const auto& pool = follow.empty() ? hand : follow;
            card = pool[rng() % pool.size()];
        }

        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        out.push_back(card);
        cur.push_back(card);

        player = (player + 1) % 4;
        if (cur.size() == 4) {
            std::array<Card, 4> t{cur[0], cur[1], cur[2], cur[3]};
            cur_leader = trick_winner(t, trump, cur_leader);
            player = cur_leader;
            cur.clear();
        }
    }

    return out;
}

auto solve_max(int trump, int leader, const std::vector<Card>& cur,
               const Hands& hands) -> int
{
    DealPBN dl;
    std::memset(&dl, 0, sizeof(dl));
    dl.trump = trump;
    dl.first = leader;
    for (size_t i = 0; i < cur.size() && i < 3; ++i) {
        dl.currentTrickSuit[i] = cur[i].first;
        dl.currentTrickRank[i] = cur[i].second;
    }

    const std::string pbn = hands_to_pbn(hands);
    std::strncpy(dl.remainCards, pbn.c_str(), sizeof(dl.remainCards) - 1);

    FutureTricks fut;
    const int rc = SolveBoardPBN(dl, -1, 1, 1, &fut, 0);
    EXPECT_EQ(RETURN_NO_FAULT, rc);
    return fut.score[0];
}

struct MatchStats {
    int compared = 0;
    int matched = 0;
};

auto evaluate_selected_rounds(const Hands& initial_hands,
                              int trump,
                              int opening_leader,
                              const std::vector<Card>& play,
                              const std::set<int>& checkpoints) -> MatchStats
{
    DealPBN dl;
    std::memset(&dl, 0, sizeof(dl));
    dl.trump = trump;
    dl.first = opening_leader;

    const std::string deal_pbn = hands_to_pbn(initial_hands);
    std::strncpy(dl.remainCards, deal_pbn.c_str(), sizeof(dl.remainCards) - 1);

    PlayTracePBN trace;
    std::memset(&trace, 0, sizeof(trace));
    trace.number = static_cast<int>(play.size());

    std::string play_str;
    for (const auto& [s, r] : play) {
        play_str += kSuitChar[s];
        play_str += rank_char(r);
    }
    std::strncpy(trace.cards, play_str.c_str(), sizeof(trace.cards) - 1);

    SolvedPlay solved;
    std::memset(&solved, 0, sizeof(solved));
    EXPECT_EQ(RETURN_NO_FAULT, AnalysePlayPBN(dl, trace, &solved, 0));

    const int decl_parity = 1 - (opening_leader % 2);

    Hands cur_hands = initial_hands;
    std::vector<Card> cur;
    int leader = opening_leader;
    int completed = 0;
    int decl_won = 0;

    MatchStats stats;

    for (size_t k = 0; k < play.size(); ++k) {
        const int cards_played = static_cast<int>(k);
        if (checkpoints.find(cards_played) != checkpoints.end()) {
            const int remaining = 13 - completed;
            const int player_to_act = (leader + static_cast<int>(cur.size())) % 4;
            const int sb = solve_max(trump, leader, cur, cur_hands);
            const int decl_remaining =
                (player_to_act % 2 == decl_parity) ? sb : (remaining - sb);
            const int expected = decl_won + decl_remaining;

            stats.compared++;
            if (k < static_cast<size_t>(solved.number) && expected == solved.tricks[k]) {
                stats.matched++;
            }
        }

        const Card card = play[k];
        const int player = (leader + static_cast<int>(cur.size())) % 4;
        auto& ph = cur_hands[static_cast<size_t>(player)];
        ph.erase(std::remove(ph.begin(), ph.end(), card), ph.end());
        cur.push_back(card);

        if (cur.size() == 4) {
            std::array<Card, 4> t{cur[0], cur[1], cur[2], cur[3]};
            const int w = trick_winner(t, trump, leader);
            if (w % 2 == decl_parity) ++decl_won;
            ++completed;
            leader = w;
            cur.clear();
        }
    }

    return stats;
}

class RandomRoundDirectionPredictionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        SetMaxThreads(0);
    }
};

TEST_F(RandomRoundDirectionPredictionTest, RandomDealsAcrossRoundsAndDirections)
{
    std::mt19937 rng(20260531u);
    // Scale test volume by 10x from the initial 16-deal baseline.
    constexpr int kDeals = 160;

    // Checkpoints: after first card is exposed, then each full trick boundary,
    // and up to the point where only the last trick remains.
    std::set<int> checkpoints;
    checkpoints.insert(1);
    for (int p = 4; p <= 48; p += 4) {
        checkpoints.insert(p);
    }

    MatchStats total;

    for (int d = 0; d < kDeals; ++d) {
        const Hands hands = make_deal(rng);

        for (int leader = 0; leader < 4; ++leader) {
            const int trump = static_cast<int>(rng() % 5);
            const std::vector<Card> play = make_play(hands, leader, trump, rng);
            const MatchStats one =
                evaluate_selected_rounds(hands, trump, leader, play, checkpoints);

            total.compared += one.compared;
            total.matched += one.matched;
        }
    }

    ASSERT_GT(total.compared, 0);
    ASSERT_GE(total.compared, 1000)
        << "Expected at least 1000 comparisons after scaling test volume";
    std::cout << "prediction_match_stats: matched=" << total.matched
              << " compared=" << total.compared
              << " rate="
              << static_cast<double>(total.matched) / static_cast<double>(total.compared)
              << std::endl;
    EXPECT_EQ(total.matched, total.compared)
        << "DDS prediction mismatch count=" << (total.compared - total.matched)
        << ", match rate="
        << static_cast<double>(total.matched) / static_cast<double>(total.compared);
}

} // namespace
