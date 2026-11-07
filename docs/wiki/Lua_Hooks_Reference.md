# Справочник методов Lua, вызываемых из C++ (Хуки и События)

> Проект: Jagged Alliance 2 v1.13 (ветка 7609 + AI-модификации)
> Составлено по анализу исходного кода. Часть 1 — хуки C++→Lua; Часть 2 — API Lua→C++.

---

# ЧАСТЬ 1. Хуки: C++ → Lua

## Общая информация

### Механизмы вызова Lua из C++

В этой кодовой базе используется **несколько независимых Lua-машин (lua_State)**, управляемых обёрткой `LuaState` (`lua\lua_state.h/.cpp`):

| Lua-состояние | Где инициализируется | Назначение |
| :--- | :--- | :--- |
| `LUA_STATE_STRATEGIC_MINES_AND_UNDERGROUND` | `Init.cpp / LoadExternalGameplayData` (стр. 1485) | Шахты (`Strategic Mines LUA.cpp`) и подземные сектора (`UndergroundInit.cpp`) |
| `LUA_STATE` (основная, `_LS`) | `InitializeLua()` (`lua\lua.cpp:179`) при `InitOverhead` (`Tactical\Overhead.cpp:797`) | Все скрипты стратегического/NPC-слоя через `LuaInitNPCs.cpp` |
| `LUA_STATE_STRUCTURE`, `LUA_STATE_GUI` | объявлены, но практически не используются | — |

**Основные обёртки для безопасного вызова:**

1. **`LuaFunction`** (`lua\lua_function.h/.cpp`) — основной класс-обёртка. Цепочка вызовов:
   ```cpp
   LuaFunction(_LS.L, "HandleQuestCodeOnSectorEntry").Param<int>(x).Param<int>(y).Param<int>(z).Call(3);
   ```
   Внутри выполняет `lua_getglobal` → пуш параметров → `lua_pcall` с обработкой ошибок. Поддерживает типы `int`, `double`, `std::string`, `bool`, `unsigned int`, а также сборку Lua-таблиц (`TableOpen()/TableClose()`).

2. **Обёртки-«хуки» `LetLua*` / `Lua*`** — почти все объявлены и реализованы в **`Strategic\LuaInitNPCs.cpp`** (файл ~13000 строк — центральный мост C++↔Lua). Каждый хук имеет сигнатуру вида `BOOLEAN LetLuaXxx(аргументы..., UINT8 Init)` и перед вызовом делает:
   - `EvalFile("scripts\Xxx.lua")` — загрузку скрипта;
   - `IniFunction()` / `IniGlobalGameSetting()` — регистрацию ~560 C-функций и глобальных настроек в Lua (`lua_register(L, "HireMerc", ...)` и т.д., см. `Luaglobal.cpp`).

3. **Сырой Lua API** (`lua_getglobal` + `lua_call` / `lua_pcall`) — используется напрямую в `Strategic Mines LUA.cpp`, `UndergroundInit.cpp` и внутри старых хуков `LuaInitNPCs.cpp`.

### Важное замечание по TacticalAI

В **данной ветке** папки `TacticalAI\` и `ModularizedTacticalAI\` **не содержат ни одного прямого вызова Lua** (нет включений lua-заголовков, `LuaFunction`, `LuaState`, хуков в `AIInternals.h` / `ai.h`). Тактический ИИ (AIMain.cpp, DecideAction.cpp, ModularizedTacticalAI и т.д.) работает полностью на C++. Мост в Lua для тактики существует только через объектную модель `lua\lua_tactical.cpp` (класс `ja2_SoldierClass`: `SoldierWalkTo`, `SoldierRunTo`, `SoldierChangeStance`, геттеры/сеттеры grid/level/APs) — т.е. Lua может управлять солдатами, но движок не «дёргает» Lua-хуки ИИ. Файл `AIHandicaps.cpp` и функция вида `AiraidHeliTurn` в этой ветке отсутствуют (хук `HandleEarlyMorningEvents` заменяет авианалётную логику).

---

## 1. Инициализация игры (GameInit)

Скрипты: `scripts\GameInit.lua`, `scripts\InitStrategicLayer.lua` (JA2-5 Unfinished Business). Хук-обёртка: `LetLuaGameInit(UINT8 Init)`, `LuaInitStrategicLayer(UINT8 Init)` — `Strategic\LuaInitNPCs.cpp:2601, 1860`.

| Имя функции Lua | Вызывающий C++ файл и метод | Игровое событие / Условие вызова | Описание (Зачем вызывается) |
| :--- | :--- | :--- | :--- |
| `InitNewGame` | Game Init.cpp / `InitNewGame()` → `LetLuaGameInit(0)` (`LuaInitNPCs.cpp:2617`) | Начало новой игры | Полная Lua-инициализация новой кампании: факты, NPC, квесты, стартовые условия |
| `InitNPCs` | Game Init.cpp / `InitNPCs()` → `LetLuaGameInit(1)` (`LuaInitNPCs.cpp:2622`) | Загрузка/инициализация всех NPC-профилей | Создаёт и настраивает всех скриптовых NPC (профили, инвентарь, расписание) |
| `InitStrategicLayer` (вариант из GameInit.lua) | Game Init.cpp / `InitCustomStrategicLayer()` → `LetLuaGameInit(2)` (`LuaInitNPCs.cpp:2627`) | Пользовательская (модовая) инициализация стратегического слоя | Переопределяемая модами инициализация карты кампании |
| `InitStrategicLayer` (вариант из InitStrategicLayer.lua) | Game Init.cpp / `InitStrategicLayer()` → `LuaInitStrategicLayer(0)` (`LuaInitNPCs.cpp:1873`) | Инициализация стратегического слоя в стиле JA25 UB | Загружает UB-версию стратегической инициализации |
| `MakeBadSectorListFromMapsOnHardDrive` | mapscreen.cpp (5303); SaveLoadScreen.cpp / `SaveGame` (2569); SaveLoadGame.cpp / `LoadSavedGame` (4688) → `LetLuaMakeBadSectorListFromMapsOnHardDrive(0)` (`LuaInitNPCs.cpp:1854`) | Сохранение/загрузка игры, экран карты | Сканирует папку Maps и строит список «битых»/отсутствующих секторов для валидации сейва |

Служебная инициализация Lua-состояний (без вызова именованных функций):

| Lua-ресурс | Вызывающий C++ файл и метод | Условие | Описание |
| :--- | :--- | :--- | :--- |
| `logging` (глобальная переменная, чтение) | UndergroundInit.cpp / `LuaUnderground::LoadScript` (260) | `Init.cpp:1485` при загрузке внешних данных игры | Читает флаг логирования для подсистемы подземелий |
| скрипт `initmines.lua` (`luaL_dostring`) | Strategic Mines LUA.cpp / `LuaMines::LoadScript` (40–63) | `Init.cpp:1488` при загрузке данных | Определяет таблицы всех шахт Арулько |
| скрипты `initunderground.lua`, `<lang>\undergroundsectornames.lua` | UndergroundInit.cpp / `RunScript` (274) | `Init.cpp:1486` | Определяет список подземных секторов и их названия (локализуемые) |
| `LuaTacticalSetup`, `LuaStrategicSetup`, `LuaEnvironmentSetup` | lua\lua.cpp / `InitializeLua` (179), вызывается из Tactical\Overhead.cpp / `InitOverhead` (797) | Инициализация тактического движка | Регистрирует классы `ja2_SoldierClass`, `ja2_SectorClass`, `ja2_MobileGroup`, `ja2_EnvironmentClass` (Lua→C++ мост) |
| `ShutdownLua` | Tactical\Overhead.cpp / `ShutdownOverhead` (804) | Выход из игры/деинициализация | Закрытие Lua-состояний |

---

## 2. Вступительные ролики и музыка (Intro / Music)

Скрипты: `scripts\Intro.lua`, `scripts\Music.lua`. Хук: `LuaIntro(UINT8 Init, ...)` (`LuaInitNPCs.cpp:1724`), `LetLuaMusicControl(UINT8 Init)` (`LuaInitNPCs.cpp:1703`).

| Имя функции Lua | Вызывающий C++ файл и метод | Игровое событие / Условие вызова | Описание (Зачем вызывается) |
| :--- | :--- | :--- | :--- |
| `PrepareToExitIntroScreen` | Intro.cpp / `HandleIntroScreen` → `LuaIntro(0,0,0,0)` (541) | Завершение экрана вступительных роликов | Скрипт готовит переход от интро к основному меню/игре |
| `GetNextIntroVideo` | Intro.cpp / `HandleIntroScreen` → `LuaIntro(1, uiCurrentVideo,0,0)` (578) | Перед воспроизведением следующего видеоролика интро | Скрипт возвращает ID следующего ролика (кастомизируемая последовательность интро) |
| `SetIntroType` | Intro.cpp / `HandleIntroScreen` → `LuaIntro(2,0,bIntroType,0)` (690) | Установка типа интро (новая игра / продолжение) | Сообщает Lua, какой вариант интро проигрывать |
| `Music` | Init.cpp / `LoadExternalGameplayData` → `LetLuaMusicControl(0)` (`LuaInitNPCs.cpp:1716`) | Загрузка внешних игровых данных при старте | Инициализация системы музыки: списки треков и правила выбора |

---

## 3. Логика секторов (вход/выход, подземелья, шахты)

Скрипты: `scripts\strategicmap.lua`, `scripts\initunderground.lua`, `scripts\initmines.lua`. Хук: `LuaHandleQuestCodeOnSector` (`LuaInitNPCs.cpp:2574`), классы `LuaUnderground` (`UndergroundInit.cpp`) и `LuaMines` (`Strategic Mines LUA.cpp`).

| Имя функции Lua | Вызывающий C++ файл и метод | Игровое событие / Условие вызова | Описание (Зачем вызывается) |
| :--- | :--- | :--- | :--- |
| `HandleQuestCodeOnSectorEntry` | Strategic\strategicmap.cpp / `HandleQuestCodeOnSectorEntry` (2763) → `LuaHandleQuestCodeOnSector(x,y,z,1)` | Игрок (или отряд) входит в сектор | Выполняет скриптовую логику квестов/событий при входе в сектор |
| `HandleQuestCodeOnSectorExit` | Strategic\strategicmap.cpp / `HandleQuestCodeOnSectorExit` (2815) → `LuaHandleQuestCodeOnSector(x,y,z,0)` | Отряд покидает сектор | Выполняет скриптовую логику при выходе из сектора (очистка, счётчики) |
| `BuildUndergroundSectorList` | Strategic\UndergroundInit.cpp / `LuaUnderground::InitializeSectorList` (53,61) — вызывается из Campaign Init.cpp / `BuildUndergroundSectorInfoList` (406) | Генерация кампании (Game Init, Queen Command.cpp:2158, Strategic AI.cpp:3797, Creature Spreading.cpp:1239) | Скрипт строит полный список подземных секторов (уровни, связи, выходы) |
| `GetLoadscreen` | UndergroundInit.cpp / `LuaUnderground::GetLoadscreen` (143,148) — вызывается из Loading Screen.cpp / `DisplayLoadScreenWithID` (431) | Отображение экрана загрузки при переходе в подземный сектор | Скрипт возвращает путь и формат картинки загрузочного экрана для конкретного подземелья |
| `GetSectorName` | UndergroundInit.cpp / `LuaUnderground::GetSectorName` (182,187) — вызывается из strategicmap.cpp / `GetSectorIDString` (3582) | Формирование текстового названия сектора (карта, UI) | Скрипт возвращает локализованное название подземного сектора |
| `InitializeMines` | Strategic Mines LUA.cpp / `LuaMines::InitializeMines` (137,73–161) — вызывается из Strategic Mines.cpp / `InitializeMines` (144), которую зовут Game Init.cpp:468 и mapscreen.cpp:7844 | Инициализация/переинициализация экономики шахт | Скрипт задаёт производительность, тип руды и состояние всех шахт |
| `InitializeHeadMiners` | Strategic Mines LUA.cpp / `LuaMines::InitializeHeadMiners` (291,84–326) — вызывается из strategicmap.cpp (2712) | Вход в сектор с шахтой | Скрипт расставляет «главных шахтёров» (head miners) в секторе шахты |

---

## 4. Квесты и События (Quests)

Скрипты: `scripts\Quests.lua`, `scripts\HourlyUpdate.lua`, `scripts\GameEventHook.lua`. Хуки: `LuaInternalQuest` (`LuaInitNPCs.cpp:2696`), `LetLuaHourlyQuestUpdate` (`LuaInitNPCs.cpp:2927`).

| Имя функции Lua | Вызывающий C++ файл и метод | Игровое событие / Условие вызова | Описание (Зачем вызывается) |
| :--- | :--- | :--- | :--- |
| `InternalStartQuest` | Strategic\Quests.cpp / `InternalStartQuest` (1860) → `LuaInternalQuest(quest,x,y,history,1)` | Старт квеста | Уведомляет Lua-слой о начале квеста: скрипт выставляет факты, таймеры, Journal |
| `InternalEndQuest` | Strategic\Quests.cpp / `InternalEndQuest` (1893) → `LuaInternalQuest(quest,x,y,history,0)` | Завершение квеста | Уведомляет Lua-слой о завершении квеста: награды, факты, записи в журнал |
| `HourlyQuestUpdate` | Strategic\Hourly Update.cpp / `HourlyQuestUpdate` (281) → `LetLuaHourlyQuestUpdate(0)` | ⚠️ **Отключено переключателем**: `LUA_HOURLY_QUEST_UPDATE` закомментирован в Strategic\Luaglobal.h (26) — работает C++-ветка `#else` | Скрипт обрабатывал бы почасовую квестовую логику (бордель в 4:00 и т.д.); сейчас логика захардкожена в C++ |
| `CheckFact` | Strategic\LuaInitNPCs.cpp / `LuaCheckFact` (2370, сырой вызов 2404–2407) | Отключено: вызов закомментирован в Strategic\Quests.cpp:1142 | Прямой C++→Lua запрос факта (сейчас `CheckFact` работает только как C-функция, экспортируемая **в** Lua) |
| `CheckForKingpinsMoneyMissing` | Strategic\LuaInitNPCs.cpp / `LuaCheckForKingpinsMoneyMissing` (2419, сырой вызов 2455–2457) | Отключено: закомментировано в Strategic Event Handler.cpp:571 | Проверка пропавших денег Кингпина (квест Деньги Кингпина) |
| `HandleDelayedItemsArrival` | Strategic\LuaInitNPCs.cpp / `LuaHandleDelayedItemsArrival` (2469, сырой вызов 2506–2508) | Отключено: закомментировано в Strategic Event Handler.cpp:419 | Доставка отложенных посылок/предметов по расписанию |
| `ExecuteStrategicEvent` | Strategic\LuaInitNPCs.cpp / `LuaExecuteStrategicEvent` (2677) | Мёртвый хук: активных вызывающих нет (объявлен в LuaInitNPCs.h:68) | Планируемый универсальный хук стратегических событий (EventCallbackID, Timestamp, Type, Flags, Param) |
| `HandleQuestCodeOnSectorEntry` (сырой вариант) | Strategic\LuaInitNPCs.cpp / `LuaHandleQuestCodeOnSectorEntry` (2520, вызов 2557–2561) | Мёртвый хук: заменён `LuaHandleQuestCodeOnSector` | Устаревшая версия хука входа в сектор |

---

## 5. Диалоги и NPC (InterfaceDialogue / NPC Actions)

