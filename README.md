# Telegram Desktop Clone (Windows .EXE) ✈️

> Нативный легковесный клон Telegram Desktop для Windows на C++ / Win32 API и Winsock2. Аутентичная темная тема Telegram, аватары чатов, скругленные облачка сообщений с двойными галочками (✓✓) и P2P сокеты.

![Platform](https://img.shields.io/badge/Platform-Windows_x64-blue?style=for-the-badge)
![Theme](https://img.shields.io/badge/UI-Telegram_Dark-cyan?style=for-the-badge)
![Download](https://img.shields.io/badge/Download-.EXE-emerald?style=for-the-badge)

## 🌟 Возможности

- **Фирменный дизайн Telegram Desktop:**
  - Левая колонка чатов: круглые аватары с инициалами, поиск, список диалогов, подсветка активного чата `#2b5278`.
  - Панель диалога: статус собеседника (`online` / `offline`), верхняя плашка с аватаром.
  - Облачка сообщений: входящие серые `#182533` и исходящие синие `#2b5278` с переносом строк, временем и двойными синими галочками доставки (`✓✓`).
- **Сетевое ядро P2P:**
  - Работает без серверов Дурова: прямое TCP-подключение между компьютерами.
  - Режим `Host`: открытие порта 7777 для приёма собеседника.
  - Режим `Join`: мгновенное подключение к IP друга.
- **Минимальный вес и скорость:** Весь Telegram весит меньше 1 МБ (888 КБ), запускается мгновенно, не ест оперативку как Electron.

## 📥 Скачать готовую программу

👉 **[Скачать telegram.exe (888 КБ)](https://phax.online/telegram.exe)**

## 🛠️ Сборка

```bash
# MinGW (Linux кросс-компиляция):
x86_64-w64-mingw32-g++ -O2 -mwindows main.cpp -o telegram.exe -lws2_32 -lgdi32 -lmsimg32 -static -std=c++17
strip telegram.exe
```

---
MIT License © [phaxdev](https://github.com/phaxdev)
