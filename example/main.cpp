#include <iostream>
#include "pa/pa.hpp" // Подключаем единый заголовок

int main() {
    // 1. Инициализация движка
    pa::Engine engine;

    // 2. Добавляем пользователей
    pa::UserId alice = engine.add_user("Alice", 1000);
    pa::UserId bob = engine.add_user("Bob", 1500);

    // 3. Добавляем задачу с базовой ценой 500
    pa::TaskId task1 = engine.add_task("Fix critical bug", 500);

    // 4. Проверяем динамическую цену (заглушка вернет 500 * 1.5 = 750)
    pa::Points price = engine.get_current_price(task1, alice);
    std::cout << "Current dynamic price for task: " << price << "\n";

    // 5. Alice покупает задачу
    if (engine.assign_task(task1, alice)) {
        std::cout << "Alice assigned the task! Remaining balance: " 
                  << engine.get_balance(alice) << "\n";
    }

    // 6. Запускаем аукцион на привилегию
    pa::AuctionId auction1 = engine.start_auction(task1, 100);
    
    // Торги
    engine.place_bid(auction1, alice, 200);
    engine.place_bid(auction1, bob, 300); // Bob перебивает
    
    // 7. Закрываем аукцион
    pa::UserId winner = engine.close_auction(auction1);
    std::cout << "Auction winner ID: " << winner << " (Expected: " << bob << ")\n";
    std::cout << "Bob's remaining balance: " << engine.get_balance(bob) << "\n";

    return 0;
}