Скрипты: `scripts\InterfaceDialogue.lua`, `scripts\Overhead.lua`, `scripts\HandleNPCDoAction\%03d.lua`. Хуки: `LetLuaInterfaceDialogue` (`LuaInitNPCs.cpp:2878`), `LetLuaMyCustomHandleAtNewGridNo` (`LuaInitNPCs.cpp:2720`), `LuaHandleNPCDoAction` (`LuaInitNPCs.cpp:2823`).

| Имя функции Lua | Вызывающий C++ файл и метод | Игровое событие / Условие вызова | Описание (Зачем вызывается) |
| :--- | :--- | :--- | :--- |
| `CarmenLeavesSectorCallback` | Tactical\Interface Dialogue.cpp / `CarmenLeavesSectorCallback` (5240) → `LetLuaInterfaceDialogue(0,0)` | 10-секундный таймер ухода Кармена (таймер ставится в строке 3023 того же файла) | Скрипт обрабатывает уход Кармена из сектора (квест с сумкой/деньгами) |
| `HandleStuffForNPCEscorted` | Tactical\Interface Dialogue.cpp / `HandleStuffForNPCEscorted` (1749) → `LetLuaInterfaceDialogue(ubNPC,1)` | NPC-профиль присоединён к отряду игрока (эскортируется) | Скрипт выполняет реакции NPC и квестовые последствия при взятии NPC в отряд |
| `HandleAtNewGridNo` | Tactical\Overhead.cpp / `HandleAtNewGridNo` (3450) → `LetLuaMyCustomHandleAtNewGridNo(0, ubProfile, 0)` | Солдат (NPC с профилем) встаёт на новую клетку (gridNo) | Профильно-зависимая скриптовая логика перемещения NPC (триггеры зоны, реплики) |
| `HandleNPCDoAction` | Strategic\LuaInitNPCs.cpp / `LuaHandleNPCDoAction` (2823, сырой вызов 2860–2864) | Отключено: закомментировано в Tactical\Interface Dialogue.cpp:4514 | Диспетчер NPC-действий: передаёт скрипту (ubTargetNPC, usActionCode, ubQuoteNum) исполнение действия NPC |
| `LoadScr` | Strategic\LuaInitNPCs.cpp / `LuaIDScripts` (2743) | Мёртвый хук: активных вызывающих нет | Загрузчик per-action скриптов `HandleNPCDoAction\NNN.lua` по ID действия |
| `HandleNPCSystemEvent` | Strategic\LuaInitNPCs.cpp / `LetLuaHandleNPCSystemEvent` (8276, вызов 8314–8316) | Отключено: закомментировано в Strategic Event Handler.cpp:657 | Хук системных NPC-событий; должен вызываться из `HandleNPCSystemEvent` (Game Event Hook.cpp:303) |

---

## 6. Лояльность городов (Town Loyalty)

Скрипты: `scripts\StrategicTownLoyalty.lua`. Хуки: `LetHandleLoyaltyChangeForNPCAction` (`LuaInitNPCs.cpp:11545`), `LuaHandleGlobalLoyaltyEvent` (`LuaInitNPCs.cpp:11567`).

| Имя функции Lua | Вызывающий C++ файл и метод | Игровое событие / Условие вызова | Описание (Зачем вызывается) |
| :--- | :--- | :--- | :--- |
| `HandleLoyaltyChangeForNPCAction` | Strategic\Strategic Town Loyalty.cpp / `HandleLoyaltyChangeForNPCAction` (1912) | Действие конкретного NPC влияет на лояльность города (по профилю) | Скрипт пересчитывает модификаторы лояльности, связанные с этим NPC |
| `HandleGlobalLoyaltyEvent` | Strategic\Strategic Town Loyalty.cpp / `HandleGlobalLoyaltyEvent` (1659) | Глобальное событие лояльности: победа/поражение в бою, захват города, оставление ополчения и т.д. | Скрипт применяет изменение лояльности по типу события (ubEventType) и сектору |

---

## 7. Тактика, предметы и взрывы (Tactical / Items)

Скрипты: `scripts\ExplosionControl.lua`, `scripts\Overhead.lua`. Хук: `LetLuaPerformItemAction` (`LuaInitNPCs.cpp:2905`).

| Имя функции Lua | Вызывающий C++ файл и метод | Игровое событие / Условие вызова | Описание (Зачем вызывается) |
| :--- | :--- | :--- | :--- |
| `PerformItemAction` | TileEngine\Explosion Control.cpp / `PerformItemAction` (3280) | Использован/сработал предмет с ActionID (взрыв, ловушка, активация) | Скрипт исполняет кастомные эффекты предмета на сетке sGridNo (скриптуемые предметы) |

*Примечание: системный хук `HandleNPCTeamMemberDeath` (`LuaInitNPCs.cpp:2633`, скрипт Overhead.lua — смерть NPC) и `HandlePlayerTeamMemberDeath` (`LuaInitNPCs.cpp:2655` — смерть наёмника игрока) объявлены, но в этой ветке их вызовы из `Overhead.cpp` отсутствуют — мёртвые хуки.*

---

## 8. Временные события (Strategic Event Handler — ежедневно/почасово)

Скрипт: `scripts\StrategicEventHandler.lua`. Хук: `LetLuaHandleEarlyMorningEvents(UINT8 Init)` (`LuaInitNPCs.cpp:8230`).

| Имя функции Lua | Вызывающий C++ файл и метод | Игровое событие / Условие вызова | Описание (Зачем вызывается) |
| :--- | :--- | :--- | :--- |
| `HandleEarlyMorningEvents` | Strategic\Strategic Event Handler.cpp / `HandleEarlyMorningEvents` (828) → `LetLuaHandleEarlyMorningEvents(0)` | Наступление «раннего утра» игрового дня (ежедневный тик) | Ежедневная скриптовая логика: расписание кампании, события дня |
| `HandlePossiblyDamagedPackage` | Strategic\Strategic Event Handler.cpp:523 → `LetLuaHandleEarlyMorningEvents(1)` (`LuaInitNPCs.cpp:8253`) | Закомментировано в вызывающем коде | Проверка посылки (квест с посылкой Пабло) на повреждение при доставке |
| `AddSecondAirportAttendant` | Strategic\Strategic Event Handler.cpp:534 → `LetLuaHandleEarlyMorningEvents(2)` (`LuaInitNPCs.cpp:8258`) | Закомментировано | Добавление второго сотрудника аэропорта (после прогресса сюжета) |
| `SetPabloToUnbribed` | Strategic\Strategic Event Handler.cpp:551 → `LetLuaHandleEarlyMorningEvents(3)` (`LuaInitNPCs.cpp:8263`) | Закомментировано | Сброс взятки Пабло: со временем он снова становится невзяточным |
| `CheckForMissingHospitalSupplies` | `LetLuaHandleEarlyMorningEvents(4)` (`LuaInitNPCs.cpp:8268`) | Активного вызова нет | Ежедневная проверка квеста с госпитальными поставками |

---

## 9. Прочие вызовы (инфраструктура)

| Имя функции Lua / ресурс | Вызывающий C++ файл и метод | Условие вызова | Описание |
| :--- | :--- | :--- | :--- |
| `EvalLua(строка)` | Standard Gaming Platform\sgp.cpp / обработчик клавиш окна (642, `#ifdef LUACONSOLE` 470/627) | Интерактивная Lua-консоль (отладка) | Выполняет произвольную Lua-строку в основном состоянии |
| `SaveLuaGlobalToSaveGameFile` / `LoadLuaGlobalFromLoadGameFile` | SaveLoadGame.cpp (3989 / 5570), реализация в LuaInitNPCs.cpp (12894 / 12907) | Сохранение/загрузка игры | Персистенция выбранных Lua-глобальных переменных в сейв |
| `logging` (глобал, чтение) | UndergroundInit.cpp:260, Strategic Mines LUA.cpp:96 | Загрузка скриптов шахт/подземелий | Включение отладочного логирования Lua-подсистем |

---

## 10. Сводка: мёртвые / отключённые хуки (кандидаты на реанимацию)

| Lua-функция | Обёртка (LuaInitNPCs.cpp) | Место отключённого вызова |
| :--- | :--- | :--- |
| `HourlyQuestUpdate` | `LetLuaHourlyQuestUpdate` (2927) | Переключатель `LUA_HOURLY_QUEST_UPDATE` закомментирован в Strategic\Luaglobal.h:26 |
| `CheckFact` | `LuaCheckFact` (2370) | Strategic\Quests.cpp:1142 (закомментировано) |
| `CheckForKingpinsMoneyMissing` | `LuaCheckForKingpinsMoneyMissing` (2419) | Strategic Event Handler.cpp:571 |
| `HandleDelayedItemsArrival` | `LuaHandleDelayedItemsArrival` (2469) | Strategic Event Handler.cpp:419 |
| `HandleNPCDoAction` | `LuaHandleNPCDoAction` (2823) | Tactical\Interface Dialogue.cpp:4514 |
| `HandleNPCSystemEvent` | `LetLuaHandleNPCSystemEvent` (8276) | Strategic Event Handler.cpp:657 |
| `HandleQuestCodeOnSectorEntry` (старый вариант) | `LuaHandleQuestCodeOnSectorEntry` (2520) | Вызовов нет (заменён универсальным хуком) |
| `HandleNPCTeamMemberDeath` | `LuaHandleNPCTeamMemberDeath` (2633) | Overhead.cpp — вызова нет |
| `HandlePlayerTeamMemberDeath` | `LuaHandlePlayerTeamMemberDeath` (2655) | Overhead.cpp — вызова нет |
| `ExecuteStrategicEvent` | `LuaExecuteStrategicEvent` (2677) | Вызовов нет |
| `LoadScr` (per-action скрипты) | `LuaIDScripts` (2743) | Вызовов нет |
| `HandlePossiblyDamagedPackage`, `AddSecondAirportAttendant`, `SetPabloToUnbribed`, `CheckForMissingHospitalSupplies` | через `LetLuaHandleEarlyMorningEvents(1..4)` | Strategic Event Handler.cpp:523/534/551 (закомментировано) |

---

# ЧАСТЬ 2. API, доступный Lua-скриптам (Lua → C++)

## Общая информация

Каждый C++ хук (обёртка из `LuaInitNPCs.cpp`) перед выполнением Lua-скрипта создаёт **свежее Lua-состояние** (`LuaScopeState _LS(true)` или явный `lua_open()`), а затем регистрирует в нём:

1. **`IniFunction(L, bQuests)`** (`LuaInitNPCs.cpp:937`) — ~560 C-функций, сгруппированных по разделам: Briefing Room, Global, Email, Time, Loyalty, NPC, Merc, Tactical, Quests, Items, Map, Music/Video, UB и т.д.
2. **`IniGlobalGameSetting(L)`** (`Strategic\Luaglobal.cpp`) — глобальные переменные-настройки из INI и структур движка.

Всего найдено **576** вызовов `lua_register`, из них активны **564**, закомментированы **12**.

## 1. Глобальное состояние и миссии

| Lua-имя | Реализация (C++) | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `HireMerc` | `l_HireMerc` | 940 | Нанять наёмника программно |
| `SetLaptopBroken` | `l_SetLaptopBroken` | 942 | Пометить ноутбук сломанным |
| `SetEndMission` / `SetStartMission` / `CheckMission` | `l_SetEndMission` / `l_SetStartMission` / `l_CheckMission` | 945–947 | Флаги начала/конца миссии (Briefing Room) |
| `SetGlobalBool` / `SetGlobalTrue` / `SetGlobalFalse` / `SetGlobalVal` | `l_SetGlobalLuaBool` и др. | 951–954 | Запись Lua-глобальных переменных, персистируемых в сейв |
| `CheckGlobalBool` / `CheckGlobalVal` / `GetGlobalVal` | `l_CheckGlobalLuaBool` и др. | 955–957 | Чтение персистируемых Lua-глобальных переменных |
| `ReStartingGame` | `l_ReStartingGame` | 1575 | Флаг перезапуска игры |

## 2. Электронная почта (Laptop)

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `AddPreReadEmail`, `AddEmail`, `AddCustomEmail` | `l_AddPreReadEmail`, `l_AddEmail`, `l_AddCustomEmail` | 961–962, 1577 | Добавить письмо (прочитанное/непрочитанное/кастомное) |
| `AddEmailMercAvailableXML`, `AddEmailMercLevelUpXML`, `AddEmailXML` | `l_AddEmailXML`, `l_AddEmailLevelUpXML`, `l_AddEmailXML2` | 963–965 | Письма о доступности наёмника / повышении уровня (по данным XML) |

## 3. Время и стратегические события

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `GetWorldMinutesInDay`, `GetWorldHour`, `GetWorldTotalMin`, `GetWorldTotalSeconds`, `GetWorldDay`, `GetWorldDayInSeconds`, `GetWorldDayInMinutes` | соответствующие `l_...` | 969–977 | Текущее игровое время в разных форматах |
| `GetFutureDayInMinutes`, `GetMidnightOfFutureDayInMinutes` | 978–979 | Время полуночи будущих дней |
| `DayTime`, `NightTime` | 981–982 | Проверка дневного/ночного времени |
| `AddFutureDayStrategicEvent`, `AddSameDayStrategicEvent`, `AddAdvancedStrategicEvent` | 1272–1273, 1447 | Планирование стратегических событий (таймеры) |
| `GetMercArrivalTimeOfDay` | 1279 | Время прибытия наёмника |
| `PauseGame` | 1435 | Пауза |
| `ExecuteStrategicAIAction` | 971 | Вызов действия стратегического ИИ (Королевы) из скрипта |

## 4. Лояльность городов

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `IncrementTownLoyalty`, `IncrementTownLoyaltyEverywhere`, `DecrementTownLoyalty`, `SetTownLoyalty` | 986–989 | Изменение лояльности |
| `InTownSectorWithTrainingLoyalty` | 990 | Проверка тренировки лояльности в секторе |
| `SetGlobalLoyaltyEvent` | 991 | Установить глобальное событие лояльности |
| `GetTownLoyaltyLiberatedAlready/Started/Rating/Change`, `SetTownLoyalty...` | 1238–1245 | Прямой доступ к полям структур лояльности |
| `GetTownUsesLoyalty`, `SetTownUsesLoyalty` | 1246–1247 | Флаг «город использует лояльность» |
| `AffectAllTownsLoyaltyByDistanceFrom` | 1439 | Изменение лояльности всех городов по расстоянию |
| `AdjustLoyaltyForCivsEatenByMonsters` | 1475 | Лояльность после тварей |
| `CheckIfEntireTownHasBeenLiberated/Lost`, `IsTownUnderCompleteControlByPlayer`, `GetNumberOfWholeTownsUnderControl`, `DidFirstBattleTakePlaceInThisTown` | 1402–1403, 1473, 1476–1477 | Состояние городов |

