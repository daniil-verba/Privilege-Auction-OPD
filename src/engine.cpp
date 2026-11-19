#include "pa/engine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pa {
namespace {

constexpr double kMaxTimeFactor = 3.0;
constexpr int kMaxAttempts = 5;
constexpr double kAttemptPenalty = 0.15;
constexpr double kMaxPenaltyRatio = 0.9;
constexpr double kRankDPenaltyRatio = 0.15;
constexpr double kMinComplexityValue = 1.0;
constexpr double kMaxComplexityValue = 5.0;
constexpr double kWeightTolerance = 1e-6;
constexpr long double kMaxDemand = 3.0L;

void validate_complexity(const TaskComplexity& complexity) {
    const double values[] = {
        complexity.volume,
        complexity.difficulty,
        complexity.risk
    };
    for (double value : values) {
        if (!std::isfinite(value)
            || value < kMinComplexityValue
            || value > kMaxComplexityValue) {
            throw std::invalid_argument("complexity values must be between 1 and 5");
        }
    }

    const double weights[] = {
        complexity.w_volume,
        complexity.w_difficulty,
        complexity.w_risk
    };
    for (double weight : weights) {
        if (!std::isfinite(weight) || weight < 0.0 || weight > 1.0) {
            throw std::invalid_argument("complexity weights must be between 0 and 1");
        }
    }

    const double weight_sum = complexity.w_volume
                            + complexity.w_difficulty
                            + complexity.w_risk;
    if (std::fabs(weight_sum - 1.0) > kWeightTolerance) {
        throw std::invalid_argument("complexity weights must sum to 1.0");
    }
}

Points add_points(Points lhs, Points rhs) noexcept {
    const Points max = std::numeric_limits<Points>::max();
    return lhs > max - rhs ? max : lhs + rhs;
}

Points to_points(long double value) noexcept {
    if (!(value > 0.0L)) {
        return 0;
    }

    const long double max = static_cast<long double>(std::numeric_limits<Points>::max());
    if (value >= max) {
        return std::numeric_limits<Points>::max();
    }

    return static_cast<Points>(value);
}

} // namespace

struct Engine::Impl {
    uint64_t next_user_id = 1;
    uint64_t next_task_id = 1;
    uint64_t next_auction_id = 1;

    struct User {
        std::string name;
        Points balance;
    };

    struct Task {
        TaskId id;
        std::string description;
        TaskRank rank;
        Points base_price;
        Timestamp planned_time;
        Timestamp deadline;
        TaskComplexity complexity;
        bool completed = false;
    };

    struct Bid {
        UserId user;
        Points amount;
    };

    struct Auction {
        AuctionId id;
        PrivilegeId privilege;
        Points starting_price;
        size_t slots;
        bool is_active;
        std::vector<Bid> bids;
    };

    std::unordered_map<UserId, User> users;
    std::unordered_map<TaskId, Task> tasks;
    std::unordered_map<AuctionId, Auction> auctions;
};

Engine::Engine() : pimpl_(std::make_unique<Impl>()) {}
Engine::~Engine() = default;
Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

UserId Engine::add_user(std::string name, Points initial_balance) {
    UserId id{pimpl_->next_user_id++};
    pimpl_->users[id] = {std::move(name), initial_balance};
    return id;
}

Points Engine::get_balance(UserId user) const {
    auto it = pimpl_->users.find(user);
    return (it != pimpl_->users.end()) ? it->second.balance : 0;
}

TaskId Engine::add_task(std::string description,
                        TaskRank rank,
                        Points base_price,
                        TaskComplexity complexity,
                        Timestamp planned_time,
                        Timestamp deadline) {
    if (base_price < rank_min_base_price(rank)
        || base_price > rank_max_base_price(rank)) {
        throw std::invalid_argument("base_price is outside the range for the given TaskRank");
    }
    if (deadline <= planned_time) {
        throw std::invalid_argument("deadline must be after planned_time");
    }
    validate_complexity(complexity);

    TaskId id{pimpl_->next_task_id++};
    pimpl_->tasks[id] = {
        id,
        std::move(description),
        rank,
        base_price,
        planned_time,
        deadline,
        complexity,
        false
    };
    return id;
}

Points Engine::calculate_final_score(TaskId task,
                                     Timestamp actual_completion_time,
                                     int attempts_made) const {
    auto t_it = pimpl_->tasks.find(task);
    if (t_it == pimpl_->tasks.end()) {
        throw std::invalid_argument("Task not found");
    }
    const auto& t = t_it->second;

    if (actual_completion_time < t.planned_time) {
        throw std::invalid_argument("completion time cannot precede planned_time");
    }
    if (attempts_made < 1) {
        throw std::invalid_argument("attempts_made must be at least 1");
    }

    const auto t_plan_sec = std::chrono::duration_cast<std::chrono::seconds>(
                                t.deadline - t.planned_time).count();
    const auto t_fact_sec = std::chrono::duration_cast<std::chrono::seconds>(
                                actual_completion_time - t.planned_time).count();

    if (t_plan_sec <= 0) {
        throw std::invalid_argument("planned duration must be positive");
    }

    double k1 = kMaxTimeFactor;
    if (t_fact_sec > 0) {
        k1 = std::min(kMaxTimeFactor,
                      static_cast<double>(t_plan_sec)
                          / static_cast<double>(t_fact_sec));
    }

    const auto& c = t.complexity;
    const double k2_raw = c.w_volume * c.volume
                        + c.w_difficulty * c.difficulty
                        + c.w_risk * c.risk;
    const double k2 = std::round(k2_raw * 10.0) / 10.0;

    const int capped_attempts = std::min(attempts_made, kMaxAttempts);
    const double k3 = 1.0 - kAttemptPenalty
                            * static_cast<double>(capped_attempts - 1);

    const double multiplier = rank_multiplier(t.rank);
    const long double bonus_block = static_cast<long double>(multiplier) * k1 * k2 * k3;

    long double penalty = 0.0L;
    if (t_fact_sec > t_plan_sec) {
        if (t.rank == TaskRank::D) {
            penalty = static_cast<long double>(t.base_price) * kRankDPenaltyRatio;
        } else {
            const long double delay_ratio =
                static_cast<long double>(t_fact_sec - t_plan_sec)
                / static_cast<long double>(t_plan_sec);
            penalty = static_cast<long double>(t.base_price)
                    * std::min(static_cast<long double>(kMaxPenaltyRatio),
                               std::sqrt(delay_ratio));
        }
    }

    const long double raw = static_cast<long double>(t.base_price)
                          + bonus_block
                          - penalty;
    return to_points(raw);
}

