#pragma once
#include "types.hpp"
#include <string>

namespace pa {

class Engine {
public:
    Engine();
    ~Engine();

    // Запрещаем копирование для простоты (можно добавить при необходимости)
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // --- Пользователи ---
    UserId add_user(const std::string& name, Points initial_balance);
    Points get_balance(UserId user) const;

    // --- Задачи ---
    TaskId add_task(const std::string& description, Points base_price);
    Points get_current_price(TaskId task, UserId user) const; // Здесь будет формула K1*K2*K3
    bool assign_task(TaskId task, UserId user);

    // --- Аукцион ---
    AuctionId start_auction(TaskId task, Points start_price);
    bool place_bid(AuctionId auction, UserId user, Points amount);
    UserId close_auction(AuctionId auction); // Возвращает ID победителя

private:
    struct Impl;
    Impl* pimpl; // Скрываем всю внутреннюю логику и поля здесь
};

} // namespace pa