## 5. NPC-профили и проверки (Check*)

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `CheckNPCSector`, `CheckNPCinSector`, `CheckNPCSectorBool` | 996, 1004, 1026 | Сектор NPC |
| `CheckLastDateSpokenTot` | 997 | Когда последний раз говорили с NPC |
| `CheckNPCData1/Data2`, `SetNPCData1/Data2` | 998–1000, 1074–1075 | Поля `bNPCData` |
| `CheckNPCLife`, `SetNPCLife`, `GetNPCLifeMax` | 999, 1111, 1130 | Здоровье NPC |
| `CheckMoney`, `SetMoney`, `SetMoneyInSoldierProfile`, `SetNPCBalance`, `GetNPCBalance` | 1001, 1077, 1081, 1076, 1131 | Деньги/баланс NPC |
| `CheckCharacterSectorX/Y/Z`, `SetCharacterSectorX/Y/Z` | 1002–1003, 1051, 1078–1080 | Сектор персонажа |
| `CheckMiscFlags1/2`, `SetMiscFlags1/2` | 1005–1006, 1072–1073 | Флаги профилей |
| `CheckMercIsDead`, `MercIsDead`, `IsMercDead`, `IsMercHireable` | 1007–1008, 1056–1057 | Статус наёмника |
| `CheckNPCWounded`, `CheckNPCWoundedByPlayer`, `CheckNPCInOkayHealth`, `CheckNPCBleeding` | 1009–1012 | Состояние здоровья |
| `CheckNPCWithin`, `PythSpacesAway`, `StrategicPythSpacesAway`, `CheckNPCAt` | 1013, 1015, 1043, 1280 | Расстояния/позиции |
| `CheckGuyVisible`, `CheckNPCIsEnemy`, `CheckNPCIsEPC`, `CheckNPCIsRPC` | 1014, 1016, 1018–1019 | Видимость и типы |
| `NPCInRoom`, `NPCInRoomRange`, `PCInSameRoom`, `InARoom` | 1020–1023 | Комнаты |
| `FemalePresent`, `CheckPlayerHasHead` | 1024–1025 | Спецпроверки квестов |
| `AIMMercWithin`, `CheckNPCCowering`, `CheckNPCIsUnderFire`, `NPCHeardShot` | 1027–1030 | Боевые проверки |
| `CheckSoldierSide`, `CheckSoldierNeutral`, `CheckSoldierCivilianGroup`, `CheckSoldierubProfile`, `CheckSoldierInSector`, `CheckSoldierAssignment`, `CheckSoldierActive`, `CheckSoldierAttitude` | 1031–1038, 1052–1053 | Свойства солдат |
| `CheckAction`, `CheckSoldierNoiseVolume`, `CheckSoldierAlertStatus`, `CheckCombatMode` | 1040–1042, 1140 | Действия/тревога |
| `CheckSoldierBodyType`, `GetSoldierBodyType`, `SetSoldierBodyType`, `IS_CIV_BODY_TYPE` | 1161–1163, 1184 | Типы тел |
| `CheckCivGroupHostile`, `SetCivGroupHostile` | 1138, 1151 | Враждебность гражданских групп |
| `CheckForPotentialAddToBattleIncrement`, `CheckFPAddBattleIncrement` | 1039, 1414 | Инкремент подкреплений |
| `WhoIsThere2`, `WhoIs` | 1558, 1560 | Кто находится в клетке |

## 6. Управление солдатами и NPC (мутации)

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `SetMercProfiles`, `AddNPC`, `AddNPCtoSector`, `ChangeNpcToDifferentSector`, `InitialProfile` | 1065–1068, 1585 | Создание/перемещение NPC |
| `SetTDRUsedRecently/RDR/FDR` (+ длинные имена) | 1062–1064, 1069–1071 | Флаги использованных дефолтных реплик |
| `SetAlertStatus`, `SetNoiseVolume`, `SetNoiseGridno`, `SetNewSituation`, `SetNextAction`, `SetNextActionData` | 1082–1087 | Поля AI-состояния |
| `SetProfileFaceData`, `GetProfileFaceData` | 1088, 1128 | Портреты |
| `SetSoldierSide`, `SetSoldierOrders`, `ChangeSoldierTeam`, `ChangeMercPtrsTeam`, `SetSoldierNonNeutral`, `MakeHostile`, `MakeMercPtrsHostile` | 1089–1090, 1035, 1115, 1153, 1160, 1183 | Стороны/враждебность |
| `TriggerNPCRecord`, `TriggerNPCRecordImmediately`, `TriggerNPCWithIHateYouQuote`, `TalkingMenuDialogue` | 1091–1093, 1692 | Запуск диалоговых записей NPC |
| `RecruitRPC`, `RecruitEPC`, `UnRecruitEPC` | 1094–1096 | Найм/увольнение RPC/EPC |
| `NPCGotoGridNo`, `HandleNPCGotoGridNo`, `TeleportSoldier`, `DeadSoldier`, `CancelAIAction`, `RESETTIMECOUNTER`, `InitNewSoldierAnim`, `ActionStopMerc` | 1097–1104, 1106 | Движение/анимации/сброс AI |
| `HandleNPCDoAction`, `HandleNPCClosePanel`, `HandleNPCTriggerNPC` | 1107–1109 | Диспетчер NPC-действий |
| `EnterShopKeeperInterfaceScreen` | 1105 | Открыть магазинный интерфейс |
| `SetMercStatus`, `GetMercStatus`, `SetSalary` | 1110, 1122, 1129 | Статусы/зарплата |
| `TacticalCharacterDialogue`, `TacticalCharacterDialogueWithSpecialEvent` | 1112, 1121 | Реплики в тактике |
| `DeleteMercInventory`, `DeleteSlotMercInventory`, `SoldierGiveItem`, `ProfilGiveItem`, `AddItemToInventory` | 1113–1114, 1118–1120 | Инвентарь |
| `MercProfileSetBIGPOCK2POS`, `MercProfileSetPOCKPOS` | 1116–1117 | Слоты экипировки профиля |
| `SetProfileStrategicInsertionData`, `CheckProfileStrategicInsertionData` | 1123, 1681 | Точка входа в сектор |
| `GetDirection`, `GetNPCGridNo` | 1126–1127 | Направление/клетка NPC |
| `ResetBoxers`, `gubBoxerID`, `BoxerExists`, `AnotherFightPossible` | 1135, 1633, 1375, 1379 | Боксёрский клуб |
| `gTacticalStatus`, `GetTacticalStatusFirstID/LastID`, `TacticalStatusTeamHuman/Active`, `GetPlayerMercID`, `GetMapMercSlotID`, `GetTacticalStatusEnemyInSector` | 1139–1144, 1146–1147, 1357 | Доступ к `TacticalStatus` |
| `SetOffPanicBombs`, `WearGasMaskIfAvailable`, `SetNewSituationMercPtrs`, `EnterTeamCombatMode`, `EnterGroupCombatMode` | 1148–1150, 1152, 1418 | Боевые команды |
| `DeadMercPtrs`, `AnimMercPtrs`, `CheckMercPtrs*` (InCivilianGroup/InActive/Assignment/InSector/ID1SeenID2/Life), `GetMercPtsrProfileID` | 1153–1167 | Итерации по указателям на солдат |
| `MercPtsrAIFlags/NextAction/NextActionData/AbsoluteFinalDestination/StrategicInsertionCode/StrategicInsertionData` | 1169–1174 | AI-поля по указателям |
| `MercPtsrTeleportToSector`, `SetMercPtsrGroupID/SectorX/Y/Z`, `GetMercPtsrSectorX/Y/Z/GroupID` | 1165, 1649–1657 | Стратегические позиции |
| `PlayerMercsInSector`, `GetPlayerMercsInSector`, `PlayerGroupsInSector`, `GetPlayerGroupsInSector`, `PlayerTeamFull`, `NumberOfMercsOnPlayerTeam` | 1176–1179, 1451, 1679 | Отряды игрока |
| `UpdateMercsInSector` | 1181 | Обновить солдат в секторе |
| `SetOffBombsByFrequency`, `SetOffBombs`, `SetOffBombsInGridNo`, `ToggleActionItemsByFrequency`, `ActiveTimedBombExists`, `RemoveAllActiveTimedBombs` | 1188–1190, 1426, 1432–1433 | Бомбы/дистанционные детонаторы |
| `ActionDoor`, `ActionDoorOpen`, `ActionDoorClose`, `ActivateSwitchInGridNo` | 1191–1193, 1427 | Двери/выключатели |
| `FindSoldierByProfileID`, `FindProfile`, `FindSoldierTeam`, `ClosestPC`, `TileIsOutOfBounds`, `TileIsOutOfBoundsClosestPC`, `SoldierTo3DLocationLineOfSightTest` | 1044–1050 | Поиск солдат/LOS |
| `FindItemSoldier`, `FindItemProfile`, `FindItemSoldierAddItemToPool` | 1048–1049, 1059 | Поиск предметов у солдат |

## 7. Квесты и факты (Quests)

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `CheckFact` | `l_CheckFact` | 1202 (+дубли 2440, 2490, 2541, 2845, 8298) | Проверка факта (ядро NPC-логики) |
| `SetFactTrue`, `SetFactFalse`, `gubFact`, `GetgubFact` | 1198–1210 | Изменение фактов |
| `CheckQuest`, `gubQuest`, `SetQuest` | 1203, 1211–1213 | Состояние квестов |
| `StartQuest`, `EndQuest` | 1206–1207 | Запуск/завершение квестов из скрипта |
| `GiveQuestRewardPoint` | 1212 | Очки за квесты |
| `GetTimeQuestWasStarted` | 1482 | Время старта квеста |
| `SetHistoryFact`, `ResetHistoryFact`, `AddHistoryToPlayersLog`, `AddFilesToPlayersLog` | 1271, 1275–1276, 1663 | Журнал/история |
| `AddTransactionToPlayersBook` | 1270 | Финансовые записи |
| `InitCreatureQuest` | 1441 | Старт квеста тварей |

## 8. Предметы и мир (Items/Map)

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `CreateItem`, `CreateMoney` | 1217–1218 | Создать предмет/деньги |
| `SetWorldItemsExists`, `GetWorldItemsExists`, `GetWorldItemsObjectItem`, `GetWorldItemsObjectDataMoney`, `MoveItemPools`, `ItemExistsAtLocation`, `ItemTypeExistsAtLocation` | 1219–1225 | Пулы предметов мира |
| `AddCreateItemToPool`, `AddCreateItemsToUnLoadedSector`, `GuaranteeAtLeastXItemsOfIndex`, `GuaranteeAtMostNumOfItemsForItem` | 1226–1227, 1407–1408 | Добавление предметов (в т.ч. в незагруженные сектора) |
| `GetNumberOfWorldItemsFromTempItemFile` | 1224 | Временные файлы карт |
| `SetRender`, `ConvertGridNoToXY`, `AddStructToHead`, `RemoveStruct`, `ApplyMapChangesToMapTempFile` | 1231–1235 | Геометрия карты |
| `RemoveGraphicFromTempFile`, `AddRemoveObjectToUnLoadedMapTempFile`, `RemoveStructFromUnLoadedMapTempFile`, `AddObjectToUnLoadedMapTempFile`, `AddStructToUnLoadedMapTempFile` | 1250–1254 | Правки незагруженных карт |
| `Add3X3Pit`, `Add5X5Pit`, `SearchForOtherMembersWithinPitRadiusAndMakeThemFall`, `TogglePressureActionItemsInGridNo` | 1258–1261 | Ямы/ловушки |
| `SectorEnemyControlled`, `SectorEnemy`, `SECTOR`, `CALCULATE_STRATEGIC_INDEX`, `GetSectorInfoBloodCats` | 1236, 1454, 1486–1489, 1492 | Статусы секторов |
| `WorldLevelDataTerrainID`, `WorldLevelDataMapElementRevealed` | 1269, 1670 | Данные мира |
| `MakeNoise`, `SetPublicNoiseVolume`, `SetPublicNoiseGridNo` | 1268, 1277–1278 | Шумы |
| `AddExitGridToWorld` | 1667 | Выходные сетки |
| `FindUnderGroundSector`, `AddEnemyToUnderGroundSector`, `FindUnderGroundSectorVisited`, `UnderGroundSectorVisited` | 1562–1564, 1377 | Подземелья |
| `AddAlternateSector`, `AddAltUnderGroundSector`, `AddAltSectorNew`, `AddAltUGSectorNew` | 1311–1316 | Альтернативные сектора (моды) |

## 9. Госпиталь, шахты, вертолёт, спецквесты

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `HospitalTempBalance`, `HospitalRefund`, `HospitalPriceModifier` | 1263–1265 | Данные госпиталя (квест поставок) |
| `PlayerTeamSawJoey` | 1266 | Флаг «видели Джоуи» |
| `SetUpHelicopterForPlayer` | 1267 | Выдача вертолёта |
| `IssueHeadMinerQuote`, `GetHeadMinersMineIndex`, `PlayerSpokeToHeadMiner`, `SpokenToHeadMiner`, `GetIdOfMineForSector` | 1365–1367, 1489–1490 | Главные шахтёры |
| `IsHisMineRunningOut/Empty/Disloyal/Infested/LostAndRegained/AtMaxProduction`, `HasHisMineBeenProducingForPlayerForSomeTime`, `ResetQueenRetookMine`, `PredictDailyIncomeFromAMine` | 1368–1374, 1378, 1380 | Состояние шахт |
| `CurrentPlayerProgressPercentage` | 1376 | Прогресс игрока |
| `CountNumberOfBobbyPurchasesThatAreInTransit`, `CheckForNewShipment` | 1358–1359 | Заказы Bobby R's |
| `NumMalesPresent`, `NumWoundedMercsNearby`, `NumMercsNear`, `CheckTalkerStrong/Female/UnpropositionedFemale` | 1360–1364, 1381 | Проверки для диалогов |
| `UpdateAndDamageSAMIfFound`, `DoesSAMExistHere`, `UpdateSAMDoneRepair` | 1429–1431 | SAM-сайты |

## 10. Музыка, звук, видео, экраны

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `PlayJA2Sample` | 1320 | Звуковой сэмпл |
| `SetMusicMode`, `MusicPlay`, `MusicIdPlay`, `AddMusic`, `SetMusicID`, `GetMusicID`, `MusicSetVolume`, `MusicGetVolume` | 1321–1331 | Управление музыкой (для Music.lua) |
| ~~`MusicStop`, `MusicFadeOut`, `MusicFadeIn`~~ | 1332–1334 | Закомментированы |
| `SetSoundEffectsVolume`, `SetSpeechVolume`, `GetSoundEffectsVolume`, `GetSpeechVolume` | 1335–1338 | Громкость |
| `DisplaySirtechSplashScreen`, `SetEnteringMapScreen`, `SetIntroScreenExit`, `SetDoneWithSplashScreen`, `SetIntroExitScreen`, `SetIntroScreenMode`, `SetVideo`, `StopVideo`, `StartVideo` | 1385–1393 | Splash/интро (для Intro.lua) |
| `FadeOutGameScreen`, `FadeInGameScreen` | 1464–1466 | Затенение экрана |
| `SetPendingNewScreen`, `InternalLocateGridNo`, `InternalLeaveTacticalScreen`, `ChangeSelectedMapSector` | 1662, 1606/1626, 1642, 1640 | Смены экранов |
| `SetScreenMsg` | 1700 | Текстовые сообщения |

