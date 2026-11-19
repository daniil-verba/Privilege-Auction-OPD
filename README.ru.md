# Privilege-Auction-OPD

🇬🇧 [English](README.md) | 🇷🇺 Русский

[![CI](https://github.com/IceMan4ik/Privilege-Auction-OPD/actions/workflows/ci.yml/badge.svg?branch=dev)](https://github.com/IceMan4ik/Privilege-Auction-OPD/actions/workflows/ci.yml)

---

## О проекте

C++-библиотека для геймификационных систем с **динамической стоимостью задач** и **аукционами привилегий**. Предоставляет чистый API для расчёта вознаграждения за задачу в зависимости от времени, сложности и числа попыток, а также для проведения английских аукционов.

**Ключевые возможности:**
- Строгие типы (`UserId`, `TaskId`, `AuctionId`) — ID разных сущностей невозможно перепутать на этапе компиляции
- `Engine` на основе pimpl — стабильный ABI, реализация скрыта
- Формулы из спецификации: `Score = Base + (M × K₁ × K₂ × K₃) − Penalty`
- Аукцион с автоматическим расчётом стартовой цены: `Cost × Demand × Rarity`

---

## Быстрый старт

```cpp
#include "pa/pa.hpp"
#include <chrono>
#include <iostream>

int main() {
    pa::Engine engine;

    // 1. Добавляем пользователей
    auto alice = engine.add_user("Alice", 1000);
    auto bob   = engine.add_user("Bob",   1500);

    // 2. Создаём задачу (ранг B → множитель M=5, базовая цена 300)
    pa::TaskComplexity complexity = {
        /*volume=*/4.0, /*difficulty=*/3.0, /*risk=*/2.0,
        /*w_volume=*/0.2, /*w_difficulty=*/0.3, /*w_risk=*/0.5
    };
    auto now = std::chrono::system_clock::now();
    auto task = engine.add_task(
        "Исправить критический баг",
        pa::TaskRank::B,
        /*base_price=*/300,
        complexity,
        /*planned_time=*/now,
        /*deadline=*/now + std::chrono::hours(24)
    );

    // 3. Считаем награду (выполнено за 12 часов, 1 попытка)
    auto score = engine.calculate_final_score(
        task, now + std::chrono::hours(12), /*attempts=*/1
    );
    std::cout << "Score: " << score << "\n";  // 300 + (5*2.0*2.7*1.0) = 327

    // 4. Запускаем аукцион
    auto auction = engine.start_auction(
        /*privilege=*/pa::PrivilegeId{1},
        /*cost_price=*/100,
        /*slots=*/2,
        /*interested=*/5
    );
    engine.place_bid(auction, alice, 600);
    engine.place_bid(auction, bob,   700);
    auto winner = engine.close_auction(auction);
    std::cout << "Winner: " << winner.value << "\n";  // Bob (ID=2)
}
```

---

## Сборка

**Windows (cmd):**
```cmd
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

**Linux (bash):**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

Результат: `build/lib/libPrivilege-Auction-OPD.a` (или `.so`, если переключить тип библиотеки на `SHARED` в `CMakeLists.txt`). Демо: `build/bin/pa_demo` (`build/bin/Release/pa_demo.exe` на Windows).

---

## Интеграция в проект

### 1. Скопируйте заголовки

Скопируйте `include/pa/` в include-путь вашего проекта (например, `APP_GUI/include/pa/`).

### 2. Слинкуйте библиотеку

**CMake:**
```cmake
target_include_directories(your_app PRIVATE /path/to/pa/include)
target_link_libraries(your_app PRIVATE /path/to/libPrivilege-Auction-OPD.a)
```

**Напрямую через GCC/Clang:**
```bash
g++ main.cpp -I/path/to/pa/include -L/path/to/lib -lPrivilege-Auction-OPD -std=c++17
```

### 3. Используйте

```cpp
#include "pa/pa.hpp"   // единая точка входа
```

Все публичные типы находятся в пространстве имён `pa::`.

---

## Основные концепции

| Компонент | Назначение |
|---|---|
| `pa::Engine` | Главный фасад. Владеет пользователями, задачами и аукционами. Не потокобезопасен — один экземпляр на «комнату». |
| `TaskRank` (D/C/B/A) | Определяет множитель **M** (1/3/5/10) и проверяет диапазон базовой цены. |
| `TaskComplexity` | Взвешенная сумма объёма/сложности/рисков (сумма весов должна быть равна 1.0). Это **K₂**. |
| `calculate_final_score()` | Применяет полную формулу: `Base + (M·K₁·K₂·K₃) − Penalty`. |
| `start_auction()` | Автоматически рассчитывает стартовую цену по себестоимости, числу слотов и числу желающих. |
| `place_bid()` / `close_auction()` | Английский аукцион. Предыдущий участник получает возврат; победитель платит свою максимальную ставку. |

### Формула награды

```
Score = Base + (M × K₁ × K₂ × K₃) − Penalty

K₁ (время)      = min(3.0, T_plan / T_fact)
K₂ (силы)       = w_vol·vol + w_diff·diff + w_risk·risk   (округление до 0.1)
K₃ (попытки)    = 1 − 0.15·(n−1),  n ≤ 5,  минимум 0.25
M (ранг)        = 1 (D) / 3 (C) / 5 (B) / 10 (A)
Penalty         = 0, если T_fact ≤ T_plan
                  15% от бонуса для ранга D
                  min(90% от бонуса, бонус·delay_ratio) в остальных случаях
```

### Формула аукциона

```
StartPrice = Cost × Demand × Rarity
Demand     = min(interested / slots, 3) + 1
Rarity     = 1 + 1 / slots
```

---

## CI/CD

- **CI** — собирает проект в Debug и Release на Linux, Windows и macOS, затем запускает smoke-тест `pa_demo`.
- **Release** — по тегу `v*` собирает пакеты для трёх платформ и прикладывает их к GitHub Release.

---

## 📚 Документация

- 📖 [Правила участия (Contributing)](CONTRIBUTING.ru.md) — как настроить проект, работать с ветками и создавать Pull Request.
- 📝 [Журнал изменений (Changelog)](CHANGELOG.md) — история проекта и обновления версий.
- 🛠️ [Техническая документация](docs/) — внутренние гайды и материалы по архитектуре.
- 💡 [Полный пример](example/main.cpp) — запускаемая демонстрация всех возможностей.

---

## 👥 Участники

<table>
  <tr>
    <td align="center">
      <a href="https://github.com/daniil-verba">
        <img src="https://github.com/daniil-verba.png" width="100px;" alt="daniil-verba"/>
        <br />
        <sub><b>Daniil Verba</b></sub>
      </a>
      <br />
      <sub>Разработчик API</sub>
    </td>
    <td align="center">
      <a href="https://github.com/KrioGenius">
        <img src="https://github.com/KrioGenius.png" width="100px;" alt="KrioGenius"/>
        <br />
        <sub><b>Ilya Ukolov</b></sub>
      </a>
      <br />
      <sub>Тимлид</sub>
    </td>
  </tr>
</table>
