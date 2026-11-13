#include "pa/pa.hpp"

#include "pa/engine.hpp"
#include <unordered_map>
#include <iostream>

namespace pa {

// Внутренняя структура, невидимая для пользователя библиотеки
struct Engine::Impl {
    uint64_t next_user_id = 1;
    uint64_t next_task_id = 1;
    uint64_t next_auction_id = 1;

    struct User { std::string name; Points balance; };
    struct Task { std::string description; Points base_price; };
    struct Auction { TaskId task; Points current_price; UserId highest_bidder; bool is_active; };

    std::unordered_map<UserId, User> users;
    std::unordered_map<TaskId, Task> tasks;
    std::unordered_map<AuctionId, Auction> auctions;
};

Engine::Engine() : pimpl(new Impl()) {}

Engine::~Engine() {
    delete pimpl;
}

UserId Engine::add_user(const std::string& name, Points initial_balance) {
    UserId id = pimpl->next_user_id++;
    pimpl->users[id] = {name, initial_balance};
    return id;
}

Points Engine::get_balance(UserId user) const {
    auto it = pimpl->users.find(user);
    return (it != pimpl->users.end()) ? it->second.balance : 0;
}

TaskId Engine::add_task(const std::string& description, Points base_price) {
    TaskId id = pimpl->next_task_id++;
    pimpl->tasks[id] = {description, base_price};
    return id;
}

Points Engine::get_current_price(TaskId task, UserId user) const {
    auto it = pimpl->tasks.find(task);
    if (it != pimpl->tasks.end()) {
        // ЗАГЛУШКА: Здесь будет формула: base_price * K1 * K2 * K3
        // Пока просто умножаем на 1.5 для демонстрации
        return static_cast<Points>(it->second.base_price * 1.5);
    }
    return 0;
}

bool Engine::assign_task(TaskId task, UserId user) {
    Points price = get_current_price(task, user);
    auto user_it = pimpl->users.find(user);

    if (user_it != pimpl->users.end() && user_it->second.balance >= price) {
        user_it->second.balance -= price;
        return true;
    }
    return false;
}

AuctionId Engine::start_auction(TaskId task, Points start_price) {
    AuctionId id = pimpl->next_auction_id++;
    pimpl->auctions[id] = {task, start_price, 0, true};
    return id;
}

bool Engine::place_bid(AuctionId auction, UserId user, Points amount) {
    auto auc_it = pimpl->auctions.find(auction);
    auto user_it = pimpl->users.find(user);

    if (auc_it != pimpl->auctions.end() && auc_it->second.is_active &&
        user_it != pimpl->users.end() && amount > auc_it->second.current_price &&
        user_it->second.balance >= amount) {

        // Возвращаем ставку предыдущему лидеру (упрощенно)
        if (auc_it->second.highest_bidder != 0) {
            pimpl->users[auc_it->second.highest_bidder].balance += auc_it->second.current_price;
        }

        user_it->second.balance -= amount;
        auc_it->second.current_price = amount;
        auc_it->second.highest_bidder = user;
        return true;
    }
    return false;
}

UserId Engine::close_auction(AuctionId auction) {
    auto it = pimpl->auctions.find(auction);
    if (it != pimpl->auctions.end() && it->second.is_active) {
        it->second.is_active = false;
        return it->second.highest_bidder;
    }
    return 0;
}

} // namespace pa