## 11. Города и стратегическое перемещение

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `VisibleTown`, `HiddenTown`, ~~`EraseTown`, `ResizeTown`~~ | 1397–1400 | Видимость городов (*EraseTown/ResizeTown закомментированы*) |
| `GetTownIdForSector`, `GetCharacterTown`, `SetCharacterTown` | 1401, 1248–1249 | Принадлежность городов |
| `SetGroupSectorValue`, `SetGroupNextSectorValue`, `PlaceGroupInSector`, `GetGroup` | 1461–1462, 1635, 1659 | Перемещение групп |
| `SetCurrentWorldSector` (×2: 1468, 1566), `EnterSector`, `EnterTacticalInFinalSector`, `UB_EnterTacticalInFinalSector` | 1468/1566, 1470, 1570–1572 | Вход в сектора |
| `SetTheFirstBattleSector` | 1474 | Сектор первой битвы |
| `SetDefaultArrivalSector/GridNo/X/Y`, `GetDefaultArrivalSector...` | 1579–1583, 1349–1353 | Стартовая точка прибытия |
| `GetStartingCashNovice/Experienced/Expert/Insane`, `GetFirstArrivalDelay`, `SetFirstArrivalDelay` | 1344–1348, 1351 | Стартовые параметры сложности |
| `SetInvestigateSector`, `CheckInvestigateSector` | 1342–1343 | Сектор-цель ИИ |
| `SetTurnTimeLimit` | 1494 | Лимит хода |

## 12. Стартовая инициализация и UB-хелперы (JA2-5 Unfinished Business)

| Lua-имя | Реализация | Строка | Описание |
| :--- | :--- | :--- | :--- |
| `InitialHeliGridNo1..7`, `InitialJerryGridNo`, `InitialLaptopQuest`, `InitialHeliCrash`, `InitialJerryQuotes`, `InitialJerry`, `InitialHeli` | 1589–1604 | Установка стартовых позиций вертолёта/Джерри и флагов (вызываются из GameInit.lua) |
| `UB_Initial*` (дубли тех же функций) | 1609–1624 | UB-варианты тех же сеттеров |
| `UB_GetManuelID/BiggensID/JohnID/TexID/StogieID/GastonID/JerryID/Pgmale4ID/BettyID/RaulID/MorrisID/RudyID` | 1503–1514 | ID персонажей UB |
| `Ja25JohnKulbaIsInGame`, `Ja25JohnKulbaInitialSectorX/Y`, `UB_JohnKulba...` | 1517–1520, 1537–1540 | Джон Кульба (UB) |
| `SetNumberJa25EnemiesInSurfaceSector`, `SetNumberOfJa25BloodCatsInSector`, `UB_SetNumber...` | 1522–1526, 1542–1544 | Количества врагов/кровокотов (UB) |
| `HasNpcSaidQuoteBefore`, `UB_HasNpcSaidQuoteBefore` | 1526–1546 | Повторы реплик |
| `ShouldThePlayerStopWhenWalkingOnBiggensActionItem`, `HandleSeeingPowerGenFan`, `HandleSwitchToOpenFortifiedDoor`, `HandleSeeingFortifiedDoor`, `HandlePlayerHittingSwitchToLaunchMissles`, `HavePersonAtGridnoStop` (+ `UB_` дубли) | 1528–1534, 1550–1554 | Спец-действия UB-кампании |
| `InitMercFace`, `AddProfileToMap`, `SetKeyProfile`, `CreateKeyProfInv`, `CreateKeyAndAddKeyToPool`, `CreateKeyProfInvAndAddKeyToPool` | 1496–1501, 1674–1677 | Лица и ключи |

## 13. Константы Action Items

`ACTION_ITEM_OPEN_DOOR` (1284), `ACTION_ITEM_CLOSE_DOOR` (1285), ~~`ACTION_ITEM_BLOW_UP` (1286)~~, `ACTION_ITEM_SMALL_PIT` (1287), `ACTION_ITEM_LARGE_PIT` (1288), `ACTION_ITEM_TOGGLE_DOOR` (1289), `ACTION_ITEM_TOGGLE_ACTION1..4` (1290–1293), `ACTION_ITEM_ENTER_BROTHEL` (1294), `ACTION_ITEM_EXIT_BROTHEL` (1295), `ACTION_ITEM_KINGPIN_ALARM` (1296), `ACTION_ITEM_SEX` (1297), `ACTION_ITEM_REVEAL_ROOM` (1298), `ACTION_ITEM_LOCAL_ALARM` (1299), `ACTION_ITEM_GLOBAL_ALARM` (1300), `ACTION_ITEM_KLAXON` (1301), `ACTION_ITEM_UNLOCK_DOOR` (1302), `ACTION_ITEM_TOGGLE_LOCK` (1303), `ACTION_ITEM_UNTRAP_DOOR` (1304), `ACTION_ITEM_TOGGLE_PRESSURE_ITEMS` (1305), `ACTION_ITEM_MUSEUM_ALARM` (1306), `ACTION_ITEM_BLOODCAT_ALARM` (1307).

## 14. Отключённые (закомментированные) регистрации — всего 12

| Lua-имя | Строка | Примечание |
| :--- | :--- | :--- |
| `CheckIfMercIsNearNPC` | 1017 | Заменён другими проверками близости |
| `CheckForMissingHospitalSupplies` (в блоке 1262) | 1262 | Перерегистрируется позже |
| `ACTION_ITEM_BLOW_UP` | 1286 | Убран из Lua |
| `MusicStop`, `MusicFadeOut`, `MusicFadeIn` | 1332–1334 | Музыкальные фейды недоступны скриптам |
| `EraseTown`, `ResizeTown` | 1399–1400 | Редактирование городов запрещено |
| `ActionInProgress` | 1421 | — |
| `MakeCivHostile` | 1422 | Используйте `MakeHostile` |
| `CheckForKingpinsMoneyMissing` (в блоке 1449) | 1449 | Перерегистрируется позже |
| `ExecuteTacticalTextBox` | 1629 | — |

## 15. Карта Lua-скриптов (что чем вызывается из C++)

| Скрипт (`scripts\...`) | Хук-обёртка (LuaInitNPCs.cpp) | C++ точка входа |
| :--- | :--- | :--- |
| `GameInit.lua` | `LetLuaGameInit(0/1/2)` | Game Init.cpp (`InitNewGame`, `InitNPCs`, `InitCustomStrategicLayer`) |
| `InitStrategicLayer.lua` | `LuaInitStrategicLayer` | Game Init.cpp (JA25 UB) |
| `strategicmap.lua` | `LuaHandleQuestCodeOnSector` (+ отключённые `LuaCheckForKingpinsMoneyMissing`, `LuaHandleDelayedItemsArrival`) | strategicmap.cpp; Strategic Event Handler.cpp |
| `Quests.lua` | `LuaInternalQuest` (+ отключённый `LuaCheckFact`) | Quests.cpp |
| `Overhead.lua` | `LetLuaMyCustomHandleAtNewGridNo` (+ мёртвые `LuaHandleNPCTeamMemberDeath`, `LuaHandlePlayerTeamMemberDeath`) | Tactical\Overhead.cpp |
| `InterfaceDialogue.lua` | `LetLuaInterfaceDialogue` (+ отключённый `LuaHandleNPCDoAction`) | Tactical\Interface Dialogue.cpp |
| `HandleNPCDoAction\NNN.lua` | `LuaIDScripts` (мёртв) | — |
| `ExplosionControl.lua` | `LetLuaPerformItemAction` | TileEngine\Explosion Control.cpp |
| `HourlyUpdate.lua` | `LetLuaHourlyQuestUpdate` | Strategic\Hourly Update.cpp |
| `StrategicEventHandler.lua` | `LetLuaHandleEarlyMorningEvents` (+ мёртвый `LetLuaHandleNPCSystemEvent`) | Strategic\Strategic Event Handler.cpp |
| `StrategicTownLoyalty.lua` | `LetHandleLoyaltyChangeForNPCAction`, `LuaHandleGlobalLoyaltyEvent` | Strategic\Strategic Town Loyalty.cpp |
| `GameEventHook.lua` | `LuaExecuteStrategicEvent` (мёртв) | — |
| `Music.lua` | `LetLuaMusicControl` | Init.cpp |
| `Intro.lua` | `LuaIntro` | Intro.cpp |
| `MakeMapsOnHardDrive.lua` | `LetLuaMakeBadSectorListFromMapsOnHardDrive` | mapscreen.cpp, SaveLoadScreen.cpp, SaveLoadGame.cpp |
| `initmines.lua` | `LuaMines::LoadScript` (luaL_dostring) | Strategic Mines LUA.cpp |
| `initunderground.lua`, `<lang>\undergroundsectornames.lua` | `LuaUnderground::RunScript` | UndergroundInit.cpp |

## 16. Важные особенности жизненного цикла (для моддеров)

1. **Каждый хук = новое состояние.** Обёртки на базе `LuaScopeState _LS(true)` создают временное `lua_State`, загружают скрипт заново (`EvalFile`), регистрируют `IniFunction` + `IniGlobalGameSetting`, вызывают нужную функцию и уничтожают состояние. Глобальные Lua-переменные между вызовами **не сохраняются**, кроме явно персистируемых через `SaveLuaGlobalToSaveGameFile` / `LoadLuaGlobalFromLoadGameFile`.
2. **Исключения.** Ошибки скриптов пробрасываются как SGP-исключения (`SGP_THROW_IFFALSE`), старые хуки возвращают `false` при ошибке `luaL_dostring` (ошибка молча игнорируется).
3. **Объектные классы** (`ja2_SoldierClass`, `ja2_SectorClass`, `ja2_MobileGroup`, `ja2_EnvironmentClass`) регистрируются только в долгоживущем состоянии `InitializeLua()` (lua\lua.cpp:179, вызывается из `InitOverhead`) — они доступны в консоли (`EvalLua`, sgp.cpp:642, `#ifdef LUACONSOLE`), но **не** в краткоживущих хуках `LuaScopeState`.
4. **Дубликаты регистраций** (`CheckFact`, `CheckForMissingHospitalSupplies`, `CheckForKingpinsMoneyMissing`, `SetCurrentWorldSector`, `GetDefaultArrivalSectorX/Y`, `CheckSoldierNeutral`) безвредны: последнее `lua_register` просто перезаписывает глобал.

---

# ЧАСТЬ 3. Глобальные переменные, объектные классы и подробности подсистем

## 1. Глобальные переменные Lua, устанавливаемые из C++ (`IniGlobalGameSetting`, Strategic\Luaglobal.cpp)

Функция `IniGlobalGameSetting(lua_State *L)` выполняется перед **каждым** вызовом Lua-хука и пушит в Lua **217 глобальных переменных** — настройки из `ja2_options.ini`, параметров новой игры и текущего состояния движка. Скрипты читают их как обычные глобалы.

### Настройки новой игры (префикс `new`)

| Lua-глобал | Источник (C++) | Описание |
| :--- | :--- | :--- |
| `newGAME_STYLE` | `gGameOptions.ubGameStyle` | Стиль игры (оригинал / мод-стиль) |
| `newDIFFICULTY_LEVEL` | `gGameOptions.ubDifficultyLevel` | Уровень сложности (1–4) |
| `UB_difficultyLevel` (JA2UB) | `gGameOptions.ubDifficultyLevel` | Дубль для UB-кампании |

### Настройки ja2_options.ini (префикс `ini`) — выборочно, ключевые

| Lua-глобал | Источник | Описание |
| :--- | :--- | :--- |
| `iniENABLE_CREPITUS` | `gGameExternalOptions.fEnableCrepitus` | Включены ли Crepitus |
| `iniENEMY_INVESTIGATE_SECTOR` | `gfInvestigateSector` | Исследование секторов врагом |
| `iniMERC_WEBSITE_IMMEDIATELY_AVAILABLE` | `fMercDayOne` | A.I.M.-сайт доступен с первого дня |
| `iniCHANCE_TONY_AVAILABLE` | `ubChanceTonyAvailable` | Шанс доступности Тони |
| `iniSTARTING_CASH_NOVICE/EXPERIENCED/EXPERT/INSANE` | `iStartingCash*` | Стартовые деньги по сложности |
| `iniCAN_TRUE_CIVILIANS_BECOME_HOSTILE` | `fCanTrueCiviliansBecomeHostile` | Враждебность мирных граждан |
| `iniNEW_MERC_ARRIVAL_LOCATION` | `iInitialMercArrivalLocation` / UB `LOCATEGRIDNO` | GridNo прибытия наёмников |
| `iniDEFAULT_ARRIVAL_SECTOR_X/Y` | `ubDefaultArrivalSectorX/Y` | Стартовый сектор прибытия |
| `iniDEFAULT_END_SECTOR_X/Y/Z` | UB-конфиг | Финальный сектор кампании |
| `iniINITIALHELIGRIDNO1..7` | UB-конфиг | Клетки высадки из вертолёта |
| `iniJERRYGRIDNO`, `iniJERRY`, `iniJERRYQUOTES` | UB-конфиг | Стартовые данные Джерри |
| `iniLAPTOP_QUEST`, `iniINGAMEHELICRASH`, `iniINGAMEHELI` | UB-конфиг | Квест ноутбука, состояние вертолёта |
| `iniHIDEOUT_SECTOR_X/Y/Z`, `iniHIDEOUT_EXIT`, `iniHIDEOUT_SURFACE_*`, `iniHIDEOUT_ENTRY` | UB-конфиг | Убежище (UB) |
| `iniBASEMENT_ENTRY_1..10`, `iniBASEMENT_EXIT_1..12` | UB-конфиг | Точки входа/выхода подвала (UB) |
| `iniFINAL_CRATE_GRID`, `iniCRATE_TILE_DEF`, `iniTRAPDOOR_TILE_DEF` | UB-конфиг | Финальный ящик, тайлы люка (UB) |
| `iniPORN_SHOP_ROOM_*`, `iniHANS_POSTION` | UB-конфиг | Комнаты секс-шопа Ханса/Бренды/Тони (UB) |
| `iniANGELS_LEATHERSHOP`, `iniDOOR_TO_*_ROOM` | UB-конфиг | Магазин и двери борделя (UB) |
| `iniFIRST/LAST_ROOM_IN_BROTHEL`, `iniBROTHEL_GUARD_ROOM`, `iniBROTHEL_DOOR_1..3` | UB-конфиг | Геометрия борделя (UB) |
| `iniKINGPIN_MONEY_SECTOR_X/Y/Z` | UB-конфиг | Сектор денег Кингпина |
| `iniOMERTA_DROPOFF_X/Y/Z/GRIDNO` | UB-конфиг | Точка высадки в Омertas (UB) |
| `iniROCKETRIFLE_POSITION` | UB-конфиг | Позиция ракетной винтовки (UB) |
| `iniHOSPITAL_SECTOR_X/Y/Z` | UB-конфиг | Сектор госпиталя |
| `iniADD_MADLAB_SECTOR_1..4_X/Y` | UB-конфиг | Сектора лабораторий Мадлаба (UB) |
| `iniWHICH_MINE_SHUTS_DOWN` | `gGameExternalOptions.bWhichMineRunsOut` (пушится отдельно в `LuaMines::InitializeMines`) | Какая шахта истощается |
| `TestUB` | флаг тестового UB-режима | Отладка UB |

### Текущее состояние движка (без префикса)