bool Engine::assign_task(TaskId task, UserId user,
                         Timestamp actual_completion_time,
                         int attempts_made) {
    auto t_it = pimpl_->tasks.find(task);
    if (t_it == pimpl_->tasks.end()) {
        throw std::invalid_argument("Task not found");
    }
    if (t_it->second.completed) {
        return false;
    }

    auto user_it = pimpl_->users.find(user);
    if (user_it == pimpl_->users.end()) {
        return false;
    }

    const Points score = calculate_final_score(task,
                                               actual_completion_time,
                                               attempts_made);
    user_it->second.balance = add_points(user_it->second.balance, score);
    t_it->second.completed = true;
    return true;
}

AuctionId Engine::start_auction(PrivilegeId privilege,
                                Points cost_price,
                                size_t slots,
                                size_t interested_count) {
    if (slots == 0) {
        throw std::invalid_argument("slots must be at least 1");
    }

    const long double demand =
        std::min(static_cast<long double>(interested_count)
                     / static_cast<long double>(slots),
                 kMaxDemand) + 1.0L;
    const long double rarity = 1.0L + 1.0L / static_cast<long double>(slots);
    const Points starting_price =
        to_points(static_cast<long double>(cost_price) * demand * rarity);

    AuctionId id{pimpl_->next_auction_id++};
    pimpl_->auctions[id] = {id, privilege, starting_price, slots, true, {}};
    return id;
}

bool Engine::place_bid(AuctionId auction, UserId user, Points amount) {
    auto auc_it = pimpl_->auctions.find(auction);
    if (auc_it == pimpl_->auctions.end() || !auc_it->second.is_active) {
        return false;
    }

    auto usr_it = pimpl_->users.find(user);
    if (usr_it == pimpl_->users.end()) {
        return false;
    }

    const auto& auc = auc_it->second;
    auto& bids = auc_it->second.bids;

    bool has_existing_bid = false;
    Points existing_amount = 0;
    for (const auto& bid : bids) {
        if (bid.user == user) {
            has_existing_bid = true;
            existing_amount = bid.amount;
            break;
        }
    }

    if (has_existing_bid && amount <= existing_amount) {
        return false;
    }

    if (has_existing_bid) {
        if (amount - existing_amount > usr_it->second.balance) {
            return false;
        }
    } else if (usr_it->second.balance < amount) {
        return false;
    }

    std::vector<Impl::Bid> others;
    others.reserve(bids.size());
    for (const auto& bid : bids) {
        if (bid.user != user) {
            others.push_back(bid);
        }
    }
    std::stable_sort(others.begin(), others.end(),
                     [](const Impl::Bid& lhs, const Impl::Bid& rhs) {
                         return lhs.amount > rhs.amount;
                     });

    if (others.size() < auc.slots) {
        if (amount < auc.starting_price) {
            return false;
        }
    } else if (amount <= others[auc.slots - 1].amount) {
        return false;
    }

    if (has_existing_bid) {
        usr_it->second.balance -= amount - existing_amount;
        bids.erase(std::remove_if(bids.begin(), bids.end(),
                                  [user](const Impl::Bid& bid) {
                                      return bid.user == user;
                                  }),
                   bids.end());
    } else {
        usr_it->second.balance -= amount;
    }

    bids.push_back({user, amount});
    std::stable_sort(bids.begin(), bids.end(),
                     [](const Impl::Bid& lhs, const Impl::Bid& rhs) {
                         return lhs.amount > rhs.amount;
                     });

    if (bids.size() > auc.slots) {
        const Impl::Bid displaced = bids.back();
        bids.pop_back();

        auto displaced_it = pimpl_->users.find(displaced.user);
        if (displaced_it != pimpl_->users.end()) {
            displaced_it->second.balance =
                add_points(displaced_it->second.balance, displaced.amount);
        }
    }

    return true;
}

std::vector<UserId> Engine::close_auction(AuctionId auction) {
    auto it = pimpl_->auctions.find(auction);
    if (it == pimpl_->auctions.end()) {
        throw std::invalid_argument("Auction not found");
    }
    if (!it->second.is_active) {
        return {};
    }

    it->second.is_active = false;

    const size_t winner_count = std::min(it->second.slots,
                                         it->second.bids.size());
    std::vector<UserId> winners;
    winners.reserve(winner_count);
    for (size_t i = 0; i < winner_count; ++i) {
        winners.push_back(it->second.bids[i].user);
    }
    return winners;
}

} // namespace pa
