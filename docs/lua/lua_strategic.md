# Lua API — файл: lua\lua_strategic.cpp

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md). Стратегический модуль интерпретатора.

## Регистрация

`LuaStrategicSetup(lua_State *L)` (вызывается из `lua\lua.cpp` → `InitializeLua()`):

1. `CreateLuaClass(L, "ja2_SectorClass", Sector)` — класс сектора.
2. Создаётся глобальная таблица **`Sectors`** (256 элементов, ссылки на `SectorInfo[sect]`); `lua_setglobal(L, "Sectors")`.
3. `CreateLuaClass(L, "ja2_MobileGroup", MobileGroup)` + `lua_setglobal(L, "MobileGroups")` — глобальная таблица **`MobileGroups`** со статическими методами.

## Класс Sector — свойства `Sectors[i]`

| Имя в Lua | Функция в C++ | Тип | Описание |
| :--- | :--- | :--- | :--- |
| `Sectors[i].numadmins` | `LuaGetSectorNumAdmins` / `LuaSetSectorNumAdmins` | свойство (чт./зап.) | Количество администраторов (`ubNumAdmins`, 0..255) |
| `Sectors[i].numtroops` | `LuaGetSectorNumTroops` / `LuaSetSectorNumTroops` | свойство (чт./зап.) | Количество войск (`ubNumTroops`) |
| `Sectors[i].numelites` | `LuaGetSectorNumElites` / `LuaSetSectorNumElites` | свойство (чт./зап.) | Количество элитных войск (`ubNumElites`) |
| `Sectors[i].numcreatures` | `LuaGetSectorNumCreatures` / `LuaSetSectorNumCreatures` | свойство (чт./зап.) | Количество креатур (`ubNumCreatures`) |

## Класс MobileGroup — статические члены `MobileGroups.*`

| Имя в Lua | Функция в C++ | Тип | Описание |
| :--- | :--- | :--- | :--- |
| `MobileGroups.grouplist` | `LuaGetGroupList` | статическое свойство | Таблица ID всех активных групп (обход `gpGroupList`) |
| `MobileGroups.get(id)` | `LuaGetGroup` | статический метод | Объект группы по `ubGroupID` (`GetGroup`) или `nil` |
| `MobileGroups.new(sector, admins, troops, elites)` | `LuaGroupCreate` | статический метод | Создание вражеской группы: `CreateNewEnemyGroupDepartingFromSector(...)` |

## Класс MobileGroup — свойства/методы экземпляра

| Имя в Lua | Функция в C++ | Тип | Описание |
| :--- | :--- | :--- | :--- |
| `group.ID` | `LuaGetGroupID` | свойство | Идентификатор группы `ubGroupID` |
| `group.X` | `LuaGetGroupX` | свойство | Сектор X (`ubSectorX`) |
| `group.Y` | `LuaGetGroupY` | свойство | Сектор Y (`ubSectorY`) |
| `group.Z` | `LuaGetGroupZ` | свойство | Уровень (`ubSectorZ`) |
| `group.waypoints` | `LuaGetGroupWaypoints` | свойство | Массив `{x=.., y=..}` из связного списка `pWaypoints` |
| `group.numadmins` | `LuaGetGroupNumAdmins` | свойство | `pEnemyGroup->ubNumAdmins` |
| `group.numtroops` | `LuaGetGroupNumTroops` | свойство | `pEnemyGroup->ubNumTroops` |
| `group.numelites` | `LuaGetGroupNumElites` | свойство | `pEnemyGroup->ubNumElites` |
| `group.addwaypoint(x, y)` | `LuaGroupAddWaypoint` | метод | `AddWaypointToPGroup(group, x, y)`; возвращает результат или 0 |