| Lua-глобал | Источник | Описание |
| :--- | :--- | :--- |
| `giNPCReferenceCount`, `gubWaitingForAllMercsToExitCode` | одноимённые переменные | Счётчики движка |
| `gWorldSectorX/Y/Z`, `gbWorldSectorZ` | `gWorldSectorX` и др. | Текущий сектор мира |
| `cHour`, `cDay`, `cMin`, `guiDay`, `guiHour`, `guiMin` | Game Clock | Игровое время |
| `gbPlayerNum` | номер игрока (MP) | Мультиплеер |
| `is_networked` | флаг сетевой игры | Мультиплеер |
| `gubMusicMode`, `uiMusicHandle`, `fMusicPlaying`, `gfForceMusicToTense` | Music Control | Состояние музыки |
| `giHospitalTempBalance`, `gbHospitalPriceModifier`, `giHospitalRefund` | Hospital | Квест госпиталя |
| `gfBoxersResting`, `gubBoxersRests`, `gubBoxingMatchesWon` | Boxing | Бокс |
| `guiHelicopterSkyriderTalkState`, `guiTimeOfLastSkyriderMonologue` | Map Screen Helicopter | Диалоги Скайрайдера |
| `gubBloodcatLairSectorId`, `giLairID`, `gubSrcSoldierProfile` | логово кровокотов | Спецквесты |
| `guiNumWorldItems` | World Items | Число предметов в мире |
| `gfEnteringMapScreen`, `guiIntroExitScreen`, `gfIntroScreenExit`, `gbIntroScreenMode`, `gfDoneWithSplashScreen`, `guiCurrentScreen` | экраны | Состояния UI |
| `sSelMapX`, `sSelMapY`, `iCurrentMapSectorZ` | Map Screen | Выбранный сектор на карте |
| `gameStyle`, `enableCrepitus`, `difficultyLevel`, `startingCash*`, `fMercDayOne`, `fCanTrueCiviliansBecomeHostile`, `gfInvestigateSector` | легаси-дубли настроек | Старые имена (без префиксов) |
| `ubDefaultArrivalSectorX/Y`, `iFirstArrivalDelay` | легаси | Прибытие наёмников |
| `enabledJohnAndTex`, `RandomManuelText` (+ `UB_*` дубли) | UB-конфиг | Джон и Текс (UB) |
| `itemBARRETT_UB`, `itemCALICO_960_UB`, `itemPSG1_UB`, `itemL85_UB`, `itemTAR21_UB`, `itemVAL_SILENT_UB`, `itemMICRO_UZI_UB`, `itemCALICO_950_UB`, `itemCALICO_900_UB`, `itemCLIP_CANNON_BALL`, `itemMERC_UMBRELLA`, `itemHAND_CANNON`, `itemHARTFORD_6_SHOOTER`, `itemTEX_MOVIE_*`, `itemLAPTOP_TRANSMITTER`, `itemCHE_GUEVARA_CANTEEN`, `itemMERC_WRISTWATCH`, `itemSAM_GARVER_COMBAT_KNIFE`, `itemMERC_UMBRELLA_OLD`, `itemMORRIS_INSTRUCTION_NOTE` (+ `UB_item*` дубли) | индексы предметов из XML | ID уникальных предметов UB |

## 2. Объектные классы долгоживущего Lua-состояния (InitializeLua)

Регистрируются один раз при `InitializeLua()` (lua\lua.cpp:179) — из `InitOverhead`. Предназначены для Lua-консоли и интерактивного управления.

### `ja2_SoldierClass` (lua\lua_tactical.cpp) + глобальная таблица `Soldiers`

Массив `Soldiers[0..TOTAL_SOLDIERS-1]` оборачивает все `MercPtrs`. Атрибуты и методы:

| Имя | Тип | C++ реализация | Описание |
| :--- | :--- | :--- | :--- |
| `name` | get | `LuaGetSoldierName` | Короткое имя солдата |
| `fullname` | get | `LuaGetSoldierFullName` | Имя из профиля (`gMercProfiles[].zName`) |
| `grid` | get/set | `LuaGet/SetSoldierGrid` | Текущая клетка; сеттер телепортирует (`TeleportSoldier`) |
| `level` | get/set | `LuaGet/SetSoldierLevel` | Уровень (0 пол, 1 крыша); через `HandlePlacingRoofMarker` + `SetSoldierHeight` |
| `APs` | get/set | `LuaGet/SetSoldierAPs` | Очки действия |
| `walkto(grid)` | метод | `LuaSoldierWalkTo` | Пешком к клетке: `aiData.bAction = AI_ACTION_WALK`, `ExecuteAction` |
| `runto(grid)` | метод | `LuaSoldierRunTo` | Бегом: `AI_ACTION_RUN`, `ExecuteAction` |
| `changestance(stance)` | метод | `LuaSoldierChangeStance` | Смена позы (`ChangeSoldierStance`) |

Пример: `Soldiers[3].walkto(10234)`.

### `ja2_SectorClass` (lua\lua_strategic.cpp) + глобальная таблица `Sectors[0..255]`

Оборачивает `SectorInfo[]`. Атрибуты get/set:

| Имя | Описание |
| :--- | :--- |
| `numadmins` | Число администраторов в секторе |
| `numtroops` | Число regular-войск |
| `numelites` | Число элит |
| `numcreatures` | Число тварей |

Сеттеры валидируют диапазон 0–255. Пример: `Sectors[SECTOR(H8)].numtroops = 20`.

### `MobileGroups` (класс + статические методы, lua\lua_strategic.cpp)

| Имя | Тип | Описание |
| :--- | :--- | :--- |
| `MobileGroups.grouplist` | get (static) | Таблица ID всех активных групп (`gpGroupList`) |
| `MobileGroups.get(id)` | метод (static) | Объект группы по ID (`GetGroup`) |
| `MobileGroups.new(sector, admins, troops, elites)` | метод (static) | Создать группу врагов (`CreateNewEnemyGroupDepartingFromSector`) |
| `obj.ID` / `obj.X` / `obj.Y` / `obj.Z` | get | Позиция группы |
| `obj.waypoints` | get | Массив таблиц `{x=, y=}` — все путевые точки |
| `obj.numadmins` / `numtroops` / `numelites` | get | Состав вражеской группы (`pEnemyGroup`) |
| `obj.addwaypoint(x, y)` | метод | Добавить путевую точку (`AddWaypointToPGroup`) |

### `Environment` (lua\lua_env.cpp, глобальный объект)

| Имя | Тип | Описание |
| :--- | :--- | :--- |
| `Environment.rain` | get/set | `guiEnvWeather`; set 0 = `EnvEndRainStorm`, 1–2 = `EnvBeginRainStorm(level)` |
| `Environment.ambient` | get/set | `gubEnvLightValue` (0–255, освещение); вне подвала/пещер поднимает `gfDoLighting` |
| `Environment.nightlights(bool)` | метод | `TurnOn/OffNightLights` |
| `Environment.primelights(bool)` | метод | `TurnOn/OffPrimeLights` |

Также регистрируется тип `wstring` (lua\lwstring.cpp) — широкосимвольные строки для Lua.

## 3. Подсистема шахт: протокол обмена с `initmines.lua` (Strategic Mines LUA.cpp)

Собственное долгоживущее Lua-состояние (`lua_open()` в конструкторе `LuaMines`). Скрипт `scripts\initmines.lua` загружается один раз (`LoadScript`, `luaL_dostring`), а функции сохраняются в реестр через `luaL_ref` и вызываются повторно через `lua_rawgeti`:

| Lua-функция | C++ вызов | Параметры | Возврат |
| :--- | :--- | :--- | :--- |
| `InitializeMines` | `LuaMines::InitializeMines` (137) — из Strategic Mines.cpp / `InitializeMines` (Game Init.cpp:468, mapscreen.cpp:7844) | читает глобалы `newDIFFICULTY_LEVEL`, `iniWHICH_MINE_SHUTS_DOWN` | массив таблиц: `{Location="H8", Type=0|1, MaxRemovalRate, RemainingOreSupply, Infectible, AssociatedUnderground={"I1",...}}` |
| `InitializeHeadMiners` | `LuaMines::InitializeHeadMiners(ubThisMine)` (291) — из strategicmap.cpp:2712 при входе в сектор шахты | `(mines={ {MaxRemovalRate=..}, ...}, currentMine=1-based)` | nil (шахтёры ещё не назначены) или массив `{Profile=..., ...}` |
| `logging` (глобал) | читается в `LoadScript` (96) | — | boolean: включить лог в `initmines.log` |

C++ парсит возврат в структуры `MINE_STATUS_TYPE` (`gMineStatus`), `HEAD_MINER_TYPE`, `associatedMineSectors`; `MAX_NUMBER_OF_MINES` обновляется из размера массива. Состояние сохраняется в сейв (`LuaMines::Save/Load`, 439/498; функции-функции `InitMinerFaces` (115) проставляют лица шахтёров, вызывается из Save/Load и из Dialogue Control.cpp:338 — комментарий).

## 4. Подсистема подземелий: протокол обмена с `initunderground.lua` (UndergroundInit.cpp)

Отдельное Lua-состояние `LUA_STATE_STRATEGIC_MINES_AND_UNDERGROUND` (общее с шахтами — нет: шахты имеют своё `m_L`, а подземелья — состояние из `LuaState::GET(LUA_STATE_STRATEGIC_MINES_AND_UNDERGROUND)`; оба инициализируются в `Init.cpp:1485-1488` при `LoadExternalGameplayData`).

| Lua-функция | C++ вызов | Параметры | Возврат |
| :--- | :--- | :--- | :--- |
| `BuildUndergroundSectorList` | `LuaUnderground::InitializeSectorList` (53) — из Campaign Init.cpp / `BuildUndergroundSectorInfoList` (406); пропускается в мультиплеере при сложности 0 | таблица `{difficultyLevel=, gameStyle=}` | массив таблиц секторов: `{location="J9_B1", numAdmins=, numTroops=, numElites=, numBloodcats=, numCreatures=, creatureHabitat=, music=}` |
| `GetLoadscreen` | `LuaUnderground::GetLoadscreen` (143) — из Loading Screen.cpp / `DisplayLoadScreenWithID` (431) | `(sector="J9_B1", minutesInDay)` | 2 строки: путь к картинке + формат (PNG/BMP/…) |
| `GetSectorName` | `LuaUnderground::GetSectorName` (182) — из strategicmap.cpp / `GetSectorIDString` (3582); вызывается часто (UI карты) | `(sector, {visited=bool, creaturesPresent=bool, detailed=bool})` | строка названия в UTF-8 (конвертируется `MultiByteToWideChar`) |
| `logging` (глобал) | читается в `LoadScript` (260) | — | boolean: лог в `initunderground.log` |

Особенности: `RunScript` (274) вручную вырезает UTF-8 BOM (Lua 5.1 его не переваривает), локализованные имена лежат в `scripts\<lang>\undergroundsectornames.lua`. Поле `location` формата `"X9_B1"` разбирается в координаты через `locationStringToCoordinates`.

## 5. Полный текстовый протокол `LuaFunction`-хуков (параметры по вызовам)

| Хук (LuaInitNPCs.cpp) | Lua-функция | Аргументы (по порядку) |
| :--- | :--- | :--- |
| `LetLuaGameInit(Init)` (2601) | `InitNewGame` / `InitNPCs` / `InitStrategicLayer` | без аргументов |
| `LuaInitStrategicLayer` (1860) | `InitStrategicLayer` | без аргументов |
| `LuaHandleQuestCodeOnSector(x,y,z,entry)` (2574) | `HandleQuestCodeOnSectorEntry` / `...Exit` | `(sSectorX, sSectorY, bSectorZ)` |
| `LuaInternalQuest(quest,x,y,history,start?)` (2696) | `InternalStartQuest` / `InternalEndQuest` | `(ubQuest, sSectorX, sSectorY, fUpdateHistory)` |
| `LetLuaMyCustomHandleAtNewGridNo(_, profile, _)` (2720) | `HandleAtNewGridNo` | `(ubProfile)` |
| `LuaHandleNPCTeamMemberDeath(profile)` (2633) | `HandleNPCTeamMemberDeath` | `(ubProfile)` |
| `LuaHandlePlayerTeamMemberDeath(profile)` (2655) | `HandlePlayerTeamMemberDeath` | `(ubProfile)` |
| `LuaExecuteStrategicEvent(...)` (2677) | `ExecuteStrategicEvent` | `(EventCallbackID, EventTimeStamp, EventTimeOffset, EventType, EventFlags, EventParam)` |
| `LetLuaInterfaceDialogue(npc, mode)` (2878) | `CarmenLeavesSectorCallback` / `HandleStuffForNPCEscorted` | 0 аргументов / `(ubNPC)` |
| `LuaHandleNPCDoAction(npc, action, quote, _)` (2823) | `HandleNPCDoAction` | `(ubTargetNPC, usActionCode, ubQuoteNum)` |
| `LuaIDScripts(npc, action, quote)` (2743) | `LoadScr` | `(ubTargetNPC, usActionCode, ubQuoteNum)` |
| `LetLuaPerformItemAction(action, gridno, _)` (2905) | `PerformItemAction` | `(ActionID, sGridNo)` |
| `LetLuaHourlyQuestUpdate(_)` (2927) | `HourlyQuestUpdate` | без аргументов |
| `LetLuaHandleEarlyMorningEvents(mode)` (8230) | `HandleEarlyMorningEvents` (+ отключённые режимы 1–4) | без аргументов |
| `LetHandleLoyaltyChangeForNPCAction(profile, _)` (11545) | `HandleLoyaltyChangeForNPCAction` | `(ubNPCProfileId)` |
| `LuaHandleGlobalLoyaltyEvent(type,x,y,z,_)` (11567) | `HandleGlobalLoyaltyEvent` | `(ubEventType, sSectorX, sSectorY, bSectorZ)` |
| `LetLuaMusicControl(_)` (1703) | `Music` | без аргументов |
| `LuaIntro(mode, video, type, _)` (1724) | `PrepareToExitIntroScreen` / `GetNextIntroVideo` / `SetIntroType` | 0 / `(uiCurrentVideo)` / `(bIntroType)` |
| `LetLuaMakeBadSectorListFromMapsOnHardDrive(_)` (1841) | `MakeBadSectorListFromMapsOnHardDrive` | без аргументов |

*Каждый из этих хуков также регистрирует все C-функции из Части 2 перед вызовом, т.е. скрипт всегда имеет полный API.*

## 6. Полезные замечания для моддинга

1. **Добавить свой хук**: заведите функцию в нужном `.lua`-скрипте и добавьте `LuaFunction(_LS.L, "ИмяФункции").Param<int>(...).Call(n)` в существующую обёртку, либо новую обёртку по образцу `LetLuaPerformItemAction` и вызовите её из C++-кода события. Скрипт будет перечитан заново при каждом вызове — правки подхватываются без перезапуска (удобно для отладки).
2. **Данные для скриптов**: помимо аргументов, скрипт видит 217 глобалов из `IniGlobalGameSetting` (Часть 3.1) — например, ветвление по `newDIFFICULTY_LEVEL` или `iniENABLE_CREPITUS`.
3. **Логи**: `initmines.log`, `initunderground.log` (включаются глобалом `logging = true` в соответствующем скрипте); ошибки `EvalFile` пробрасывают SGP-исключение с именем файла.
4. **Кодировка**: `undergroundsectornames.lua` читается как UTF-8 с ручной вырезкой BOM; названия конвертируются в UTF-16 для движка.
5. **Мультиплеер**: `LuaUnderground::InitializeSectorList` и `LuaMines::InitializeMines` пропускаются, пока `is_networked` и сложность не получена от сервера.

# ЧАСТЬ 4. Компиляционные переключатели, инфраструктура моста и точный статус всех хуков
## 1. Переключатели Lua-систем (Strategic\Luaglobal.h)

Включение/отключение каждого скриптового хука выполняется на этапе компиляции. Активные определения на момент анализа:

