#include "pa/engine.hpp"

#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace pa {

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
    };

    struct Auction {
        AuctionId id;
        PrivilegeId privilege;
        Points current_highest_bid;
        UserId highest_bidder;
        bool is_active;
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
    if (base_price > rank_max_base_price(rank)) {
        throw std::invalid_argument("base_price exceeds maximum for the given TaskRank");
    }
    const double w_sum = complexity.w_volume
                       + complexity.w_difficulty
                       + complexity.w_risk;
    if (std::fabs(w_sum - 1.0) > 1e-6) {
        throw std::invalid_argument("complexity weights must sum to 1.0");
    }

    TaskId id{pimpl_->next_task_id++};
    pimpl_->tasks[id] = {
        id,
        std::move(description),
        rank,
        base_price,
        planned_time,
        deadline,
        complexity
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

    const auto t_plan_sec = std::chrono::duration_cast<std::chrono::seconds>(
                                t.deadline - t.planned_time).count();
    const auto t_fact_sec = std::chrono::duration_cast<std::chrono::seconds>(
                                actual_completion_time - t.planned_time).count();

    double k1 = 0.0;
    if (t_fact_sec > 0 && t_plan_sec > 0) {
        k1 = std::min(3.0, static_cast<double>(t_plan_sec)
                            / static_cast<double>(t_fact_sec));
    } else {
        k1 = 3.0;
    }

    const auto& c = t.complexity;
    double k2_raw = c.w_volume   * c.volume
                  + c.w_difficulty * c.difficulty
                  + c.w_risk     * c.risk;
    double k2 = std::round(k2_raw * 10.0) / 10.0;

    const int safe_attempts = std::max(1, std::min(attempts_made, 5));
    double k3 = 1.0 - 0.15 * static_cast<double>(safe_attempts - 1);
    if (k3 < 0.25) k3 = 0.25;

    const double M = rank_multiplier(t.rank);
    const double bonus_block = M * k1 * k2 * k3;

    double penalty = 0.0;
    if (t_fact_sec > t_plan_sec && t_plan_sec > 0) {
        const double delay_ratio =
            static_cast<double>(t_fact_sec - t_plan_sec)
            / static_cast<double>(t_plan_sec);

        if (t.rank == TaskRank::D) {
            penalty = bonus_block * 0.15;
        } else {
            penalty = std::min(bonus_block * 0.9,
                               bonus_block * delay_ratio);
        }
    }

    const double raw = static_cast<double>(t.base_price) + bonus_block - penalty;
    return static_cast<Points>(std::max(0.0, raw));
}

bool Engine::assign_task(TaskId task, UserId user,
                         Timestamp actual_completion_time,
                         int attempts_made) {
    const Points price = calculate_final_score(task, actual_completion_time, attempts_made);
    auto user_it = pimpl_->users.find(user);
    if (user_it == pimpl_->users.end()) return false;
    if (user_it->second.balance < price) return false;

    user_it->second.balance -= price;
    return true;
}

AuctionId Engine::start_auction(PrivilegeId privilege,
                                Points cost_price,
                                size_t slots,
                                size_t interested_count) {
    const double demand =
        std::min(static_cast<double>(interested_count)
                     / std::max(static_cast<double>(slots), 1.0),
                 3.0) + 1.0;
    const double rarity = 1.0 + 1.0 / std::max(static_cast<double>(slots), 1.0);
    const Points starting_price =
        static_cast<Points>(static_cast<double>(cost_price) * demand * rarity);

    AuctionId id{pimpl_->next_auction_id++};
    pimpl_->auctions[id] = {id, privilege, starting_price, UserId{0}, true};
    return id;
}

bool Engine::place_bid(AuctionId auction, UserId user, Points amount) {
    auto auc_it = pimpl_->auctions.find(auction);
    auto usr_it = pimpl_->users.find(user);

    if (auc_it == pimpl_->auctions.end() || !auc_it->second.is_active) return false;
    if (usr_it == pimpl_->users.end()) return false;

    if (amount <= auc_it->second.current_highest_bid) return false;
    if (usr_it->second.balance < amount) return false;

    if (auc_it->second.highest_bidder.value != 0) {
        auto prev = pimpl_->users.find(auc_it->second.highest_bidder);
        if (prev != pimpl_->users.end()) {
            prev->second.balance += auc_it->second.current_highest_bid;
        }
    }

    usr_it->second.balance -= amount;
    auc_it->second.current_highest_bid = amount;
    auc_it->second.highest_bidder = user;
    return true;
}

UserId Engine::close_auction(AuctionId auction) {
    auto it = pimpl_->auctions.find(auction);
    if (it == pimpl_->auctions.end() || !it->second.is_active) {
        return UserId{0};
    }
    it->second.is_active = false;
    return it->second.highest_bidder;
}

} // namespace pa
