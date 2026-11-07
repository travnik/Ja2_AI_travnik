# Lua API — файл: Strategic\UndergroundInit.cpp

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md). Скрипты подземелий.

## Роль файла

Класс `LuaUnderground` (глобальный объект `g_luaUnderground`) управляет Lua-состоянием `LUA_STATE_STRATEGIC_MINES_AND_UNDERGROUND` (инициализация — `Init.cpp`, ~строка 1485) и выполняет скрипты:

* `scripts\initunderground.lua` — построение списка подземных секторов;
* `scripts\undergroundsectornames.lua` (с языковым префиксом) — имена секторов.

## Вызываемые Lua-функции (реализуются в скриптах)

| Lua-функция | C++-вызывающий метод | Параметры | Возврат |
| :--- | :--- | :--- | :--- |
| `BuildUndergroundSectorList` | `LuaUnderground::InitializeSectorList()` | таблица `{ difficultyLevel, gameStyle }` | таблица секторов: `{ location="D13-1", numAdmins, numTroops, numElites, numBloodcats, numCreatures, creatureHabitat, music }` |
| `GetLoadscreen` | `LuaUnderground::GetLoadscreen(x,y,z, loadscreen, format)` | `sector` ("D13-1"), `GetWorldMinutesInDay()` | loadscreen-карта, формат |
| `GetSectorName` | `LuaUnderground::GetSectorName(x,y,z, pSector, buffer, size, detailed)` | `sector`, таблица `{ visited, creaturesPresent, detailed }` | имя сектора (UTF-8 → UTF-16) |

## Методы класса

| Метод | Описание |
| :--- | :--- |
| `InitializeSectorList()` | Вызывает `BuildUndergroundSectorList`, создаёт `UNDERGROUND_SECTORINFO` через `NewUndergroundNode` и заполняет поля из таблицы результата; результат пишется в лог `initunderground.log` |
| `GetLoadscreen(...)` | Экран загрузки подземного сектора |
| `GetSectorName(...)` | Имя подземного сектора (учитывает `visited`, `creaturesPresent`, `detailed`) |
| `LoadScript(langPrefix)` | Загружает оба скрипта (с отбрасыванием UTF-8 BOM) |
| `RunScript(L, filename)` | Читает файл через `FileOpen/FileRead`, выполняет через `LuaState::EvalString` |

## Вспомогательные функции

| Функция | Описание |
| :--- | :--- |
| `locationStringToCoordinates(loc, &x, &y, &z)` | Разбор строки вида «D13-1» в координаты (буква A–P → Y, число 1–16 → X, цифра 1–3 → уровень Z) |
| `XYZtoString(x, y, z)` | Обратное преобразование координат в строку |