```cpp
#define LUA_QUESTS                         // Quests.lua            — ВКЛ
#define LUA_INTERFACE_DIALOGUE             // InterfaceDialogue.lua — ВКЛ
#define LUA_EXPLOSION_CONTROL              // ExplosionControl.lua  — ВКЛ
#define LUA_HANDLE_QUEST_CODE_ON_SECTOR    // strategicmap.lua      — ВКЛ
#define LUA_GAME_INIT_NEW_GAME             // GameInit.lua InitNewGame — ВКЛ
#define LUA_GAME_INIT_NPCS                 // GameInit.lua InitNPCs    — ВКЛ
#define LUA_INTRO                          // Intro.lua             — ВКЛ
//#define LUA_HOURLY_QUEST_UPDATE           // HourlyUpdate.lua     — ВЫКЛ (заккомментировано!)
#define LUA_STRATEGY_EVENT_HANDLER         // StrategicEventHandler.lua — ВКЛ
#define LUA_STRATEGY_TOWN_LOYALTY          // StrategicTownLoyalty.lua  — ВКЛ
#define LUA_OVERHEAD                       // Overhead.lua          — ВКЛ
```

Где проверяется каждый:

| Переключатель | Файл:строка | Что делает при ВКЛ / ВЫКЛ |
| :--- | :--- | :--- |
| `LUA_QUESTS` | Strategic\Quests.cpp:1859, 1892 | ВКЛ: `InternalStartQuest`/`InternalEndQuest` → `LuaInternalQuest` (Lua); ВЫКЛ: C++-логика |
| `LUA_INTERFACE_DIALOGUE` | Tactical\Interface Dialogue.cpp:1747, 5238 | ВКЛ: `HandleStuffForNPCEscorted`/`CarmenLeavesSectorCallback` → Lua; ВЫКЛ: C++ |
| `LUA_EXPLOSION_CONTROL` | TileEngine\Explosion Control.cpp:3275 | ВКЛ: `PerformItemAction` → Lua; ВЫКЛ: C++ |
| `LUA_HANDLE_QUEST_CODE_ON_SECTOR` | Strategic\strategicmap.cpp:2761, 2813 | ВКЛ: вход/выход из сектора → Lua; ВЫКЛ: C++ |
| `LUA_GAME_INIT_NPCS` | Strategic\Game Init.cpp:116 | ВКЛ: `InitNPCs` → Lua `InitNPCs`; ВЫКЛ: огромный C++-блок инициализации профилей |
| `LUA_GAME_INIT_NEW_GAME` | Strategic\Game Init.cpp:726 | ВКЛ: новая игра → Lua `InitNewGame` (+ UB: `InitCustomStrategicLayer`); ВЫКЛ: C++-письма Энрико и стартовые деньги |
| `LUA_INTRO` | Intro.cpp:540, 574, 689 | ВКЛ: интро → Lua; ВЫКЛ: C++-последовательность роликов |
| `LUA_HOURLY_QUEST_UPDATE` | Strategic\Hourly Update.cpp:280 | **ВЫКЛ**: почасовой апдейт квестов выполняется C++-кодом; Lua-ветка неактивна |
| `LUA_STRATEGY_EVENT_HANDLER` | Strategic Event Handler.cpp:827 | ВКЛ: ежедневное «раннее утро» → Lua `HandleEarlyMorningEvents`; ВЫКЛ: большой C++-блок |
| `LUA_STRATEGY_TOWN_LOYALTY` | Strategic Town Loyalty.cpp:1658, 1911 | ВКЛ: лояльность → Lua; ВЫКЛ: C++ |
| `LUA_OVERHEAD` | Tactical\Overhead.cpp:3449 | ВКЛ: `HandleAtNewGridNo` → Lua (с заполнением `PROFILLUA_*`); ВЫКЛ: чистый C++ |

Также существует `LUACONSOLE` (Standard Gaming Platform\sgp.cpp:470, 1236, 1310) — интерактивная Lua-консоль по Alt+`\` (`USE_CONSOLE` = 0 в sgp.cpp:55, т.е. консоль выключена в текущей конфигурации). Обёртки `MakeMapsOnHardDrive`, `Music`, `InitStrategicLayer` (UB), `HandleNPCSystemEvent` переключателями не управляются.

## 2. Точный статус каждого C++→Lua вызова (итоговая таблица)

| Lua-функция | Статус | Путь вызова |
| :--- | :--- | :--- |
| `InitNewGame` | ✅ АКТИВЕН | Game Init.cpp:728 (`LUA_GAME_INIT_NEW_GAME`) |
| `InitNPCs` | ✅ АКТИВЕН | Game Init.cpp:117 (`LUA_GAME_INIT_NPCS`) |
| `InitStrategicLayer` (GameInit.lua, режим 2) | ✅ АКТИВЕН (только JA2UB) | Game Init.cpp:108 `InitCustomStrategicLayer` ← 444, 731 |
| `InitStrategicLayer` (InitStrategicLayer.lua) | ✅ АКТИВЕН (только JA2UB) | Game Init.cpp:497 |
| `MakeBadSectorListFromMapsOnHardDrive` | ✅ АКТИВЕН | mapscreen.cpp:5303, SaveLoadScreen.cpp:2569, SaveLoadGame.cpp:4688 |
| `Music` | ✅ АКТИВЕН | Init.cpp:1482 |
| `PrepareToExitIntroScreen` / `GetNextIntroVideo` / `SetIntroType` | ✅ АКТИВЕН | Intro.cpp:541/578/690 (`LUA_INTRO`) |
| `HandleQuestCodeOnSectorEntry` / `HandleQuestCodeOnSectorExit` | ✅ АКТИВЕН | strategicmap.cpp:2763/2815 (`LUA_HANDLE_QUEST_CODE_ON_SECTOR`) |
| `InternalStartQuest` / `InternalEndQuest` | ✅ АКТИВЕН | Quests.cpp:1860/1893 (`LUA_QUESTS`) |
| `HandleAtNewGridNo` | ✅ АКТИВЕН | Overhead.cpp:3450 (`LUA_OVERHEAD`) |
| `HandleStuffForNPCEscorted` / `CarmenLeavesSectorCallback` | ✅ АКТИВЕН | Interface Dialogue.cpp:1749/5240 (`LUA_INTERFACE_DIALOGUE`) |
| `PerformItemAction` | ✅ АКТИВЕН | Explosion Control.cpp:3280 (`LUA_EXPLOSION_CONTROL`) |
| `HandleEarlyMorningEvents` | ✅ АКТИВЕН | Strategic Event Handler.cpp:828 (`LUA_STRATEGY_EVENT_HANDLER`) |
| `HandleLoyaltyChangeForNPCAction` / `HandleGlobalLoyaltyEvent` | ✅ АКТИВЕН | Strategic Town Loyalty.cpp:1912/1659 (`LUA_STRATEGY_TOWN_LOYALTY`) |
| `BuildUndergroundSectorList` | ✅ АКТИВЕН | Campaign Init.cpp:406 (пропуск в MP при сложности 0) |
| `GetLoadscreen` | ✅ АКТИВЕН | Loading Screen.cpp:431 |
| `GetSectorName` | ✅ АКТИВЕН | strategicmap.cpp:3582 (`GetSectorIDString`) |
| `InitializeMines` | ✅ АКТИВЕН | Strategic Mines.cpp:144 ← Game Init.cpp:468, mapscreen.cpp:7844 |
| `InitializeHeadMiners` | ✅ АКТИВЕН | strategicmap.cpp:2712 |
| `HourlyQuestUpdate` | ❌ ВЫКЛЮЧЕН переключателем | Hourly Update.cpp:281 — `LUA_HOURLY_QUEST_UPDATE` закомментирован (Luaglobal.h:26) |
| `CheckFact` (C++→Lua) | ❌ ЗАКОММЕНТИРОВАН | Strategic\Quests.cpp:1142 |
| `CheckForKingpinsMoneyMissing` (C++→Lua) | ❌ ЗАКОММЕНТИРОВАН | Strategic Event Handler.cpp:571 (C++-версия вызывается на стр. 689) |
| `HandleDelayedItemsArrival` (C++→Lua) | ❌ ЗАКОММЕНТИРОВАН | Strategic Event Handler.cpp:419 (C++-версия работает: вызовы 556, 726) |
| `HandleNPCDoAction` | ❌ ЗАКОММЕНТИРОВАН | Interface Dialogue.cpp:4514 |
| `HandleNPCSystemEvent` | ❌ ЗАКОММЕНТИРОВАН | Strategic Event Handler.cpp:657 (C++ `HandleNPCSystemEvent` вызывается из Game Event Hook.cpp:303 по `EVENT_SET_BY_NPC_SYSTEM`) |
| `HandlePossiblyDamagedPackage` | ❌ ЗАКОММЕНТИРОВАН | Strategic Event Handler.cpp:549 (внутри — вызов Lua закомментирован, работает C++ `#if 0`-логика; вызывается из Game Event Hook) |
| `AddSecondAirportAttendant` | ❌ ЗАКОММЕНТИРОВАН | Strategic Event Handler.cpp:520 (Lua-вызов закомментирован; C++-версия дергается из Game Event Hook.cpp:306 по `EVENT_SECOND_AIRPORT_ATTENDANT_ARRIVED`) |
| `SetPabloToUnbribed` | ❌ ЗАКОММЕНТИРОВАН | Strategic Event Handler.cpp:532 (работает C++-версия из `HandleNPCSystemEvent`) |
| `CheckForMissingHospitalSupplies` | ❌ Lua-вызовов нет | C++-версия на стр. 1101 того же файла |
| `HandleNPCTeamMemberDeath` | ❌ МЁРТВ | Обёртка есть (2633), вызова нет; C++ `HandleNPCTeamMemberDeath` вызывается из Soldier Ani.cpp:4315, но Lua не дергает |
| `HandlePlayerTeamMemberDeath` | ❌ МЁРТВ | Обёртка есть (2655); C++-версия вызывается из Soldier Ani.cpp:4468 (через `HandlePlayerTeamMemberDeathAfterSkullAnimation`) |
| `ExecuteStrategicEvent` | ❌ МЁРТВ | Обёртка есть (2677), вызовов нет |
| `LoadScr` (`LuaIDScripts`) | ❌ МЁРТВ | Обёртка есть (2743), объявлена в LuaInitNPCs.h:83, вызовов нет |

## 3. Контекст текущего профиля (`IniGlobal_0` / `IniGlobal_1`, LuaInitNPCs.cpp:887/916)

Два экспортёра глобальных переменных, заполняемых из `Tactical\Overhead.cpp` (строки 2866–2873, в начале `HandleAtNewGridNo`):

| Lua-глобал | Заполняется из | Значение |
| :--- | :--- | :--- |
| `ProfileIdsSectorX` | `PROFILLUA_sSectorX` = `pSoldier->sSectorX` | Сектор X солдата |
| `ProfileIdsSectorY` | `PROFILLUA_sSectorY` | Сектор Y |
| `ProfileIdbSectorZ` | `PROFILLUA_bSectorZ` | Уровень сектора (подземелье) |
| `ProfileIdLevel` | `PROFILLUA_Level` = `pathing.bLevel` | Земля (0) / крыша (1) |
| `ProfileIdubID` | `PROFILLUA_ubID` | Тактический ID солдата |
| `ProfileIdsGridNo` | `PROFILLUA_sGridNo` | Текущая клетка |
| `ProfileIdubDirectiono` | `PROFILLUA_ubDirectiono` = `ubDirection` | Направление взгляда |
| `ProfileIdbTeam` | `PROFILLUA_bTeam` | Команда |
| `ProfileId` | `PROFILLUA2_ubProfile` (IniGlobal_1) | ID профиля |

⚠️ **Статус**: функции `IniGlobal_0`/`IniGlobal_1` определены, но **нигде не вызываются** в этой ветке (поиск по всему коду даёт только определения). Переменные `PROFILLUA_*` заполняются в `HandleAtNewGridNo`, но глобалы `ProfileId*` в Lua при этом **не попадают** — скрипт `Overhead.lua` получает только аргумент `ubProfile` через `LuaFunction(...).Param<int>(ProfileId)`. Если ваши скрипты опираются на `ProfileIdsSectorX` и т.п. — нужно добавить вызовы `IniGlobal_0(L)`/`IniGlobal_1(L)` в обёртку `LetLuaMyCustomHandleAtNewGridNo`.

## 4. Хранилище персистируемых Lua-переменных (`gLuaGlobal[1000]`, LuaInitNPCs.cpp:872)

| Аспект | Описание |
| :--- | :--- |
| Тип | `LUA_GLOBAL gLuaGlobal[1000]` — массив структур `{BOOLEAN fGlobalLuaBool; INT32 iGlobalLuaVal;}` |
| Инициализация | `IniLuaGlobal()` (стр. 878) — обнуление; вызывается при старте |
| Lua API | `SetGlobalBool(idx, v)`, `SetGlobalTrue/False(idx)`, `SetGlobalVal(idx, v)` — запись; `CheckGlobalBool`, `CheckGlobalVal`, `GetGlobalVal` — чтение (реализации `l_*`, стр. 7336–7417) |
| Сейв | `SaveLuaGlobalToSaveGameFile` / `LoadLuaGlobalFromLoadGameFile` (стр. 12893/12905) — бинарная запись всего массива в сейв (вызовы из SaveLoadGame.cpp:3989/5570) |
| Смысл | Единственный способ сохранить Lua-состояние между вызовами хуков (каждый хук = новое состояние) — скрипты хранят флаги здесь по числовым индексам 0–999 |

## 5. Инфраструктурные классы моста (детально)

### `LuaState` (lua\lua_state.cpp/.h)

| Метод | Описание |
| :--- | :--- |
| `LuaState::INIT(lua::State, bool init_libs)` | Инициализация долгоживущего состояния по ID (`LUA_STATE`, `LUA_STATE_STRUCTURE`, `LUA_STATE_GUI`, `LUA_STATE_STRATEGIC_MINES_AND_UNDERGROUND`) |
| `LuaState::INIT(bool init_libs)` | Создать временное состояние |
| `GET(id)` / `CLOSE(id)` | Получить/закрыть состояние |
| `EvalFile(filename)` | `luaL_loadfile` + `lua_pcall`; при ошибке — SGP-исключение с диагностикой |
| `EvalString(str, len, id)` | `luaL_loadbuffer` + `lua_pcall`; перегрузки ASCII/UTF-16 (конвертация `vfs::String::as_utf8`); частичный ввод (`'<eof>'`) молча возвращается |
| `CallFunction(name)` | Устаревший вызов без параметров: `lua_getglobal` + `lua_pcall` (0 аргументов, 1 результат) |
| `registry()` | Доступ к `Registry` — таблицы `__CallBackRegistry` (SetCallback/GetCallback) и userdata |

### `LuaFunction` (lua\lua_function.h/.cpp) — точная семантика

```cpp
LuaFunction(L, "Name")      // lua_getglobal + проверка lua_isfunction (m_is_function)
  .Param<T>(v)              // lua_pushinteger/number/string/boolean; счётчик аргументов ++
  .TParam(idx, v)           // запись в открытую таблицу (setValue по индексу)
  .TableOpen() / .TableClose()   // вложенные таблицы: lua_newtable / stack pop, counter++
  .Call(num_ret)            // lua_pcall(_L, counter, num_ret, 0); при ошибке SGP_THROW с текстом Lua-ошибки
```
Важно: `Param()` и `Call()` **не работают**, если глобал `Name` не является функцией (`is_function()` проверяет) — вызов тихо пропускается, `Call` вернёт `false`. Перегрузки `Param`: `std::string` (pushstring), `double` (pushnumber), `int` (pushinteger), `unsigned int` (pushnumber как double), `bool` (pushboolean).

