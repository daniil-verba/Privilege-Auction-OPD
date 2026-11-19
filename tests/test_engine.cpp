#include "pa/pa.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {

using namespace pa;
using Hours = std::chrono::hours;

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const char* expression, const char* file, int line) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cerr << file << ":" << line << ": CHECK failed: " << expression << "\n";
    }
}

#define CHECK(expression) check((expression), #expression, __FILE__, __LINE__)

template <typename Function>
bool throws_invalid_argument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

template <typename Function>
bool accepts(Function function) {
    try {
        function();
        return true;
    } catch (...) {
        return false;
    }
}

Timestamp start_time() {
    return Timestamp{};
}

Timestamp after_hours(int hours) {
    return Timestamp{} + Hours(hours);
}

TaskComplexity complexity(double volume = 1.0,
                          double difficulty = 1.0,
                          double risk = 1.0) {
    return TaskComplexity{volume, difficulty, risk, 0.2, 0.3, 0.5};
}

Points score(Engine& engine, TaskId task, int fact_hours, int attempts) {
    return engine.calculate_final_score(task,
                                        after_hours(fact_hours),
                                        attempts);
}

void test_user_ids() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);
    const UserId bob = engine.add_user("Bob", 1500);

    CHECK(alice.value == 1);
    CHECK(bob.value == 2);
    CHECK(engine.get_balance(alice) == 1000);
    CHECK(engine.get_balance(bob) == 1500);
    CHECK(engine.get_balance(UserId{999}) == 0);
}

void test_basic_score() {
    Engine engine;
    const TaskId task = engine.add_task("task",
                                        TaskRank::B,
                                        300,
                                        complexity(4.0, 3.0, 2.0),
                                        start_time(),
                                        after_hours(24));

    CHECK(score(engine, task, 12, 1) == 327);
    CHECK(score(engine, task, 24, 1) == 313);
    CHECK(score(engine, task, 1, 1) == 340);
    CHECK(score(engine, task, 0, 1) == 340);
}

void test_attempt_coefficient() {
    Engine engine;
    const TaskId task = engine.add_task("task",
                                        TaskRank::B,
                                        300,
                                        complexity(4.0, 3.0, 2.0),
                                        start_time(),
                                        after_hours(24));

    CHECK(score(engine, task, 24, 1) == 313);
    CHECK(score(engine, task, 24, 3) == 309);
    CHECK(score(engine, task, 24, 5) == 305);
    CHECK(score(engine, task, 24, 100) == 305);
    CHECK(throws_invalid_argument([&] { score(engine, task, 24, 0); }));
    CHECK(throws_invalid_argument([&] { score(engine, task, 24, -1); }));
}

void test_force_rounding() {
    Engine engine;
    const TaskId task = engine.add_task("rounded",
                                        TaskRank::D,
                                        50,
                                        complexity(2.34, 3.34, 4.34),
                                        start_time(),
                                        after_hours(10));

    CHECK(score(engine, task, 10, 1) == 53);
}

void test_penalty() {
    Engine engine;

    const TaskId c_task = engine.add_task("late-c",
                                          TaskRank::C,
                                          100,
                                          complexity(),
                                          start_time(),
                                          after_hours(10));
    CHECK(score(engine, c_task, 15, 1) == 31);

    const TaskId capped_task = engine.add_task("late-cap",
                                               TaskRank::C,
                                               100,
                                               complexity(),
                                               start_time(),
                                               after_hours(1));
    CHECK(score(engine, capped_task, 100, 1) == 10);

    const TaskId d_task = engine.add_task("late-d",
                                          TaskRank::D,
                                          50,
                                          complexity(),
                                          start_time(),
                                          after_hours(10));
    CHECK(score(engine, d_task, 20, 1) == 43);
    CHECK(score(engine, d_task, 10, 1) == 51);
}

