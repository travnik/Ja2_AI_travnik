# Lua API — файл: Strategic\Strategic Mines LUA.cpp

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md). Скрипты шахт.

## Роль файла

Класс `LuaMines` (глобальный объект `g_luaMines`) создаёт **собственное** Lua-состояние (`lua_open()` + `luaL_openlibs` в конструкторе) и выполняет скрипт `scripts\initmines.lua` (`LuaMines::LoadScript()`).

## Экспортируемые глобальные переменные

| Имя в Lua | Описание |
| :--- | :--- |
| `newDIFFICULTY_LEVEL` | Уровень сложности (`gGameOptions.ubDifficultyLevel`) |
| `iniWHICH_MINE_SHUTS_DOWN` | Индекс истощаемой шахты (`gGameExternalOptions.bWhichMineRunsOut`) |
| `logging` *(чтение)* | Флаг ведения лога `initmines.log` (считывается из скрипта) |

## Вызываемые Lua-функции (реализуются в initmines.lua)

| Lua-функция | Метод | Параметры | Назначение |
| :--- | :--- | :--- | :--- |
| `InitializeMines` | `InitializeMines()` (через `luaL_ref`/`lua_pcall`) | нет | Инициализация всех шахт кампании |
| `InitializeHeadMiners` | `InitializeHeadMiners()` (через `luaL_ref`/`lua_pcall`) | параметры старателей (через push-значения) | Инициализация главных старателей и их цитат |

Функции обязаны существовать в скрипте — иначе выбрасывается исключение «missing LUA function 'InitializeMines'/'InitializeHeadMiners'».