### `LuaTable` (lua\lua_table.h/.cpp)

| Конструктор | Смысл |
| :--- | :--- |
| `LuaTable(L)` | Создать новую таблицу (`lua_newtable`) |
| `LuaTable(L, st_off)` | Привязаться к значению на стеке |
| `LuaTable(L, "global")` | Привязаться к глобальной таблице |
| `LuaTable(table, idx)` | Вложенная таблица по строковому/числовому индексу |

Методы: `getValue<T>(index, out)` / `setValue<T>(index, v)` для `std::string`, `double`, `int`, `unsigned int`, `bool` (а также шаблон `getValue<T*>(index)` для userdata-объектов, наследующих `LuaInterface`); `is_valid()`, `length()` (оператор `#`). Используется для парсинга возвратов `BuildUndergroundSectorList` и т.п.

### `LuaInterface` / `LuaUserObject` (lua\lua_class_interface.h/.cpp) — ООП-экспорт C++ классов

Механизм для экспорта C++-классов как Lua-объектов: `DECLARE_LUA_INTERFACE(BaseClass)` / `IMPLEMENT_LUA_INTERFACE(className, luaName, AttribArray)`; `luaExportClass<T>(L)` регистрирует интерфейс, `luaPushObject(L, name)` кладёт объект, `luaGetTypedUserData<T>(L, idx)` безопасно извлекает с проверкой типа (`luaL_typerror`). Метаметоды `__index`/`__newindex` (`luaGetObject`/`luaSetObject`) читают/пишут атрибуты из `LuaAttrib`-списков. ⚠️ В текущей ветке **не используется ни одним игровым классом** (поиск `IMPLEMENT_LUA_INTERFACE` по коду не даёт применений) — инфраструктура готова, но не задействована.

### `LuaAttrib` (lua\Lua Interpreter.h) — дескриптор атрибута класса

```cpp
typedef struct { STR8 name; lua_CFunction get; lua_CFunction set; lua_CFunction call; lua_CFunction staticcall; } LuaAttrib;
```
`get`/`set` — свойства (работают через метаметоды), `call` — метод экземпляра, `staticcall` — статический метод класса. Так описаны `Soldier[]`, `Sector[]`, `MobileGroup[]`, `Environment[]`.

## 6. Lua-консоль (отладка)