void test_complexity_validation() {
    Engine engine;

    CHECK(throws_invalid_argument([&] {
        engine.add_task("v0",
                        TaskRank::C,
                        100,
                        TaskComplexity{0.99, 3.0, 3.0, 0.2, 0.3, 0.5},
                        start_time(),
                        after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("v6",
                        TaskRank::C,
                        100,
                        TaskComplexity{5.01, 3.0, 3.0, 0.2, 0.3, 0.5},
                        start_time(),
                        after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("d6",
                        TaskRank::C,
                        100,
                        TaskComplexity{3.0, 6.0, 3.0, 0.2, 0.3, 0.5},
                        start_time(),
                        after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("r0",
                        TaskRank::C,
                        100,
                        TaskComplexity{3.0, 3.0, 0.0, 0.2, 0.3, 0.5},
                        start_time(),
                        after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("nan-value",
                        TaskRank::C,
                        100,
                        TaskComplexity{std::numeric_limits<double>::quiet_NaN(),
                                       3.0,
                                       3.0,
                                       0.2,
                                       0.3,
                                       0.5},
                        start_time(),
                        after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("nan-weight",
                        TaskRank::C,
                        100,
                        TaskComplexity{3.0,
                                       3.0,
                                       3.0,
                                       std::numeric_limits<double>::quiet_NaN(),
                                       0.3,
                                       0.5},
                        start_time(),
                        after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("inf-weight",
                        TaskRank::C,
                        100,
                        TaskComplexity{3.0,
                                       3.0,
                                       3.0,
                                       std::numeric_limits<double>::infinity(),
                                       0.3,
                                       0.5},
                        start_time(),
                        after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("negative-weight",
                        TaskRank::C,
                        100,
                        TaskComplexity{3.0, 3.0, 3.0, -0.2, 0.7, 0.5},
                        start_time(),
                        after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("weight-sum",
                        TaskRank::C,
                        100,
                        TaskComplexity{3.0, 3.0, 3.0, 0.2, 0.3, 0.4},
                        start_time(),
                        after_hours(1));
    }));
    CHECK(accepts([&] {
        engine.add_task("valid",
                        TaskRank::C,
                        100,
                        complexity(3.0, 3.0, 3.0),
                        start_time(),
                        after_hours(1));
    }));
}

void test_task_validation() {
    Engine engine;

    CHECK(!throws_invalid_argument([&] {
        engine.add_task("d0", TaskRank::D, 0, complexity(), start_time(), after_hours(1));
    }));
    CHECK(!throws_invalid_argument([&] {
        engine.add_task("d50", TaskRank::D, 50, complexity(), start_time(), after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("d51", TaskRank::D, 51, complexity(), start_time(), after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("c50", TaskRank::C, 50, complexity(), start_time(), after_hours(1));
    }));
    CHECK(!throws_invalid_argument([&] {
        engine.add_task("c51", TaskRank::C, 51, complexity(), start_time(), after_hours(1));
    }));
    CHECK(!throws_invalid_argument([&] {
        engine.add_task("c200", TaskRank::C, 200, complexity(), start_time(), after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("c201", TaskRank::C, 201, complexity(), start_time(), after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("b200", TaskRank::B, 200, complexity(), start_time(), after_hours(1));
    }));
    CHECK(!throws_invalid_argument([&] {
        engine.add_task("b201", TaskRank::B, 201, complexity(), start_time(), after_hours(1));
    }));
    CHECK(!throws_invalid_argument([&] {
        engine.add_task("b500", TaskRank::B, 500, complexity(), start_time(), after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("b501", TaskRank::B, 501, complexity(), start_time(), after_hours(1));
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("a500", TaskRank::A, 500, complexity(), start_time(), after_hours(1));
    }));
    CHECK(!throws_invalid_argument([&] {
        engine.add_task("a501", TaskRank::A, 501, complexity(), start_time(), after_hours(1));
    }));

    CHECK(throws_invalid_argument([&] {
        engine.add_task("same-deadline", TaskRank::C, 100, complexity(), start_time(), start_time());
    }));
    CHECK(throws_invalid_argument([&] {
        engine.add_task("reverse-deadline",
                        TaskRank::C,
                        100,
                        complexity(),
                        start_time(),
                        start_time() - Hours(1));
    }));
}

void test_time_and_task_errors() {
    Engine engine;
    const TaskId task = engine.add_task("task",
                                        TaskRank::C,
                                        100,
                                        complexity(),
                                        start_time(),
                                        after_hours(10));

    CHECK(throws_invalid_argument([&] {
        engine.calculate_final_score(task, start_time() - Hours(1), 1);
    }));
    CHECK(throws_invalid_argument([&] {
        engine.calculate_final_score(TaskId{999}, start_time(), 1);
    }));
    CHECK(throws_invalid_argument([&] {
        engine.assign_task(TaskId{999}, UserId{1}, start_time(), 1);
    }));
}

void test_completion() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);
    const TaskId task = engine.add_task("task",
                                        TaskRank::B,
                                        300,
                                        complexity(4.0, 3.0, 2.0),
                                        start_time(),
                                        after_hours(24));

    CHECK(engine.assign_task(task, alice, after_hours(12), 1));
    CHECK(engine.get_balance(alice) == 1327);
    CHECK(!engine.assign_task(task, alice, after_hours(12), 1));
    CHECK(engine.get_balance(alice) == 1327);

    const TaskId second = engine.add_task("second",
                                          TaskRank::C,
                                          100,
                                          complexity(),
                                          start_time(),
                                          after_hours(10));
    CHECK(!engine.assign_task(second, UserId{999}, after_hours(5), 1));
    CHECK(engine.assign_task(second, alice, after_hours(5), 1));
    CHECK(engine.get_balance(alice) == 1327 + 106);
}

void test_start_prices() {
    Engine engine;
    const UserId bidder = engine.add_user("bidder", 1'000'000);

    auto expect_start_price = [&](Points cost,
                                  size_t slots,
                                  size_t interested,
                                  Points expected) {
        const AuctionId auction = engine.start_auction(PrivilegeId{1},
                                                       cost,
                                                       slots,
                                                       interested);
        CHECK(!engine.place_bid(auction, bidder, expected - 1));
        CHECK(engine.place_bid(auction, bidder, expected));
    };

    expect_start_price(100, 2, 5, 525);
    expect_start_price(50, 2, 10, 300);
    expect_start_price(100, 1, 5, 800);
    expect_start_price(100, 2, 100, 600);
    expect_start_price(100, 4, 0, 125);

    CHECK(throws_invalid_argument([&] {
        engine.start_auction(PrivilegeId{1}, 100, 0, 5);
    }));
}

void test_auction_errors() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);

    CHECK(throws_invalid_argument([&] {
        engine.close_auction(AuctionId{999});
    }));

    const AuctionId empty_auction = engine.start_auction(PrivilegeId{1}, 10, 2, 0);
    CHECK(engine.close_auction(empty_auction).empty());
    CHECK(engine.close_auction(empty_auction).empty());
    CHECK(!engine.place_bid(empty_auction, alice, 1000));

    const AuctionId unknown_bidder_auction = engine.start_auction(PrivilegeId{1}, 10, 2, 0);
    CHECK(!engine.place_bid(unknown_bidder_auction, UserId{999}, 1000));
    CHECK(!engine.place_bid(AuctionId{999}, alice, 1000));
}

void test_auction_insufficient_funds() {
    Engine engine;
    const UserId poor = engine.add_user("poor", 3);
    const AuctionId auction = engine.start_auction(PrivilegeId{1}, 2, 1, 0);

    CHECK(!engine.place_bid(auction, poor, 4));
    CHECK(engine.get_balance(poor) == 3);
}

void test_auction_outbid_refund() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);
    const UserId bob = engine.add_user("Bob", 1000);
    const AuctionId auction = engine.start_auction(PrivilegeId{1}, 100, 1, 5);

    CHECK(engine.place_bid(auction, alice, 900));
    CHECK(engine.get_balance(alice) == 100);
    CHECK(engine.place_bid(auction, bob, 1000));
    CHECK(engine.get_balance(alice) == 1000);
    CHECK(engine.get_balance(bob) == 0);
    CHECK(!engine.place_bid(auction, alice, 1000));

    const std::vector<UserId> winners = engine.close_auction(auction);
    CHECK(winners.size() == 1);
    CHECK(winners[0] == bob);
}

void test_auction_same_bidder_raise() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);
    const AuctionId auction = engine.start_auction(PrivilegeId{1}, 100, 1, 5);

    CHECK(engine.place_bid(auction, alice, 800));
    CHECK(engine.get_balance(alice) == 200);
    CHECK(engine.place_bid(auction, alice, 900));
    CHECK(engine.get_balance(alice) == 100);
    CHECK(!engine.place_bid(auction, alice, 900));
    CHECK(!engine.place_bid(auction, alice, 800));

    const std::vector<UserId> winners = engine.close_auction(auction);
    CHECK(winners.size() == 1);
    CHECK(winners[0] == alice);
}

void test_auction_equal_start_price() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);
    const UserId bob = engine.add_user("Bob", 1000);
    const UserId carol = engine.add_user("Carol", 1000);
    const AuctionId auction = engine.start_auction(PrivilegeId{1}, 100, 2, 5);

    CHECK(engine.place_bid(auction, alice, 525));
    CHECK(engine.place_bid(auction, bob, 525));
    CHECK(!engine.place_bid(auction, carol, 524));
    CHECK(!engine.place_bid(auction, carol, 525));
    CHECK(engine.place_bid(auction, carol, 526));

    const std::vector<UserId> winners = engine.close_auction(auction);
    CHECK(winners.size() == 2);
    CHECK(winners[0] == carol);
    CHECK(winners[1] == alice);
    CHECK(engine.get_balance(alice) == 475);
    CHECK(engine.get_balance(bob) == 1000);
    CHECK(engine.get_balance(carol) == 474);
}

void test_auction_multi_slot() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);
    const UserId bob = engine.add_user("Bob", 1000);
    const UserId carol = engine.add_user("Carol", 1000);
    const UserId dave = engine.add_user("Dave", 1000);
    const AuctionId auction = engine.start_auction(PrivilegeId{1}, 100, 2, 5);

    CHECK(engine.place_bid(auction, alice, 600));
    CHECK(engine.place_bid(auction, bob, 700));
    CHECK(engine.place_bid(auction, carol, 650));
    CHECK(!engine.place_bid(auction, dave, 640));
    CHECK(engine.place_bid(auction, dave, 660));

    const std::vector<UserId> winners = engine.close_auction(auction);
    CHECK(winners.size() == 2);
    CHECK(winners[0] == bob);
    CHECK(winners[1] == dave);
    CHECK(engine.get_balance(alice) == 1000);
    CHECK(engine.get_balance(bob) == 300);
    CHECK(engine.get_balance(carol) == 1000);
    CHECK(engine.get_balance(dave) == 340);
}

void test_auction_three_slots() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);
    const UserId bob = engine.add_user("Bob", 1000);
    const UserId carol = engine.add_user("Carol", 1000);
    const UserId dave = engine.add_user("Dave", 1000);
    const AuctionId auction = engine.start_auction(PrivilegeId{1}, 100, 3, 5);

    CHECK(engine.place_bid(auction, alice, 600));
    CHECK(engine.place_bid(auction, bob, 700));
    CHECK(engine.place_bid(auction, carol, 650));
    CHECK(engine.place_bid(auction, dave, 800));

    const std::vector<UserId> winners = engine.close_auction(auction);
    CHECK(winners.size() == 3);
    CHECK(winners[0] == dave);
    CHECK(winners[1] == bob);
    CHECK(winners[2] == carol);
    CHECK(engine.get_balance(alice) == 1000);
    CHECK(engine.get_balance(bob) == 300);
    CHECK(engine.get_balance(carol) == 350);
    CHECK(engine.get_balance(dave) == 200);
}

