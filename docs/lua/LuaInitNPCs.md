# Lua API — файл: Strategic\LuaInitNPCs.cpp

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md). Основной мост скриптов кампании (~12 940 строк, ~580 регистраций).

## Роль файла

* `IniFunction(lua_State *L, BOOLEAN bQuests)` — ядро регистрации: ~580 вызовов `lua_register`. Выполняется перед запуском каждого игрового скрипта (из обёрток ниже).
* `IniGlobal_0(L)` / `IniGlobal_1(L)` — экспорт контекста текущего профиля (глобальные `ProfileId*`; см. [Luaglobal.md](Luaglobal.md)).
* C++-обёртки запуска скриптов (каждая создаёт состояние через `LuaScopeState` или `LuaState`, регистрирует функции и вызывает Lua):

| C++-обёртка | Скрипт / вызываемая Lua-функция |
| :--- | :--- |
| `LetLuaMusicControl` | `scripts\Music.lua` → `Music` |
| `LuaIntro` | `scripts\Intro.lua` → `PrepareToExitIntroScreen`, `GetNextIntroVideo`, `SetIntroType` |
| `LetLuaMakeBadSectorListFromMapsOnHardDrive` | `scripts\MakeMapsOnHardDrive.lua` → `MakeBadSectorListFromMapsOnHardDrive` |
| `LuaInitStrategicLayer` | `scripts\InitStrategicLayer.lua` → `InitStrategicLayer` |
| `LuaCheckFact` | `scripts\Quests.lua` → `CheckFact` |
| `LuaCheckForKingpinsMoneyMissing` | `scripts\Quests.lua` → `CheckForKingpinsMoneyMissing` |
| `LuaHandleDelayedItemsArrival` | `scripts\GameEventHook.lua` (события предметов) |
| `LuaHandleQuestCodeOnSectorEntry` / `LuaHandleQuestCodeOnSector` | `HandleQuestCodeOnSectorEntry` / `HandleQuestCodeOnSectorExit` |
| `LetLuaGameInit` | `scripts\GameInit.lua` → `InitNewGame`, `InitNPCs`, `InitStrategicLayer` |
| `LuaHandleNPCTeamMemberDeath` | `HandleNPCTeamMemberDeath(profile)` |
| `LuaHandlePlayerTeamMemberDeath` | `HandlePlayerTeamMemberDeath(profile)` |
| `LuaExecuteStrategicEvent` | `ExecuteStrategicEvent(...)` |
| `LuaInternalQuest` | `InternalEndQuest` / `InternalStartQuest` |
| `LetLuaMyCustomHandleAtNewGridNo` | `scripts\Overhead.lua` → `HandleAtNewGridNo` |
| `LuaIDScripts` | `LoadScr(npc, action, quote)` |
| `LuaHandleNPCDoAction` | `scripts\HandleNPCDoAction\%03d.lua` (по действию) |
| `LetLuaInterfaceDialogue` | `scripts\InterfaceDialogue.lua` → `CarmenLeavesSectorCallback`, `HandleStuffForNPCEscorted` |
| `LetLuaPerformItemAction` | `scripts\ExplosionControl.lua` → `PerformItemAction` |
| `LetLuaHourlyQuestUpdate` | `scripts\HourlyUpdate.lua` → `HourlyQuestUpdate` |
| `LetLuaHandleEarlyMorningEvents` | `HandleEarlyMorningEvents`, `HandlePossiblyDamagedPackage`, `AddSecondAirportAttendant`, `SetPabloToUnbribed`, `CheckForMissingHospitalSupplies` |
| `LetLuaHandleNPCSystemEvent` | системные события NPC |
| `LetHandleLoyaltyChangeForNPCAction` | `HandleLoyaltyChangeForNPCAction(profile)` |
| `LuaHandleGlobalLoyaltyEvent` | `HandleGlobalLoyaltyEvent(type,x,y,z)` |
| `SaveLuaGlobalToSaveGameFile` / `LoadLuaGlobalFromLoadGameFile` | Сохранение/загрузка `gLuaGlobal[1000]` |

## Зарегистрированные функции

Полный реестр (~550 уникальных имён Lua, сгруппированных по назначению) приведён в разделе «Часть 2» основного справочника: [Lua_API_Reference.md](../Lua_API_Reference.md).

Краткий указатель по группам:

| Группа | Примеры имён Lua |
| :--- | :--- |
| Константы Action-Items (23) | `ACTION_ITEM_OPEN_DOOR` .. `ACTION_ITEM_BLOODCAT_ALARM` |
| Миссии, ноутбук, e-mail (17) | `HireMerc`, `AddEmail`, `CheckForNewShipment`, `AddTransactionToPlayersBook` |
| Глобальные Lua-переменные (7) | `SetGlobalBool`, `CheckGlobalVal`, `SetGlobalTrue` |
| Время и дата (15) | `GetWorldHour`, `GetWorldTotalMin`, `DayTime`, `AddFutureDayStrategicEvent` |
| Лояльность городов (17) | `IncrementTownLoyalty`, `SetTownLoyalty`, `GetTownLoyaltyRating` |
| Факты и квесты (12) | `CheckFact`, `StartQuest`, `EndQuest`, `SetFactTrue` |
| NPC — проверки (33) | `CheckNPCSector`, `CheckNPCLife`, `NPCInRoom`, `IsMercDead` |
| NPC — изменение/диалоги (60+) | `AddNPC`, `TriggerNPCRecord`, `RecruitRPC`, `TacticalCharacterDialogue` |
| NPC — получение данных (15) | `GetNPCGridNo`, `GetNPCLifeMax`, `WhatKindOfMercAmI` |
| Солдаты — поиск/проверки (38) | `FindSoldierByProfileID`, `PythSpacesAway`, `ClosestPC`, `GetSoldierTeam` |
| Операции MercPtrs (25) | `AnimMercPtrs`, `MakeMercPtrsHostile`, `MercPtsrNextAction` |
| Тактический статус (29) | `gTacticalStatus`, `EnterTeamCombatMode`, `MakeNoise`, `SetCivGroupHostile` |
| Предметы/инвентарь (15) | `CreateItem`, `CreateMoney`, `GuaranteeAtLeastXItemsOfIndex` |
| World items (4) | `GetWorldItemsExists`, `GetWorldItemsObjectItem` |
| Карта/тайлы/ямы (19) | `AddStructToHead`, `Add3X3Pit`, `AddExitGridToWorld` |
| Сектора/группы/перемещения (20) | `EnterSector`, `SetGroupSectorValue`, `FindUnderGroundSector` |
| Города (13) | `GetTownIdForSector`, `VisibleTown`, `CurrentPlayerProgressPercentage` |
| События/история/больница/вертолёт (14) | `SetHistoryFact`, `HospitalRefund`, `SetUpHelicopterForPlayer` |
| Шахты (14) | `IsHisMineEmpty`, `PredictDailyIncomeFromAMine`, `IssueHeadMinerQuote` |
| Бокс (7) | `BoxerExists`, `ResetBoxers`, `gfBoxerFought` |
| Бомбы/двери (12) | `SetOffBombsByFrequency`, `ActionDoorOpen`, `ActivateSwitchInGridNo` |
| Звук/музыка/видео/экраны (35) | `PlayJA2Sample`, `MusicPlay`, `FadeOutGameScreen`, `PauseGame` |
| Разговоры (6) | `CheckTalkerStrong`, `NumMercsNear`, `FemalePresent` |
| Деньги/сложность/прибытие (18) | `GetStartingCashNovice`, `SetDefaultArrivalSector`, `SetTurnTimeLimit` |
| Инициализация кампании (11) | `InitialProfile`, `InitialHeliGridNo1..7`, `InitMercFace` |
| UB / Ja25 (60+) | `UB_GetTexID`, `HandleSeeingPowerGenFan`, `UB_InitialHeli` |
| Прочее (2) | `WhoIs`, `WhoIsThere2` |

## Важные особенности

1. **Двойные регистрации.** Одно имя Lua может регистрироваться повторно на другую C++-функцию (например, `CheckSoldierNeutral` → `l_CheckSoldierNeutral`, затем `l_bNeutral`). Действует последняя регистрация по порядку выполнения.
2. **Синонимы.** Ряд функций имеет по два Lua-имени на одну реализацию (`SetOffBombs`/`SetOffBombsByFrequency`, `MercIsDead`/`CheckMercIsDead`, префиксы `UB_*`).
3. Контекст текущего NPC передаётся не аргументами, а глобальными переменными `ProfileId*` (`IniGlobal_0`/`IniGlobal_1`).
