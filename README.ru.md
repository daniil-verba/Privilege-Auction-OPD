# Privilege-Auction-OPD

🇬🇧 [English](README.md) | 🇷🇺 [Русский](README.ru.md)

---

## Сборка

**Windows (cmd):**
```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

**Linux (bash):**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

---

## 📚 Документация

- 📖 [Правила участия (Contributing)](CONTRIBUTING.ru.md) — Как настроить проект, работать с ветками и создавать Pull Request.
- 📝 [Журнал изменений (Changelog)](CHANGELOG.ru.md) — История проекта и обновления версий.
- 🛠️ [Техническая документация](docs/) — Внутренние гайды и технические материалы.