void test_auction_fewer_bids_than_slots() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);
    const UserId bob = engine.add_user("Bob", 1000);
    const AuctionId auction = engine.start_auction(PrivilegeId{1}, 100, 3, 5);

    CHECK(engine.place_bid(auction, alice, 600));
    CHECK(engine.place_bid(auction, bob, 700));

    const std::vector<UserId> winners = engine.close_auction(auction);
    CHECK(winners.size() == 2);
    CHECK(winners[0] == bob);
    CHECK(winners[1] == alice);
    CHECK(engine.get_balance(alice) == 400);
    CHECK(engine.get_balance(bob) == 300);
}

void test_auction_multi_slot_raise() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);
    const UserId bob = engine.add_user("Bob", 1000);
    const AuctionId auction = engine.start_auction(PrivilegeId{1}, 100, 2, 5);

    CHECK(engine.place_bid(auction, alice, 600));
    CHECK(engine.place_bid(auction, bob, 700));
    CHECK(engine.place_bid(auction, alice, 750));

    const std::vector<UserId> winners = engine.close_auction(auction);
    CHECK(winners.size() == 2);
    CHECK(winners[0] == alice);
    CHECK(winners[1] == bob);
    CHECK(engine.get_balance(alice) == 250);
    CHECK(engine.get_balance(bob) == 300);
}

