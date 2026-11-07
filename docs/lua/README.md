# Lua API — точка входа и вспомогательные файлы

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md).

## Init.cpp

* `Init.cpp`, строка ~1485: `LuaState::INIT(lua::LUA_STATE_STRATEGIC_MINES_AND_UNDERGROUND, true);` — создание долгоживущего состояния подземелий, затем `g_luaUnderground.LoadScript(GetLanguagePrefix())` и `g_luaMines.LoadScript()`.
* Строка ~1704: `IniLuaGlobal()` — обнуление `gLuaGlobal[1000]` (хранилище глобальных Lua-переменных, сохраняется в сейв: `SaveLuaGlobalToSaveGameFile`/`LoadLuaGlobalFromLoadGameFile`).

## Tactical\Overhead.cpp

* Строка ~797: `InitializeLua()` — запуск устаревшего интерпретатора (см. [lua_lua.md](lua_lua.md)).
* Флаг `LUA_OVERHEAD`: вызов `LetLuaMyCustomHandleAtNewGridNo(...)` — передача контекста профиля (`PROFILLUA_*`) в `scripts\Overhead.lua`.

## gameloop.cpp

* Строка ~116: `//InitializeLua();` — закомментированный альтернативный запуск интерпретатора.

## Структура документации по файлам

| Файл | Документ | Содержимое |
| :--- | :--- | :--- |
| `lua\lua.cpp` | [lua_lua.md](lua_lua.md) | Ядро интерпретатора, `CreateLuaType`/`CreateLuaClass`, глобальные объекты |
| `lua\lua_tactical.cpp` | [lua_tactical.md](lua_tactical.md) | Класс `Soldiers` |
| `lua\lua_strategic.cpp` | [lua_strategic.md](lua_strategic.md) | Классы `Sectors`, `MobileGroups` |
| `lua\lua_env.cpp` | [lua_env.md](lua_env.md) | Объект `Environment` |
| `lua\lwstring.cpp` | [lua_lwstring.md](lua_lwstring.md) | Тип `wstring` |
| `lua\lua_state.cpp/.h` | [lua_state.md](lua_state.md) | Инфраструктура состояний, `LuaFunction`, `LuaTable`, `LuaInterface` |
| `Strategic\LuaInitNPCs.cpp` | [LuaInitNPCs.md](LuaInitNPCs.md) | Основной мост: `IniFunction`, обёртки скриптов (указатель на полный реестр) |
| `Strategic\Luaglobal.cpp` | [Luaglobal.md](Luaglobal.md) | Экспорт глобальных переменных (`IniGlobalGameSetting`) |
| `Strategic\UndergroundInit.cpp` | [UndergroundInit.md](UndergroundInit.md) | Подземелья: `BuildUndergroundSectorList`, `GetLoadscreen`, `GetSectorName` |
| `Strategic\Strategic Mines LUA.cpp` | [StrategicMinesLUA.md](StrategicMinesLUA.md) | Шахты: `InitializeMines`, `InitializeHeadMiners` |
