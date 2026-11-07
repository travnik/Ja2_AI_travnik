# Lua API — файл: lua\lua_tactical.cpp

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md). Тактический модуль интерпретатора.

## Регистрация

`LuaTacticalSetup(lua_State *L)` (вызывается из `lua\lua.cpp` → `InitializeLua()`):

1. `CreateLuaClass(L, "ja2_SoldierClass", Soldier)` + `lua_setglobal(L, "ja2_SoldierClass")`.
2. Создаётся глобальная таблица **`Soldiers`** размером `TOTAL_SOLDIERS`: каждый элемент — `NewLuaObject` для `MercPtrs[sold]` либо `nil`.

## Свойства и методы класса Soldier

| Имя в Lua | Функция в C++ | Тип | Описание / Параметры |
| :--- | :--- | :--- | :--- |
| `Soldiers[i].name` | `LuaGetSoldierName` | свойство (чтение) | Имя бойца (`pSoldier->name`) как wstring; `nil` если объект пуст |
| `Soldiers[i].fullname` | `LuaGetSoldierFullName` | свойство (чтение) | Полное имя из `gMercProfiles[ubProfile].zName`; при отсутствии профиля — обычное имя |
| `Soldiers[i].grid` | `LuaGetSoldierGrid` / `LuaSetSoldierGrid` | свойство (чт./зап.) | Текущая клетка `sGridNo`; запись → `TeleportSoldier(pSoldier, grid, TRUE)`, диапазон 1..MAX_MAP_POS |
| `Soldiers[i].level` | `LuaGetSoldierLevel` / `LuaSetSoldierLevel` | свойство (чт./зап.) | Уровень 0/1 (земля/крыша); запись → `HandlePlacingRoofMarker` + `SetSoldierHeight(50*level)` |
| `Soldiers[i].APs` | `LuaGetSoldierAPs` / `LuaSetSoldierAPs` | свойство (чт./зап.) | Очки действия `bActionPoints`, диапазон 1..255 |
| `Soldiers[i].walkto(grid)` | `LuaSoldierWalkTo` | метод | `aiData.bAction = AI_ACTION_WALK`, `usActionData = grid`, затем `ExecuteAction(pSoldier)` |
| `Soldiers[i].runto(grid)` | `LuaSoldierRunTo` | метод | `aiData.bAction = AI_ACTION_RUN`, затем `ExecuteAction(pSoldier)` |
| `Soldiers[i].changestance(stance)` | `LuaSoldierChangeStance` | метод | Смена позы: `ChangeSoldierStance(stance)` |