void test_probation_redistribution_after_displacement() {
    Engine engine;
    const UserId alice = engine.add_user("Alice", 1000);
    const UserId bob = engine.add_user("Bob", 1000);
    const AuctionId auction = engine.start_auction(PrivilegeId{1}, 100, 1, 5);

    CHECK(engine.place_bid(auction, alice, 800));
    CHECK(engine.place_bid(auction, bob, 900));
    CHECK(engine.get_balance(alice) == 1000);
    CHECK(engine.place_bid(auction, alice, 950));

    const std::vector<UserId> winners = engine.close_auction(auction);
    CHECK(winners.size() == 1);
    CHECK(winners[0] == alice);
    CHECK(engine.get_balance(alice) == 50);
    CHECK(engine.get_balance(bob) == 1000);
}

void test_overflow_saturation() {
    Engine engine;
    const Points max = std::numeric_limits<Points>::max();
    const UserId user = engine.add_user("Max", max);
    const TaskId task = engine.add_task("max",
                                        TaskRank::A,
                                        max,
                                        complexity(),
                                        start_time(),
                                        after_hours(1));

    CHECK(engine.calculate_final_score(task, after_hours(1), 1) == max);
    CHECK(engine.assign_task(task, user, after_hours(1), 1));
    CHECK(engine.get_balance(user) == max);

    const AuctionId auction = engine.start_auction(PrivilegeId{1}, max, 1, 100);
    CHECK(engine.place_bid(auction, user, max));

    const std::vector<UserId> winners = engine.close_auction(auction);
    CHECK(winners.size() == 1);
    CHECK(winners[0] == user);
}

} // namespace

int main() {
    test_user_ids();
    test_basic_score();
    test_attempt_coefficient();
    test_force_rounding();
    test_penalty();
    test_complexity_validation();
    test_task_validation();
    test_time_and_task_errors();
    test_completion();
    test_start_prices();
    test_auction_errors();
    test_auction_insufficient_funds();
    test_auction_outbid_refund();
    test_auction_same_bidder_raise();
    test_auction_equal_start_price();
    test_auction_multi_slot();
    test_auction_three_slots();
    test_auction_fewer_bids_than_slots();
    test_auction_multi_slot_raise();
    test_probation_redistribution_after_displacement();
    test_overflow_saturation();

    if (g_failures == 0) {
        std::cout << "[PASS] " << g_checks << " checks\n";
        return 0;
    }

    std::cerr << "[FAIL] " << g_failures << " of " << g_checks << " checks failed\n";
    return 1;
}
