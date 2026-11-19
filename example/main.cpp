/**
 * @file main.cpp
 * @brief Демонстрация использования библиотеки Privilege Auction (pa).
 * 
 * Этот файл служит документацией и примером интеграции для фронтенд-разработчиков.
 * Он показывает полный цикл работы системы: от создания пользователей до 
 * расчета стоимости задач и проведения аукциона.
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>

// Единая точка входа в библиотеку
#include "pa/pa.hpp"

// Алиасы для удобства чтения
using namespace pa;
using Clock = std::chrono::system_clock;
using Hours = std::chrono::hours;

// Вспомогательная функция для красивого вывода
void print_separator(const std::string& title) {
    std::cout << "\n========================================\n";
    std::cout << " " << title << "\n";
    std::cout << "========================================\n";
}

int main() {
    try {
        // 1. Инициализация движка
        // В реальном приложении это Singleton или компонент Dependency Injection
        Engine engine;

        print_separator("1. РЕГИСТРАЦИЯ УЧАСТНИКОВ");
        
        // Создаем пользователей с начальным балансом баллов
        // Фронтенд: форма регистрации / импорта из БД
        UserId alice = engine.add_user("Alice (Разработчик)", 1000);
        UserId bob   = engine.add_user("Bob (Дизайнер)", 1500);
        
        std::cout << "Alice ID: " << alice.value << ", Баланс: " << engine.get_balance(alice) << "\n";
        std::cout << "Bob   ID: " << bob.value   << ", Баланс: " << engine.get_balance(bob)   << "\n";


        print_separator("2. ДИНАМИЧЕСКОЕ ЦЕНООБРАЗОВАНИЕ ЗАДАЧИ");
        
        // Сценарий: Админ создает задачу "Исправить критический баг"
        // Ранг B (Высокий): базовая цена должна быть в диапазоне 201-500. Берем 300.
        // Множитель ранга M = 5.
        
        auto now = Clock::now();
        Timestamp planned_start = now;
        Timestamp deadline      = now + Hours(24); // Плановое время выполнения: 24 часа

        // Оценка сложности (K2 - Силы)
        // Сумма весов (w) должна быть строго равна 1.0
        TaskComplexity complexity = {
            4.0,   // Объем: 4/5
            3.0,   // Сложность: 3/5
            2.0,   // Риски: 2/5
            0.2,   // Вес объема
            0.3,   // Вес сложности
            0.5    // Вес рисков (приоритет)
        };

        TaskId bug_task = engine.add_task(
            "Исправить критический баг в модуле оплаты",
            TaskRank::B,        // Ранг B
            300,                // Базовая цена
            complexity,
            planned_start,
            deadline
        );

        std::cout << "Задача создана. ID: " << bug_task.value << "\n";
        std::cout << "Дедлайн: через 24 часа.\n";

        // Сценарий: Alice выполняет задачу БЫСТРО (за 12 часов) и с ПЕРВОЙ попытки.
        // Фронтенд: кнопка "Завершить задачу", отправляет timestamp завершения.
        Timestamp actual_completion = now + Hours(12); 
        int attempts = 1;

        // Расчет итогового балла по формуле: База + (M * K1 * K2 * K3) - Штраф
        Points final_score = engine.calculate_final_score(bug_task, actual_completion, attempts);

        std::cout << "\n--- Расчет награды для Alice ---\n";
        std::cout << "Фактическое время: 12 часов (в 2 раза быстрее плана)\n";
        std::cout << "Попыток: 1\n";
        std::cout << "Итоговый балл (награда): " << final_score << "\n";
        // Ожидаемый расчет:
        // K1 (Время) = min(3.0, 24/12) = 2.0
        // K2 (Силы) = 0.2*4 + 0.3*3 + 0.5*2 = 2.7
        // K3 (Попытки) = 1.0 - 0.15*(1-1) = 1.0
        // M (Ранг B) = 5
        // Бонус = 5 * 2.0 * 2.7 * 1.0 = 27.0
        // Итог = 300 (база) + 27.0 (бонус) - 0 (штраф) = 327


        print_separator("3. АУКЦИОН ПРИВИЛЕГИЙ");

        // Сценарий: Админ выставляет на аукцион "Отгул в пятницу"
        // Себестоимость: 100 баллов. Слотов: 2. Желающих: 5.
        PrivilegeId day_off = PrivilegeId{1};
        Points cost_price = 100;
        size_t slots = 2;
        size_t interested = 5;

        // Запуск аукциона. Стартовая цена считается автоматически:
        // Спрос = min(5/2, 3) + 1 = 3.5
        // Редкость = 1 + 1/2 = 1.5
        // Старт = 100 * 3.5 * 1.5 = 525
        AuctionId auction = engine.start_auction(day_off, cost_price, slots, interested);
        
        std::cout << "Аукцион '" << auction.value << "' запущен.\n";
        std::cout << "Лот: Отгул в пятницу (2 слота)\n";
        std::cout << "Желающих: 5. Стартовая цена рассчитана движком: 525 баллов.\n";

        // Торги (Английский аукцион, шаг не ограничен, платит максимум)
        std::cout << "\n--- Ход торгов ---\n";
        
        // Alice делает ставку 600
        bool bid1 = engine.place_bid(auction, alice, 600);
        std::cout << "Alice ставит 600: " << (bid1 ? "OK" : "Отклонено") 
                  << " (Баланс Alice: " << engine.get_balance(alice) << ")\n";

        // Bob перебивает ставкой 700. Alice получает возврат 600.
        bool bid2 = engine.place_bid(auction, bob, 700);
        std::cout << "Bob ставит 700: " << (bid2 ? "OK" : "Отклонено") 
                  << " (Баланс Bob: " << engine.get_balance(bob) << ", Баланс Alice вернется к " << engine.get_balance(alice) << ")\n";

        // Alice пытается поставить 650 (меньше текущей ставки 700) - должно быть отклонено
        bool bid3 = engine.place_bid(auction, alice, 650);
        std::cout << "Alice ставит 650 (меньше текущей): " << (bid3 ? "OK" : "ОТКЛОНЕНО (правило аукциона)") << "\n";

        // Закрытие аукциона
        UserId winner = engine.close_auction(auction);
        
        std::cout << "\n--- Результаты аукциона ---\n";
        std::cout << "Победитель ID: " << winner.value << " (Bob)\n";
        std::cout << "Финальный баланс Bob: " << engine.get_balance(bob) << " (списано 700)\n";
        std::cout << "Финальный баланс Alice: " << engine.get_balance(alice) << " (возврат после перебития ставки)\n";


        print_separator("4. ОБРАБОТКА ОШИБОК И ГРАНИЧНЫХ СЛУЧАЕВ");
        
        // Попытка создать задачу с неверным рангом (база 600 для ранга B макс 500)
        std::cout << "Попытка создать задачу с некорректной ценой для ранга...\n";
        try {
            engine.add_task("Тест", TaskRank::B, 600, complexity, now, now + Hours(1));
        } catch (const std::invalid_argument& e) {
            std::cout << "Перехвачено исключение: " << e.what() << "\n";
        }

        // Попытка сделать ставку без денег
        std::cout << "Попытка ставки без достаточного баланса...\n";
        bool bad_bid = engine.place_bid(auction, alice, 99999);
        std::cout << "Результат: " << (bad_bid ? "OK" : "Отклонено (недостаточно средств)") << "\n";

        std::cout << "\n[SUCCESS] Пример работы библиотеки завершен.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[FATAL ERROR] " << e.what() << "\n";
        return 1;
    }
}
