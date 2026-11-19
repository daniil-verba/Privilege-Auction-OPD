#pragma once
#include "types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace pa {

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;

    // Пользователи
    UserId add_user(std::string name, Points initial_balance);
    Points get_balance(UserId user) const;

    // Задачи
    TaskId add_task(std::string description,
                    TaskRank rank,
                    Points base_price,
                    TaskComplexity complexity,
                    Timestamp planned_time,
                    Timestamp deadline);

    Points calculate_final_score(TaskId task,
                                 Timestamp actual_completion_time,
                                 int attempts_made) const;

    bool assign_task(TaskId task, UserId user,
                     Timestamp actual_completion_time,
                     int attempts_made);

    // Аукцион
    AuctionId start_auction(PrivilegeId privilege,
                            Points cost_price,
                            size_t slots,
                            size_t interested_count);
    bool place_bid(AuctionId auction, UserId user, Points amount);
    std::vector<UserId> close_auction(AuctionId auction);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace pa