| Аспект | Детали |
| :--- | :--- |
| Включение | Компиляция с `LUACONSOLE` + `USE_CONSOLE` (sgp.cpp:55; сейчас `USE_CONSOLE 0` — консоль выключена) |
| Создание | `CreateConsole()` при старте (sgp.cpp:1236); `PollConsole()` на `WM_TIMER` (sgp.cpp:470) |
| Активация | **Alt + `\`** (код 219) — создаёт консоль, выводит «LUA console ready» (sgp.cpp:625) |
| Выполнение | `WM_INPUTREADY` → `EvalLua(wstring)` (sgp.cpp:642) — выполняет строку в **долгоживущем** состоянии `InitializeLua()` |
| Доступно | `Soldiers[...]`, `Sectors[...]`, `MobileGroups`, `Environment`, `wstring` — т.е. все объектные классы Части 3 |

## 7. Замечания к ведению модов

1. **Чтобы включить почасовой Lua-апдейт**: раскомментируйте `LUA_HOURLY_QUEST_UPDATE` в `Strategic\Luaglobal.h` — `HourlyQuestUpdate` начнёт вызывать `scripts\HourlyUpdate.lua` вместо C++-логики (проверьте полноту портирования: в C++-ветке есть ещё `DecayTacticalMoraleModifiers` и др.).
2. **Чтобы реанимировать событийные хуки** (`HandleNPCSystemEvent`, `CheckFact` C++→Lua и т.д.): раскомментируйте соответствующие строки вызовов (см. таблицу статусов) — обёртки полностью рабочие.
3. **Скрипты перечитываются при каждом вызове**: правки в `scripts\*.lua` подхватываются без перезапуска игры, но и производительность (файловое чтение при каждом вызове) стоит учитывать — критичные по частоте хуки (`GetSectorName` вызывается постоянно при прорисовке карты!) работают через долгоживущее состояние подземелий, а не через `LuaScopeState`.
4. **Ошибки Lua не фатальны** в старых хуках (возврат `false` при `luaL_dostring` с TODO-логом), но в новых (`LuaScopeState` + `SGP_THROW_IFFALSE`) пробрасывают исключение, которое ловится в главном цикле (sgp.cpp, `catch(sgp::Exception)`) и показывает окно ошибки.

---

# ЧАСТЬ 5. Разбор игровых Lua-скриптов (из каталога игры)

> Источник: реальная игровая установка `D:\games\JA2_1.13_20.10.2026` — `Data\Scripts` (JA2-кампания), `Data-1.13\Scripts` (1.13-версия), `Data-UB\Scripts` (JA2: Unfinished Business). C++ ищет скрипты по путям `scripts\Xxx.lua` относительно каталога игры; VFS позволяет переопределять их через мод-папки Data-*.

## 0. Обзор: какие скрипты есть, какие функции определяют

| Скрипт | Размер | Определяемые функции (вызываемые из C++) | Хук-обёртка |
| :--- | ---: | :--- | :--- |
| `GameInit.lua` | 16.5 КБ | `InitNPCs()`, `InitNewGame()` | `LetLuaGameInit(1/0)` |
| `InitStrategicLayer.lua` (только UB) | 0.7 КБ | `InitStrategicLayer()` | `LuaInitStrategicLayer` |
| `strategicmap.lua` | 8.0 КБ | `HandleQuestCodeOnSectorEntry(x,y,z)`, `HandleQuestCodeOnSectorExit(x,y,z)` | `LuaHandleQuestCodeOnSector` |
| `Quests.lua` | 6.9 КБ | `InternalStartQuest(quest,x,y,hist)`, `InternalEndQuest(...)` | `LuaInternalQuest` |
| `Overhead.lua` | 10.0 КБ | `HandleAtNewGridNo(profileId)` + локальные `HandleJohnArrival`, `HandleMaryArrival` | `LetLuaMyCustomHandleAtNewGridNo` |
| `InterfaceDialogue.lua` | 4.6 КБ | `HandleStuffForNPCEscorted(npc)`, `CarmenLeavesSectorCallback()` | `LetLuaInterfaceDialogue` |
| `ExplosionControl.lua` | 7.7 КБ | `PerformItemAction(actionId, gridNo)` — диспетчер 24 действий | `LetLuaPerformItemAction` |
| `HourlyUpdate.lua` | 1.2 КБ | `HourlyQuestUpdate()` | `LetLuaHourlyQuestUpdate` (переключатель ВЫКЛ) |
| `StrategicEventHandler.lua` | 21.7 КБ | `HandleEarlyMorningEvents()` | `LetLuaHandleEarlyMorningEvents` |
| `StrategicTownLoyalty.lua` | 10.7 КБ | `HandleGlobalLoyaltyEvent(...)`, `HandleLoyaltyChangeForNPCAction(profile)` + локальный `CheckConditionsForTriggeringCreatureQuest` | `LuaHandleGlobalLoyaltyEvent`, `LetHandleLoyaltyChangeForNPCAction` |
| `Intro.lua` (в UB — `intro.lua`) | 4.3 КБ | `PrepareToExitIntroScreen()`, `GetNextIntroVideo(video)`, `SetIntroType(type)` | `LuaIntro` |
| `Music.lua` | — | отсутствует в установке (хук бросит SGP-исключение «Cannot open file») | `LetLuaMusicControl` |
| `MakeMapsOnHardDrive.lua` (только UB) | 0.9 КБ | `MakeBadSectorListFromMapsOnHardDrive()` — пустая, шаблон в комментарии | `LetLuaMakeBadSectorList...` |
| `initmines.lua` | 10.2 КБ | `InitializeMines()`, `InitializeHeadMiners(mines, currentMine)` | `LuaMines` |
| `initunderground.lua` | 13.1 КБ | `BuildUndergroundSectorList(settings)`, `GetLoadscreen(loc, time)`, `GetSectorName(coords, details)` + локальный `PopulateSectors` | `LuaUnderground` |
| `undergroundsectornames.lua` (+ `Russian.`) | 1.7 КБ | глобальная таблица `sectornames = { ["A10-1"]="Rebel Hideout", ... }` (конкатенируется перед initunderground.lua) | `LuaUnderground::RunScript` |

Отсутствуют: `GameEventHook.lua` (хук `ExecuteStrategicEvent` мёртв), `HandleNPCDoAction\NNN.lua` (хук `LuaIDScripts` мёртв).

## 1. GameInit.lua — инициализация кампании

Структура (530 строк): шапка-документация с примерами моддинга → ~90 local-констант email-записей (адреса в `email.edt` строятся арифметикой: `IMP_EMAIL_INTRO = 0`, дальше суммы длин) → таблицы `Fincances` (31 тип транзакций), `Sender` (отправители), `Modifier` (госпиталь), `SectorY` (строки A–P), `Profil` (Skyrider=97, Micky=96, Gabby=104, Bob=84, Devin=61) → функции.

`InitNPCs()` — расстановка ключевых NPC:
- Скайрайдер — случайно в 1 из 4 секторов (B15/E14/D12/C16) через `AddNPCtoSector` + `AddAltSectorNew` (альтернативная карта сектора); в мультиплеере — всегда B15.
- Секретная лаборатория Мадлаба — случайный сектор из `iniADD_MADLAB_SECTOR_1..4_X/Y`.
- Микки — случайно 1 из 5 секторов (G9/D13/C5/H2/C6).
- Боб и Гэттби — только при `newGAME_STYLE == 1` (SciFi) и `iniENABLE_CREPITUS == 1`.
- Реалистичный стиль: альтернативная карта J9-1 (Тикса без лестницы в пещеры): `AddAltUGSectorNew(9, MAP_ROW_J, 1)`.
- Госпиталь: `HospitalTempBalance(0)`, `HospitalRefund(0)`, `HospitalPriceModifier(HOSPITAL_UNSET)`.
- Девин: `SetNPCData1(Profil.Devin, 3)` — счётчик дней до перемещения; `PlayerTeamSawJoey(false)`.

`InitNewGame()` — стартовые письма (`AddPreReadEmail` OLD_ENRICO_1..3, RIS_REPORT; `AddEmail` IMP_EMAIL_INTRO; MERC_INTRO при `iniMERC_WEBSITE_IMMEDIATELY_AVAILABLE`) и стартовые деньги по сложности → `AddTransactionToPlayersBook(ANONYMOUS_DEPOSIT, ...)`.

UB-версия (925 строк) сильно переработана: `InitStrategicLayer()` — крушение вертолёта (`UB_InitialHeliCrash`), Джерри, сетки высадки (`UB_InitialHeliGridNo1..7`), `SetDefaultArrivalSector`, `UB_InternalLocateGridNo`; `InitNPCs()` — рандомизация сюжета UB (факты 414–418 «кто погиб», Джон Кульба или Текс — `UB_JohnKulbaIsInGame`, расстановка UB-персонажей через `AddProfileToMap(UB_Get*ID(), sector, gridno)`, гарнизоны через `UB_SetNumberJa25EnemiesInSurfaceSector`).

## 2. strategicmap.lua — вход/выход из секторов

Константы: `SectorY`, `EventTypes`, `qStatus`, полная таблица `Quests` (26 квестов с номерами записей QUESTS.EDT), `Facts`, `History` (78 типов записей журнала), `Profil` (MARIA=88, ANGEL=89, ELLIOT=135, CONRAD=70, CARMEN=78, MADLAB=146, ROBOT=62).

`HandleQuestCodeOnSectorEntry`:
- C6 + квест RESCUE_MARIA завершён → убрать Марию и Энджела (`SetCharacterSectorX/Y 0`).
- D5 (бокс) → сброс боксёров `gubBoxerID(0..2, NOBODY)`.
- P3 (дворец Медуны) → подлечить Эллиота (`SetNPCLife = GetNPCLifeMax`).
- Факт ALL_TERRORISTS_KILLED → убрать Кармена.
- (Закомментировано: перенос робота Мадлаба.)

`HandleQuestCodeOnSectorExit`:
- Выход из сектора денег Кингпина (`iniKINGPIN_MONEY_SECTOR_*`) → `CheckForKingpinsMoneyMissing(true)`.
- Выход из H13 при факте CONRAD_SHOULD_GO → убрать Конрада.
- Выход из сектора госпиталя → `CheckForMissingHospitalSupplies()`.
- Сброс `FACT_MUSEUM_ALARM_WENT_OFF`.

Data-1.13 добавляет `Items = { MONEY = 219 }`.

## 3. Quests.lua — старт/финиш квестов

`InternalStartQuest`: если не начат — `SetQuest(INPROGRESS)` + `SetHistoryFact(HISTORY_QUEST_STARTED, ...)`.

`InternalEndQuest` — выдаёт очки опыта через `GiveQuestRewardPoint(x, y, очки, профиль)`:
- 3: DELIVER_LETTER, LEATHER_SHOP_DREAM;
- 4: ESCORT_SKYRIDER, CHOPPER_PILOT, FREE_CHILDREN, RESCUE_MARIA, FIND_SCIENTIST, DELIVER_VIDEO_CAMERA, FIND_HERMIT, FREE_DYNAMO (+профиль Динамо, если жив);
- 5: KINGPIN_IDOL, CHITZENA_IDOL, HELD_IN_ALMA, RUNAWAY_JOEY, ESCORT_TOURISTS, ESCORT_SHANK, KILL_TERRORISTS (5, если Кармен мёртв — выбран Slay; иначе 9);
- 6: ARMY_FARM, KINGPIN_MONEY; 7: BLOODCATS; 8: INTERROGATION, CREATURES; 25: KILL_DEIDRANNA;
- особый случай: FOOD_ROUTE не завершается здесь (через диалог с отцом Уокером);
- финал: `SetQuest(QUESTDONE)` + `ResetHistoryFact`; для RESCUE_MARIA — сброс `NPCData1/2` мадам Лайлы (антибаг борделя).

UB-версия добавляет квесты 23–26 (DESTROY_MISSLES, FIX_LAPTOP, GET_RID_BLOODCATS_AT_BETTYS, FIND_ANTIQUE_MUSKET_FOR_RAUL).

## 4. Overhead.lua — логика шага солдата (тактика)

Константы `Facts`, `Attitude`, `Quests`, `Profil` (MIGUEL=57 … MADLAB=146), `What` (типы наёмников), `Team` (OUR=0, ENEMY=1, CREATURE=2, MILITIA=3, CIV=4), `pQuest`, константы знаний (`SEEN_CURRENTLY=1`…), `NPC_TALK_RADIUS=4`.

Локальные `HandleJohnArrival(ID)` / `HandleMaryArrival(ID)`: Джон/Мэри в B13; если Джон ближе 40 клеток к цели 8228 и Мэри жива и рядом — `SetFactTrue(FACT_MARY_OR_JOHN_ARRIVED)`, `ActionStopMerc`, `TriggerNPCRecord(ID, 12/13)`. Мэри прибывает самостоятельно только если Джон мёртв.

`HandleAtNewGridNo(ProfileId)`:
1. Факт ESTONI_REFUELLING_POSSIBLE + квест ESCORT_SHANK в процессе → `EndQuest` (заправка в Эстони).
2. Король (D5): если факт 98 FALSE и игрок в комнатах 30–39 сектора D5 → гражданские группы Кингпина враждебны (`MakeMercPtrsHostile` по видимости) + `EnterTeamCombatMode(CIV_TEAM)`.
3. Для EPC (эскорт): Скайрайдер в B13 ближе 11 клеток к 8842 → `SetUpHelicopterForPlayer(13, B, SKYRIDER, 163)` (вертолёт получен); Джон/Мэри → функции прибытия; Мария в C6 при Энджеле в комнате 2 → реплика Энджела (record 12); Джоуи в G8 при факте JOEY_NEAR_MARTHA → реплика (record 9).
4. Квест ESCORT_TOURISTS в процессе + отряд в B13 → прибытие Джона/Мэри.
5. Для CIV_TEAM: хулиганы (JIM/JACK/OLAF/RAY/OLGA/TYRONE) рядом с игроком → `CancelAIAction` + `AddToShouldBecomeHostileOrSayQuoteList` (драка в баре).

⚠️ Скрипт читает `ProfileIdsSectorX/Y/Z` (ветка John&Mary) — эти глобалы в текущем C++ **не экспортируются** (Часть 4.3); ветка фактически не срабатывает, работает дублирующая логика `HandleJohnArrival`.

UB-версия (4.8 КБ): почти пустая — только `SetKeyProfile` для Текса и Бетти.

## 5. InterfaceDialogue.lua — эскорт и Кармен

`HandleStuffForNPCEscorted(npc)`: MARIA/JOEY — ничего; SKYRIDER — факт 269 + старт квеста ESCORT_SKYRIDER; JOHN — нанять Мэри (`RecruitEPC`) + старт ESCORT_TOURISTS; MARY — наоборот.

`CarmenLeavesSectorCallback`: по сектору триггерит запись Кармена 34 (C13) / 35 (G9) / 36 (C5) — сцены террористов/расставания.

## 6. ExplosionControl.lua — ActionID-диспетчер (24 действия)

| ActionID | Действие | Реализация |
| ---: | :--- | :--- |
| 1 / 2 | открыть/закрыть дверь | `ACTION_ITEM_OPEN_DOOR / CLOSE_DOOR` |
| 4 / 5 | скрытая яма 3×3 / 5×5 | `Add3X3Pit/Add5X5Pit` + `SearchForOtherMembersWithinPitRadiusAndMakeThemFall` |
| 6 | переключить дверь | `ACTION_ITEM_TOGGLE_DOOR` |
| 7–10 | переключить Action1–4 | `ToggleActionItemsByFrequency(1..4)` |
| 11 / 12 | вход/выход из борделя | пусто |
| 13 | тревога Кингпина | `PlayJA2Sample(KLAXON_ALARM)`, публичный шум CIV_TEAM, враждебность группы KINGPIN_CIV_GROUP по видимости, `EnterTeamCombatMode` |
| 14 / 15 | секс / открыть комнату | `ACTION_ITEM_SEX / REVEAL_ROOM` |
| 16 | локальная тревога | `MakeNoise(NOBODY, grid, 0, terrain, 30, NOISE_SILENT_ALARM)` |
| 17 | глобальная тревога | публичный шум всем нечеловеческим командам + `SetNewSituationMercPtrs` + `WearGasMaskIfAvailable` |
| 18 | клаксон | звук |
| 19 / 20 / 21 | дверь: снять замок / переключить / снять ловушку | `ACTION_ITEM_UNLOCK_DOOR / TOGGLE_LOCK / UNTRAP_DOOR` |
| 22 | давление-предметы | `TogglePressureActionItemsInGridNo` |
| 23 | тревога музея | Элдин: LOS-тест `SoldierTo3DLocationLineOfSightTest` → `TriggerNPCWithIHateYouQuote`, либо `AI_ACTION_GET_CLOSER` (`SetNextActionData`, `RESETTIMECOUNTER`), факт 278 |
| 24 | тревога кровокотов | как 17, но для CREATURE_TEAM |

UB-версия добавляет ActionID 25–29: бомбы/предупреждение Биггенса (`UB_HavePersonAtGridnoStop` + `TriggerNPCRecord(UB_GetBiggensID(), 17/16)`), укреплённая дверь (`UB_HandleSeeingFortifiedDoor`, `UB_HandleSwitchToOpenFortifiedDoor`), вентилятор электростанции/пуск ракет (`UB_HandleSeeingPowerGenFan`, `UB_HandlePlayerHittingSwitchToLaunchMissles`).

## 7. HourlyUpdate.lua — почасовой тик (сейчас неактивен)

`HourlyQuestUpdate()`: бордель открыт 20:00–04:00 (`FACT_BROTHEL_OPEN`), бар с 15:00 до 02:00 (`FACT_CLUB_OPEN`, сброс `gfBoxerFought(0..2)`, логика отдыха боксёров через `gfBoxersResting`/`gubBoxersRests`/`gubBoxingMatchesWon`), музей 09:00–18:00 (`FACT_MUSEUM_OPEN`).

⚠️ Скрипт никогда не вызывается — `LUA_HOURLY_QUEST_UPDATE` закомментирован (Часть 4.1); логика продублирована в C++ `Hourly Update.cpp`. В Data-1.13 есть вариация `_HourlyUpdate.lua` (точные часы `cHour == 4/20/15` вместо диапазонов) — артефакт отладки.

## 8. StrategicEventHandler.lua — ежедневное «раннее утро» (21.7 КБ)

~430 строк локальных констант email (дубликат таблиц GameInit.lua), статусы наёмников MERC_* (`MERC_IS_DEAD = -5` и т.д.), `guiPabloExtraDaysBribed`, `NPC_SYSTEM_EVENT_ACTION_PARAM_BONUS = 10000`.

`HandleEarlyMorningEvents()` — ежедневный сброс:
- Для всех RPC/NPC/Vehicle: сброс флагов дефолтных реплик (`SetThreaten/Recruit/FriendlyOrDirectDefaultResponseUsedRecently`) + `SetMiscFlags2(PROFILE_MISC_FLAG2_BANDAGED_TODAY)`.
- Отец Уокер: случайная проповедь (`SetNPCData1` 1–4) + перемещение C13↔C14, если сектора не загружены.
- Тони (C5): доступен с вероятностью `iniCHANCE_TONY_AVAILABLE`% (`FACT_TONY_NOT_AVAILABLE` + `AddNPCtoSector(5,3,0)` или `(0,0,0)`).
- Девин: счётчик дней; после 3 — случайное перемещение (G9/D13/C5/H2/C6).
- Хэймус с фургоном мороженщика: случайный маршрут (G6/F12/D7/D3/D9), если живы оба и сектор не загружен.
- Rat уезжает (`SetCharacterSector 0`); Джон и Мэри уезжают после ESCORT_TOURISTS.
- Обнуление денег Винса/Стива/Дженни, сброс `FACT_VINCE_EXPECTING_MONEY`, баланс Даррена 15000.
- Кармен: если ждёт (`PROFILE_MISC_FLAG2_DONT_ADD_TO_SECTOR`) — перенос в C13 + деньги за головы ($10000 × головы, `RemoveObjectFromSoldierProfile` слоты 248–254); иначе случайное место (C5/C13/G9) и минимум $5000.
- Случайно `FACT_DAVE_HAS_GAS`.
- Если игрок в секторе госпиталя → `CheckForMissingHospitalSupplies()`.

UB-версия: JA2-блок обёрнут в `--[[ ... ]]` — функция фактически пуста.

## 9. StrategicTownLoyalty.lua — лояльность городов

Константы: `Profil` (MIGUEL=57, DOREEN=139, MARTHA=109, KEITH=147, YANNI=108, AUNTIE=76, MATT=148), `Town` (OMERTA=1 … CHITZENA=12), `GAIN_PTS_PER_LOYALTY_PT = 500`, таблица `LOYALTY_BONUS_*` (с комментариями о множителях сентимента городов), `EventGlobal` (13 типов, соответствуют C++).

`HandleGlobalLoyaltyEvent(type,x,y,z)` — коэффициенты через `AffectAllTownsLoyaltyByDistanceFrom`:
- BATTLE_WON +500 (+проверка старта квеста тварей), QUEEN_BATTLE_WON +1000 (с проверкой), BATTLE_LOST −750, ENEMY_KILLED +50, NATIVE_KILLED −50, ABANDON_MILITIA −750, GAIN_TOWN_SECTOR +500, LOSE_TOWN_SECTOR −1000, LIBERATE_WHOLE_TOWN +1000, GAIN_MINE +1000, LOSE_MINE −1500, GAIN_SAM +250, LOSE_SAM −250.

Локальный `CheckConditionsForTriggeringCreatureQuest`: только SciFi (`newGAME_STYLE==1`, `iniENABLE_CREPITUS==1`), `giLairID ~= 0` → квест уже начат; иначе если ≥3 из 4 шахт (D13, H8, I14, H3) не под врагом (`SectorEnemyControlled`) → `InitCreatureQuest()`.

`HandleLoyaltyChangeForNPCAction(profile)`: MIGUEL → Omerta (письмо Энрико); DOREEN → Drassen; MARTHA → Cambria (если Джоуи жив); KEITH → Cambria (хиллбилли); YANNI → Chitzena + везде (кубок); AUNTIE → Alma (кровокоты); MATT → Alma (Динамо).

## 10. Intro.lua — последовательность роликов

Таблицы `ScreenTypes`, `IntroType`, `Video`. `GetNextIntroVideo` — конечный автомат: начало игры `INTRO_REBEL_CRDT → INTRO_OMERTA → INTRO_PRAGUE_CRDT → INTRO_PRAGUE → NO_VIDEO`; финал — вилки по живости Мигеля (57) и Скайрайдера (97): `END_SPEECH_MIGUEL/NO_MIGUEL → HELI_FLYBY → SKYRIDER/NO_SKYRIDER_HELICOPTER`; splash: `SPLASH_SCREEN → SPLASH_TALONSOFT`.

`PrepareToExitIntroScreen`: INTRO_BEGINNING → `INIT_SCREEN`; INTRO_SPLASH → `DisplaySirtechSplashScreen()` + `SetDoneWithSplashScreen(true)`; иначе `ReStartingGame()` → `CREDIT_SCREEN`.

UB-версия: автомат с `INTRO_HELI_CRASH` (крушение вертолёта).

## 11. initmines.lua — экономика шахт

`InitializeMines()`: таблица 6 шахт `{Location, Type(Gold/Silver), MinimumProduction, AssociatedUnderground{"D4-1",...}, Infectible}`:
- D4 San Mona (золото, 0 — истощена), D13 Drassen (серебро, 1000), I14 Alma (серебро, 1500), H8 Cambria (серебро, 1500), B2 Chitzena (серебро, 500), H3 Grumm (золото, 2000).

Логика: `MaxRemovalRate = MinimumProduction`; `RemainingOreSupply = 999999999` (или 0); рандомизация: `numProductionIncreases = {25,25,20,15}[newDIFFICULTY_LEVEL]` — случайные шахты получают +20% к базе (суммарная добыча фиксирована сложностью); опция `iniWHICH_MINE_SHUTS_DOWN`: 0 = нет, −1 = случайная (кроме Alma и пустых), >0 = индекс; для истощающейся: `RemainingOreSupply = minDays(20 для Drassen / 10 остальных) × 4 × MaxRemovalRate`, `Infectible = 0` (твари не заражают).

`InitializeHeadMiners(mines, currentMine)`: 5 шахтёров `{Profile = 106 Fred, 148 Matt, 156 Oswald, 157 Calvin, 158 Carl}` + `Quotes{RUNNING_OUT, CREATURES_ATTACK, CREATURES_GONE, CREATURES_AGAIN}` (−1 = нет реплики). Matt всегда в Alma (ID 3); Fred — в первой посещённой непустой шахте; остальные случайно по оставшимся добывающим шахтам. Возвращает `nil` (перевызов при следующем входе в шахту) или массив. UB добавляет заброшенную шахту I13 (золото, 0, 7 подземных секторов).

## 12. initunderground.lua + undergroundsectornames.lua — подземелья

Структура (463 строки): `math.randomseed(os.time())`; константы `Habitat` (распределение тварей в %: QueenLair=0, Lair=1, LairEntrance=2, InnerMine=3, OuterMine=4, FeedingGrounds=5, MineExit=6) и `CreatureMusic` (Compat/Auto/Never/Always); `sectorList` — JA2: A10-1 (Rebel Hideout), J9-1 (Tixa), J9-2, K4-1 (Orta), O3-1, P3-1, P1-1 (Demoville), P1-2 — с `loadscreen = LS_Basement/LS_Cave`; UB: I13-1…L15-3 (LS_MINEDAY/LS_TUNNELS/LS_UPSTAIRS/LS_COMPLEXBSMNT), P1-1.

`PopulateSectors(difficulty, style)` (локальная): гарнизоны от сложности, например `s["J9-1"].numTroops = {8,11,15,20}[difficulty]`, `s["J9-2"].numCreatures = 2 + 2*difficulty + rand(0,1)`, `s["P3-1"].numElites = {8,10,14,20}[difficulty] + rand(0,{2,5,6,0}[difficulty])`, P1-1 Demoville: на Insane 24 элиты вместо 16 войск.

`BuildUndergroundSectorList(gameSettings)` → `PopulateSectors` → возвращает `sectorList` (C++ парсит location/numAdmins/numTroops/numElites/numBloodcats/numCreatures/creatureHabitat/music — Часть 3.4).

`GetLoadscreen(location, timeOfDay)`: кастомный `loadscreen` сектора или дефолт: уровень «1» → `LS_Mine`, иначе `LS_Cave` (возврат 2 строк: путь + расширение `sti`).

`GetSectorName(coords, details)`: если посещён — поиск в таблице `sectornames` (из `undergroundsectornames.lua`; RU-версия подхватывается языковым префиксом): `"J9: Tixa Dungeon"`; если твари живы и имени нет — `creatureLair`; если не посещён — пустая строка. Вызывается C++ часто (каждый кадр) — потому живёт в долгоживущем состоянии.

Метатаблица `sectorList`: `__index` обеспечивает доступ `sectorList["J9-1"]` линейным поиском по полю `location`.

## 13. Практические выводы для моддинга

1. **Единственный источник квестовых наград и лояльности — скрипты**: правка `Quests.lua` (очки) и `StrategicTownLoyalty.lua` (коэффициенты) не требует перекомпиляции.
2. **Скрипты перечитываются при каждом хуке** — мод можно вести «на живую», но каждый вызов = чтение файла + новое состояние; `GetSectorName` особенно частый.
3. **Свободные ActionID** в ExplosionControl.lua: 3 (пропущен в JA2), 25+ в JA2-версии — можно добавлять свои эффекты предметов.
4. **Разночтения Data vs Data-1.13**: бэкапы `_HourlyUpdate.lua`, `_StrategicTownLoyalty.lua` — артефакты отладки; VFS перекрывает `Data` папками `Data-*` по порядку.
5. **Локализация подземелий**: `Russian.undergroundsectornames.lua` подхватывается префиксом языка; новый сектор = записи в `sectorList` + `sectornames` + loadscreen-картинка.
6. **Music.lua отсутствует** в установке — для скриптовой музыки создайте файл с функцией `Music()`.

---

