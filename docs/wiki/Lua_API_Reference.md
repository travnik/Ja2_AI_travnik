# Справочник C++ методов, доступных из Lua

> JA2 v1.13 (ветка 7609 + AI). Полный справочник: архитектура моста, инфраструктура, модули папки `lua\` и реестр функций моста `IniFunction()`.

## Общая информация

В JA2 v1.13 (7609 + AI) существуют **два независимых Lua-моста**:

### 1. Устаревший интерпретатор «Lua Interpreter» (папка `lua\`)
* Точка входа: `InitializeLua()` в `lua\lua.cpp`. Вызывается из `Tactical\Overhead.cpp` (строка ~797). В вызове из `gameloop.cpp` закомментирован.
* Создаёт глобальное состояние `lua_State *L` (`lua_open()` + `luaL_openlibs`), после чего вызывает:
  * `LuaTacticalSetup(L)` — `lua\lua_tactical.cpp`
  * `LuaStrategicSetup(L)` — `lua\lua_strategic.cpp`
  * `LuaEnvironmentSetup(L)` — `lua\lua_env.cpp`
* Механизм регистрации — **не** `lua_register`, а собственная система классов:
  * `CreateLuaType(L, TypeName, luaL_Reg[])` — создаёт метатаблицу-тип (используется для `wstring`).
  * `CreateLuaClass(L, ClassName, LuaAttrib[])` — создаёт метатаблицу класса с `__index`/`__newindex` (`LuaGetObject`/`LuaSetObject`); каждый атрибут описывается структурой `LuaAttrib { name; get; set; call; staticcall; }` (заголовок `lua\Lua Interpreter.h`). `get`/`set` — свойства, `call`/`staticcall` — методы.
  * `NewLuaObject(L, ClsName, void* Ptr)` — оборачивает указатель на C++-структуру в userdata с метатаблицей класса.
* Ввод скриптов — через `EvalLua(const wchar_t*)` (консоль), выгрузка — `ShutdownLua()`.

### 2. Основной мост скриптов кампании (`Strategic\LuaInitNPCs.cpp`)
* Ядро — функция `IniFunction(lua_State *L, BOOLEAN bQuests)` (строка ~937 файла `Strategic\LuaInitNPCs.cpp`), которая содержит **более 580 вызовов `lua_register`** и выполняется перед запуском каждого игрового скрипта.
* Глобальные переменные C++ экспортируются функциями:
  * `IniGlobalGameSetting(L)` — `Strategic\Luaglobal.cpp` (~120 переменных: настройки, координаты секторов и т.д.)
  * `IniGlobal_0(L)` / `IniGlobal_1(L)` — `Strategic\LuaInitNPCs.cpp` (контекст текущего профиля: `ProfileId*`).
* Скрипты запускаются из «обёрток» (`LuaCheckFact`, `LetLuaGameInit`, `LuaIntro`, `LuaIDScripts`, `LetLuaPerformItemAction` и т.д. в том же файле). Для короткоживущих состояний используется `LuaScopeState`, для долгоживущего состояния подземелий — `LuaState::INIT(lua::LUA_STATE_STRATEGIC_MINES_AND_UNDERGROUND, true)` (файл `Init.cpp`, строка ~1485).
* Обратные вызовы Lua-функций из C++ выполняются классом `LuaFunction` (`lua\lua_function.h/cpp`) и `LuaState::CallFunction` / `EvalFile` / `EvalString` (`lua\lua_state.cpp`).
* Выполняемые скрипты лежат в папке `scripts\` игры: `Quests.lua`, `GameInit.lua`, `InitStrategicLayer.lua`, `HourlyUpdate.lua`, `InterfaceDialogue.lua`, `Intro.lua`, `Music.lua`, `Overhead.lua`, `ExplosionControl.lua`, `GameEventHook.lua`, `StrategicEventHandler.lua`, `strategicmap.lua`, `StrategicTownLoyalty.lua`, `HandleNPCDoAction\%03d.lua`, `MakeMapsOnHardDrive.lua`, `initunderground.lua`, `undergroundsectornames.lua`, `initmines.lua`.

### 3. Вспомогательные (инфраструктурные) классы моста — не вызываются из скриптов

| Класс | Файл | Назначение |
| :--- | :--- | :--- |
| `LuaState` | `lua\lua_state.cpp/.h` | Обёртка над `lua_State`; INIT/GET/CLOSE по ID состояний (`LUA_STATE`, `LUA_STATE_STRUCTURE`, `LUA_STATE_GUI`, `LUA_STATE_STRATEGIC_MINES_AND_UNDERGROUND`); `EvalFile`, `EvalString`, `CallFunction`; вложенный `Registry` (таблицы колбэков и userdata) |
| `LuaScopeState` | `lua\lua_state.h` | RAII-обёртка для короткоживущего состояния |
| `LuaFunction` | `lua\lua_function.cpp/.h` | Fluent-вызов Lua-функции из C++: `Param<T>()`, `TParam()`, `TableOpen()/TableClose()`, `Call(n)`; перегрузки `Param` для `std::string`, `double`, `int`, `unsigned int`, `bool` |
| `LuaTable` | `lua\lua_table.cpp/.h` | Доступ к Lua-таблицам из C++: `getValue<T>`, `setValue<T>`, `length()`, `is_valid()` |
| `LuaInterface` / `LuaUserObject` | `lua\lua_class_interface.cpp/.h` | ООП-экспорт C++-классов в Lua (`IMPLEMENT_LUA_INTERFACE`, `luaExportClass`, `luaGetTypedUserData`); макросы `DECLARE_LUA_CFUNCTION` / `DEFINE_LUA_CFUNCTION` |
| `LuaUnderground` | `Strategic\UndergroundInit.cpp/.h` | Скрипты подземелий: `InitializeSectorList()`, `GetLoadscreen()`, `GetSectorName()`, `LoadScript()`, `RunScript()` |
| `LuaMines` | `Strategic\Strategic Mines LUA.cpp/.h` | Шахты: `LoadScript()`, вызовы `InitializeMines` и `InitializeHeadMiners`; экспортирует переменные `newDIFFICULTY_LEVEL`, `iniWHICH_MINE_SHUTS_DOWN` |

---

## Модуль: Тактика — класс Soldier (глобальный массив `Soldiers`)

Регистрация: `LuaTacticalSetup()` в `lua\lua_tactical.cpp`. Создаётся класс `ja2_SoldierClass` и глобальная таблица `Soldiers` (индексы 0..TOTAL_SOLDIERS-1, каждый элемент — обёрнутый `MercPtrs[i]`, либо `nil`). Доступ к свойствам — через `Soldiers[i].<атрибут>` (метатабличные `__index`/`__newindex`).

| Имя функции в Lua | Функция в C++ | Файл регистрации | Описание / Назначение и параметры |
| :--- | :--- | :--- | :--- |
| `Soldiers[i].name` | `LuaGetSoldierName` | lua\lua_tactical.cpp | Свойство (чтение). Возвращает имя бойца (`pSoldier->name`) как wstring; `nil`, если объект пуст |
| `Soldiers[i].fullname` | `LuaGetSoldierFullName` | lua\lua_tactical.cpp | Свойство (чтение). Возвращает полное имя из профиля `gMercProfiles[ubProfile].zName`; при отсутствии профиля — обычное имя |
| `Soldiers[i].grid` | `LuaGetSoldierGrid` / `LuaSetSoldierGrid` | lua\lua_tactical.cpp | Свойство (чтение/запись). Текущая ячейка `sGridNo`. Запись вызывает `TeleportSoldier(pSoldier, grid, TRUE)`; диапазон 1..MAX_MAP_POS |
| `Soldiers[i].level` | `LuaGetSoldierLevel` / `LuaSetSoldierLevel` | lua\lua_tactical.cpp | Свойство (чтение/запись). Уровень (0 — земля, 1 — крыша). Запись: `HandlePlacingRoofMarker` + `SetSoldierHeight(50*level)` |
| `Soldiers[i].APs` | `LuaGetSoldierAPs` / `LuaSetSoldierAPs` | lua\lua_tactical.cpp | Свойство (чтение/запись). Очки действия `bActionPoints` (1..255) |
| `Soldiers[i].walkto(grid)` | `LuaSoldierWalkTo` | lua\lua_tactical.cpp | Метод. Приказ идти в ячейку: `aiData.bAction = AI_ACTION_WALK`, затем `ExecuteAction(pSoldier)` |
| `Soldiers[i].runto(grid)` | `LuaSoldierRunTo` | lua\lua_tactical.cpp | Метод. Приказ бежать: `AI_ACTION_RUN` + `ExecuteAction` |
| `Soldiers[i].changestance(stance)` | `LuaSoldierChangeStance` | lua\lua_tactical.cpp | Метод. Смена позы бойца: `ChangeSoldierStance(stance)` |

---

## Модуль: Стратегия — класс Sector (глобальный массив `Sectors`)

Регистрация: `LuaStrategicSetup()` в `lua\lua_strategic.cpp`. Создаётся класс `ja2_SectorClass` и глобальная таблица `Sectors` (256 элементов, ссылки на `SectorInfo[sect]`).

| Имя функции в Lua | Функция в C++ | Файл регистрации | Описание / Назначение и параметры |
| :--- | :--- | :--- | :--- |
| `Sectors[i].numadmins` | `LuaGetSectorNumAdmins` / `LuaSetSectorNumAdmins` | lua\lua_strategic.cpp | Свойство (чтение/запись). Количество администраторов в секторе (`ubNumAdmins`, 0..255) |
| `Sectors[i].numtroops` | `LuaGetSectorNumTroops` / `LuaSetSectorNumTroops` | lua\lua_strategic.cpp | Свойство (чтение/запись). Количество войск (`ubNumTroops`) |
| `Sectors[i].numelites` | `LuaGetSectorNumElites` / `LuaSetSectorNumElites` | lua\lua_strategic.cpp | Свойство (чтение/запись). Количество элитных войск (`ubNumElites`) |
| `Sectors[i].numcreatures` | `LuaGetSectorNumCreatures` / `LuaSetSectorNumCreatures` | lua\lua_strategic.cpp | Свойство (чтение/запись). Количество креатур (`ubNumCreatures`) |

---

## Модуль: Стратегия — класс MobileGroups (глобальная таблица `MobileGroups`)

Регистрация: там же, `lua\lua_strategic.cpp`. Класс `ja2_MobileGroup`; статические методы (глобальные) доступны через таблицу `MobileGroups`.

| Имя функции в Lua | Функция в C++ | Файл регистрации | Описание / Назначение и параметры |
| :--- | :--- | :--- | :--- |
| `MobileGroups.grouplist` | `LuaGetGroupList` | lua\lua_strategic.cpp | Статическое свойство. Таблица ID всех активных групп (обход списка `gpGroupList`) |
| `MobileGroups.get(id)` | `LuaGetGroup` | lua\lua_strategic.cpp | Статический метод. Возвращает объект группы по `ubGroupID` (`GetGroup`) или `nil` |
| `MobileGroups.new(sector, admins, troops, elites)` | `LuaGroupCreate` | lua\lua_strategic.cpp | Статический метод. Создаёт вражескую группу: `CreateNewEnemyGroupDepartingFromSector(sector, admins, troops, elites)` |
| `MobileGroups[i].ID` | `LuaGetGroupID` | lua\lua_strategic.cpp | Свойство. Идентификатор группы `ubGroupID` |
| `MobileGroups[i].X` | `LuaGetGroupX` | lua\lua_strategic.cpp | Свойство. Сектор X (`ubSectorX`) |
| `MobileGroups[i].Y` | `LuaGetGroupY` | lua\lua_strategic.cpp | Свойство. Сектор Y (`ubSectorY`) |
| `MobileGroups[i].Z` | `LuaGetGroupZ` | lua\lua_strategic.cpp | Свойство. Уровень (`ubSectorZ`) |
| `MobileGroups[i].waypoints` | `LuaGetGroupWaypoints` | lua\lua_strategic.cpp | Свойство. Таблица путевых точек: массив `{x=.., y=..}` из связного списка `pWaypoints` |
| `MobileGroups[i].numadmins` | `LuaGetGroupNumAdmins` | lua\lua_strategic.cpp | Свойство. Администраторы в группе (`pEnemyGroup->ubNumAdmins`) |
| `MobileGroups[i].numtroops` | `LuaGetGroupNumTroops` | lua\lua_strategic.cpp | Свойство. Войска в группе (`pEnemyGroup->ubNumTroops`) |
| `MobileGroups[i].numelites` | `LuaGetGroupNumElites` | lua\lua_strategic.cpp | Свойство. Элита в группе (`pEnemyGroup->ubNumElites`) |
| `MobileGroups[i].addwaypoint(x, y)` | `LuaGroupAddWaypoint` | lua\lua_strategic.cpp | Метод. Добавляет путевую точку: `AddWaypointToPGroup(group, x, y)`; возвращает результат или 0 |

---

## Модуль: Окружение — глобальный объект `Environment`

Регистрация: `LuaEnvironmentSetup()` в `lua\lua_env.cpp`. Создаётся пустая таблица с метатаблицей класса `ja2_EnvironmentClass`.

| Имя функции в Lua | Функция в C++ | Файл регистрации | Описание / Назначение и параметры |
| :--- | :--- | :--- | :--- |
| `Environment.rain` | `LuaGetRainVal` / `LuaSetRainVal` | lua\lua_env.cpp | Свойство (чтение/запись). Дождь `guiEnvWeather`; запись 0 — `EnvEndRainStorm()`, 1..2 — `EnvBeginRainStorm(level-1)` |
| `Environment.ambient` | `LuaGetAmbient` / `LuaSetAmbient` | lua\lua_env.cpp | Свойство (чтение/запись). Освещённость `gubEnvLightValue` (0..255); вне подвала/пещер выставляет `gfDoLighting = TRUE` |
| `Environment.nightlights(on)` | `LuaSetNightLights` | lua\lua_env.cpp | Метод. `TurnOnNightLights()` / `TurnOffNightLights()` |
| `Environment.primelights(on)` | `LuaSetPrimeLights` | lua\lua_env.cpp | Метод. `TurnOnPrimeLights()` / `TurnOffPrimeLights()` |

---

## Модуль: Строки — тип `wstring`

Регистрация: `CreateLuaType(L, "wstring", WStringMethods)` в `lua\lua.cpp`; реализация в `lua\lwstring.cpp`. Поддержка широких (UTF-16) строк.

| Имя функции в Lua | Функция в C++ | Файл регистрации | Описание / Назначение и параметры |
| :--- | :--- | :--- | :--- |
| `wstring.fromUTF8(str)` | `LuaWStringFromUTF8` | lua\lwstring.cpp | Конструктор. Создаёт wstring из UTF-8 строки (`MultiByteToWideChar`) |
| `s:byte([start[, stop]])` | `LuaWStringByte` | lua\lwstring.cpp | Возвращает коды символов диапазона (по умолчанию символ 1) |
| `wstring.char(...)` | `LuaWStringChar` | lua\lwstring.cpp | Конструктор. Собирает wstring из кодов символов |
| `s:find(...)` | `LuaWStringFind` | lua\lwstring.cpp | Заглушка (возвращает `nil`) — аналог `string.find` |
| `s:format(...)` | `LuaWStringFormat` | lua\lwstring.cpp | Заглушка — аналог `string.format` |
| `s:gmatch(...)` | `LuaWStringGMatch` | lua\lwstring.cpp | Заглушка — аналог `string.gmatch` |
| `s:gsub(...)` | `LuaWStringGSub` | lua\lwstring.cpp | Заглушка — аналог `string.gsub` |
| `s:len()` | `LuaWStringLen` | lua\lwstring.cpp | Длина строки в символах (`tw->len`) |
| `s:lower()` | `LuaWStringLower` | lua\lwstring.cpp | Строка в нижнем регистре (`towlower`) |
| `s:match(...)` | `LuaWStringMatch` | lua\lwstring.cpp | Заглушка — аналог `string.match` |
| `s:rep(n)` | `LuaWStringRep` | lua\lwstring.cpp | Повторяет строку `n` раз |
| `s:reverse()` | `LuaWStringReverse` | lua\lwstring.cpp | Переворачивает строку |
| `s:sub(...)` | `LuaWStringSub` | lua\lwstring.cpp | Заглушка — аналог `string.sub` |
| `s:upper()` | `LuaWStringUpper` | lua\lwstring.cpp | Строка в верхнем регистре (`towupper`) |
| `s:__tostring` | `LuaWStringToString` | lua\lwstring.cpp | Метаметод. Конвертация wstring → обычная UTF-8 строка Lua |
| `s:__index` | `LuaWStringIndex` | lua\lwstring.cpp | Метаметод. Поиск метода в метатаблице; имена `__*` возвращают `nil` |

---

## Модуль: Глобальные переменные, экспортируемые в скрипты

### Контекст профиля — `IniGlobal_0(L)` / `IniGlobal_1(L)` (`Strategic\LuaInitNPCs.cpp`)

| Имя переменной в Lua | Файл регистрации | Описание |
| :--- | :--- | :--- |
| `ProfileId` | Strategic\LuaInitNPCs.cpp | ID текущего профиля NPC (`PROFILLUA2_ubProfile`; только IniGlobal_1) |
| `ProfileIdsSectorX` | Strategic\LuaInitNPCs.cpp | Сектор X текущего персонажа |
| `ProfileIdsSectorY` | Strategic\LuaInitNPCs.cpp | Сектор Y текущего персонажа |
| `ProfileIdbSectorZ` | Strategic\LuaInitNPCs.cpp | Уровень (Z) текущего персонажа |
| `ProfileIdLevel` | Strategic\LuaInitNPCs.cpp | Тактический уровень (земля/крыша; только IniGlobal_0) |
| `ProfileIdubID` | Strategic\LuaInitNPCs.cpp | ID солдата в игре |
| `ProfileIdsGridNo` | Strategic\LuaInitNPCs.cpp | Текущая ячейка (gridno) |
| `ProfileIdubDirectiono` | Strategic\LuaInitNPCs.cpp | Направление взгляда (только IniGlobal_0) |
| `ProfileIdbTeam` | Strategic\LuaInitNPCs.cpp | Номер команды (только IniGlobal_0) |

### Настройки игры и мира — `IniGlobalGameSetting(L)` (`Strategic\Luaglobal.cpp`)

| Имя переменной в Lua | Файл регистрации | Описание |
| :--- | :--- | :--- |
| `giNPCReferenceCount` | Strategic\Luaglobal.cpp | Счётчик ссылок на NPC |
| `gubWaitingForAllMercsToExitCode` | Strategic\Luaglobal.cpp | Код ожидания выхода всех наёмников из сектора |
| `newGAME_STYLE` | Strategic\Luaglobal.cpp | Стиль игры (`gGameOptions.ubGameStyle`) |
| `newDIFFICULTY_LEVEL` | Strategic\Luaglobal.cpp | Уровень сложности (`gGameOptions.ubDifficultyLevel`) |
| `iniENABLE_CREPITUS` | Strategic\Luaglobal.cpp | Включены ли креатуры |
| `iniENEMY_INVESTIGATE_SECTOR` | Strategic\Luaglobal.cpp | Флаг расследования сектора противником (не используется) |
| `iniMERC_WEBSITE_IMMEDIATELY_AVAILABLE` | Strategic\Luaglobal.cpp | «AIM» доступен с первого дня (`fMercDayOne`) |
| `iniCHANCE_TONY_AVAILABLE` | Strategic\Luaglobal.cpp | Шанс доступности Тони |
| `iniSTARTING_CASH_NOVICE` | Strategic\Luaglobal.cpp | Начальные деньги (новичок) |
| `iniSTARTING_CASH_EXPERIENCED` | Strategic\Luaglobal.cpp | Начальные деньги (опытный) |
| `iniSTARTING_CASH_EXPERT` | Strategic\Luaglobal.cpp | Начальные деньги (эксперт) |
| `iniSTARTING_CASH_INSANE` | Strategic\Luaglobal.cpp | Начальные деньги (безумный) |
| `iniCAN_TRUE_CIVILIANS_BECOME_HOSTILE` | Strategic\Luaglobal.cpp | Могут ли мирные жители становиться враждебными (не используется) |
| `iniNEW_MERC_ARRIVAL_LOCATION` | Strategic\Luaglobal.cpp | Стартовая ячейка прибытия наёмника |
| `iniDEFAULT_ARRIVAL_SECTOR_X` / `iniDEFAULT_ARRIVAL_SECTOR_Y` | Strategic\Luaglobal.cpp | Сектор прибытия по умолчанию (X/Y) |
| `iniHIDEOUT_SECTOR_X/_Y/_Z`, `iniHIDEOUT_EXIT`, `iniHIDEOUT_SURFACE_X/_Y/_Z`, `iniHIDEOUT_ENTRY` | Strategic\Luaglobal.cpp | Координаты убежища (Mod Setting.ini) |
| `iniBASEMENT_ENTRY_1`..`iniBASEMENT_ENTRY_10` | Strategic\Luaglobal.cpp | Ячейки входа в подвал (1–10) |
| `iniBASEMENT_EXIT_1`..`iniBASEMENT_EXIT_12` | Strategic\Luaglobal.cpp | Ячейки выхода из подвала (1–12) |
| `iniFINAL_CRATE_GRID` | Strategic\Luaglobal.cpp | Ячейка финального ящика (логово повстанцев) |
| `iniCRATE_TILE_DEF` / `iniTRAPDOOR_TILE_DEF` | Strategic\Luaglobal.cpp | Тайлы ящика и люка |
| `iniPORN_SHOP_ROOM_HANS` / `_BRENDA` / `_TONY` | Strategic\Luaglobal.cpp | Номера комнат магазинов Сан-Моны |
| `iniHANS_POSTION` | Strategic\Luaglobal.cpp | Ячейка Ханса |
| `iniANGELS_LEATHERSHOP` | Strategic\Luaglobal.cpp | Кожаный магазин Ангела |
| `iniDOOR_TO_BAMBIS_ROOM` / `_CARLAS_ROOM` / `_CINDYS_ROOM` / `_MARIAS_ROOM` | Strategic\Luaglobal.cpp | Ячейки дверей комнат публичного дома |
| `iniFIRST_ROOM_IN_BROTHEL` / `iniLAST_ROOM_IN_BROTHEL` | Strategic\Luaglobal.cpp | Диапазон комнат публичного дома |
| `iniBROTHEL_GUARD_ROOM` | Strategic\Luaglobal.cpp | Комната охраны публичного дома |
| `iniBROTHEL_DOOR_1`..`iniBROTHEL_DOOR_3` | Strategic\Luaglobal.cpp | Двери публичного дома |
| `iniKINGPIN_MONEY_SECTOR_X/_Y/_Z` | Strategic\Luaglobal.cpp | Сектор денег Кингпина |
| `iniOMERTA_DROPOFF_X/_Y/_Z`, `iniOMERTA_DROPOFF_GRIDNO` | Strategic\Luaglobal.cpp | Точка выдачи снаряжения в Омёрте |
| `iniROCKETRIFLE_POSITION` | Strategic\Luaglobal.cpp | Ячейка ракетной винтовки (Альма) |
| `iniHOSPITAL_SECTOR_X/_Y/_Z` | Strategic\Luaglobal.cpp | Сектор больницы |
| `iniADD_MADLAB_SECTOR_1_X/_Y` .. `iniADD_MADLAB_SECTOR_4_X/_Y` | Strategic\Luaglobal.cpp | Дополнительные сектора Мэдлаба (1–4) |
| `ubDefaultArrivalSectorX` / `ubDefaultArrivalSectorY` | Strategic\Luaglobal.cpp | Сектор прибытия по умолчанию (старые имена) |
| `iFirstArrivalDelay` | Strategic\Luaglobal.cpp | Задержка первого прибытия наёмника |
| `guiHelicopterSkyriderTalkState` | Strategic\Luaglobal.cpp | Состояние диалогов Скайдрайвера |
| `guiTimeOfLastSkyriderMonologue` | Strategic\Luaglobal.cpp | Время последнего монолога Скайдрайвера |
| `gubBloodcatLairSectorId` | Strategic\Luaglobal.cpp | ID сектора логова кровокотов |
| `gubSrcSoldierProfile` | Strategic\Luaglobal.cpp | Профиль исходного солдата |
| `giLairID` | Strategic\Luaglobal.cpp | ID логова |
| `is_networked` | Strategic\Luaglobal.cpp | Признак сетевой игры |
| `gWorldSectorX` / `gWorldSectorY` / `gbWorldSectorZ` | Strategic\Luaglobal.cpp | Текущий мировой сектор |
| `giHospitalTempBalance` / `gbHospitalPriceModifier` / `giHospitalRefund` | Strategic\Luaglobal.cpp | Счёт/модификатор цены/возврат больницы |
| `gfBoxersResting` / `gubBoxersRests` / `gubBoxingMatchesWon` | Strategic\Luaglobal.cpp | Состояние боксёрских боёв |
| `cHour` / `cDay` / `cMin` | Strategic\Luaglobal.cpp | Текущие час/день/минута (`uiHourLua`, `uiDayLua`, `uiMinLua`) |
| `gbPlayerNum` | Strategic\Luaglobal.cpp | Номер игрока (мультиплеер) |
| `uiMusicHandle` / `gubMusicMode` / `fMusicPlaying` | Strategic\Luaglobal.cpp | Состояние музыкального движка |
| `gfInvestigateSector` / `fMercDayOne` | Strategic\Luaglobal.cpp | Настройки (дубликаты без префикса ini) |
| `gameStyle` / `enableCrepitus` / `difficultyLevel` | Strategic\Luaglobal.cpp | Стиль игры, креатуры, сложность (старые имена) |
| `startingCashNovice` .. `startingCashInsane` | Strategic\Luaglobal.cpp | Начальные деньги (старые имена) |
| `fCanTrueCiviliansBecomeHostile` | Strategic\Luaglobal.cpp | Враждебность мирных (старое имя) |
| `guiNumWorldItems` | Strategic\Luaglobal.cpp | Количество предметов в мире |
| `gfEnteringMapScreen` | Strategic\Luaglobal.cpp | Флаг входа на карту |
| `guiIntroExitScreen` / `gfIntroScreenExit` / `gbIntroScreenMode` / `gfDoneWithSplashScreen` | Strategic\Luaglobal.cpp | Состояние экранов интро/заставки |
| `sSelMapX` / `sSelMapY` / `iCurrentMapSectorZ` | Strategic\Luaglobal.cpp | Выбранный сектор на карте |
| `guiDay` / `guiHour` / `guiMin` / `guiCurrentScreen` | Strategic\Luaglobal.cpp | Текущее время и активный экран |
| `TestUB` *(только JA2UB)* | Strategic\Luaglobal.cpp | Отладочный флаг UB |
| `iniDEFAULT_END_SECTOR_X/_Y/_Z`, `iniINITIALHELIGRIDNO1`..`7`, `iniJERRYGRIDNO`, `iniLAPTOP_QUEST`, `iniJERRY`, `iniJERRYQUOTES`, `iniINGAMEHELICRASH`, `iniINGAMEHELI` *(JA2UB)* | Strategic\Luaglobal.cpp | Настройки кампании UB: финальный сектор, ячейки вертолёта, Джерри, квест ноутбука и т.д. |
| `UB_iniDEFAULT_END_SECTOR_*`, `UB_iniINITIALHELIGRIDNO1`..`7`, `UB_iniJERRYGRIDNO`, `UB_iniLAPTOP_QUEST`, `UB_iniJERRY`, `UB_iniJERRYQUOTES`, `UB_iniINGAMEHELICRASH`, `UB_iniINGAMEHELI` *(JA2UB)* | Strategic\Luaglobal.cpp | Те же значения под новыми UB-именами |
| `enabledJohnAndTex` / `UB_enabledJohnAndTex`, `RandomManuelText` / `UB_RandomManuelText` *(JA2UB)* | Strategic\Luaglobal.cpp | Наличие Джона и Текса; случайные реплики Мануэля |
| `itemBARRETT_UB`, `itemCALICO_960_UB`, `itemPSG1_UB`, `itemL85_UB`, `itemTAR21_UB`, `itemVAL_SILENT_UB`, `itemMICRO_UZI_UB`, `itemCALICO_950_UB`, `itemCALICO_900_UB`, `itemCLIP_CANNON_BALL`, `itemMERC_UMBRELLA`, `itemHAND_CANNON`, `itemHARTFORD_6_SHOOTER`, `itemTEX_MOVIE_*` (3), `itemLAPTOP_TRANSMITTER`, `itemCHE_GUEVARA_CANTEEN`, `itemMERC_WRISTWATCH`, `itemSAM_GARVER_COMBAT_KNIFE`, `itemMERC_UMBRELLA_OLD`, `itemMORRIS_INSTRUCTION_NOTE` *(JA2UB)* | Strategic\Luaglobal.cpp | Номера предметов кампании UB (старые имена) |
| `UB_itemBARRETT`, `UB_itemCALICO_960`, `UB_itemPSG1`, `UB_itemL85`, `UB_itemTAR21`, `UB_itemVAL_SILENT`, `UB_itemMICRO_UZI`, `UB_itemCALICO_950`, `UB_itemCALICO_900`, `UB_itemCLIP_CANNON_BALL`, `UB_itemMERC_UMBRELLA`, `UB_itemHAND_CANNON`, `UB_itemHARTFORD_6_SHOOTER`, `UB_itemTEX_MOVIE_*`, `UB_itemLAPTOP_TRANSMITTER`, `UB_itemCHE_GUEVARA_CANTEEN`, `UB_itemMERC_WRISTWATCH`, `UB_itemSAM_GARVER_COMBAT_KNIFE`, `UB_itemMERC_UMBRELLA_OLD`, `UB_itemMORRIS_INSTRUCTION_NOTE` *(JA2UB)* | Strategic\Luaglobal.cpp | Те же номера предметов под новыми UB-именами |
| `newDIFFICULTY_LEVEL`, `iniWHICH_MINE_SHUTS_DOWN` | Strategic\Strategic Mines LUA.cpp | Сложность и «истощаемая» шахта (экспорт LuaMines) |

---

## Модуль: Функции, вызываемые из C++ (обратные вызовы Lua-скриптов)

Эти имена **реализуются в Lua-скриптах** (`scripts\*.lua`), а C++ вызывает их через `LuaFunction(...)`/`EvalFile` в `Strategic\LuaInitNPCs.cpp`, `Strategic\UndergroundInit.cpp`, `Strategic\Strategic Mines LUA.cpp`:

`CheckFact`, `Music`, `PrepareToExitIntroScreen`, `GetNextIntroVideo`, `SetIntroType`, `MakeBadSectorListFromMapsOnHardDrive`, `InitStrategicLayer`, `HandleQuestCodeOnSectorExit`, `HandleQuestCodeOnSectorEntry`, `InitNewGame`, `InitNPCs`, `HandleNPCTeamMemberDeath`, `HandlePlayerTeamMemberDeath`, `ExecuteStrategicEvent`, `InternalEndQuest`, `InternalStartQuest`, `HandleAtNewGridNo`, `LoadScr`, `CarmenLeavesSectorCallback`, `HandleStuffForNPCEscorted`, `PerformItemAction`, `HourlyQuestUpdate`, `HandleLoyaltyChangeForNPCAction`, `HandleGlobalLoyaltyEvent`, `HandleEarlyMorningEvents`, `HandlePossiblyDamagedPackage`, `AddSecondAirportAttendant`, `SetPabloToUnbribed`, `CheckForMissingHospitalSupplies` — в `Strategic\LuaInitNPCs.cpp`;
`BuildUndergroundSectorList`, `GetLoadscreen`, `GetSectorName` — в `Strategic\UndergroundInit.cpp`;
`InitializeMines`, `InitializeHeadMiners` — в `Strategic\Strategic Mines LUA.cpp`.

---

# Часть 2. Реестр функций моста `IniFunction()` (Strategic\LuaInitNPCs.cpp)

Все функции ниже регистрируются через `lua_register(L, "имя", l_функция)` внутри `IniFunction(lua_State *L, BOOLEAN bQuests)` и её повторных вызовов в скриптовых обёртках. Параметры в большинстве случаев передаются через глобальные переменные `ProfileId*` (см. выше) и аргументы Lua.

## Модуль: Константы Action-Items

Возвращают числовые коды «предметов-действий» (используются с `SetOffBombsByFrequency`, `ToggleActionItemsByFrequency` и др.).

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `ACTION_ITEM_OPEN_DOOR` | `l_ACTION_ITEM_OPEN_DOOR` | Код «открыть дверь» |
| `ACTION_ITEM_CLOSE_DOOR` | `l_ACTION_ITEM_CLOSE_DOOR` | Код «закрыть дверь» |
| `ACTION_ITEM_SMALL_PIT` | `l_ACTION_ITEM_SMALL_PIT` | Код «маленькая яма» |
| `ACTION_ITEM_LARGE_PIT` | `l_ACTION_ITEM_LARGE_PIT` | Код «большая яма» |
| `ACTION_ITEM_TOGGLE_DOOR` | `l_ACTION_ITEM_TOGGLE_DOOR` | Код «переключить дверь» |
| `ACTION_ITEM_TOGGLE_ACTION1` | `l_ACTION_ITEM_TOGGLE_ACTION1` | Код переключателя действия 1 |
| `ACTION_ITEM_TOGGLE_ACTION2` | `l_ACTION_ITEM_TOGGLE_ACTION2` | Код переключателя действия 2 |
| `ACTION_ITEM_TOGGLE_ACTION3` | `l_ACTION_ITEM_TOGGLE_ACTION3` | Код переключателя действия 3 |
| `ACTION_ITEM_TOGGLE_ACTION4` | `l_ACTION_ITEM_TOGGLE_ACTION4` | Код переключателя действия 4 |
| `ACTION_ITEM_ENTER_BROTHEL` | `l_ACTION_ITEM_ENTER_BROTHEL` | Код «вход в публичный дом» |
| `ACTION_ITEM_EXIT_BROTHEL` | `l_ACTION_ITEM_EXIT_BROTHEL` | Код «выход из публичного дома» |
| `ACTION_ITEM_KINGPIN_ALARM` | `l_ACTION_ITEM_KINGPIN_ALARM` | Код «сигнализация Кингпина» |
| `ACTION_ITEM_SEX` | `l_ACTION_ITEM_SEX` | Код «интимные услуги» |
| `ACTION_ITEM_REVEAL_ROOM` | `l_ACTION_ITEM_REVEAL_ROOM` | Код «раскрыть комнату» |
| `ACTION_ITEM_LOCAL_ALARM` | `l_ACTION_ITEM_LOCAL_ALARM` | Код «локальная тревога» |
| `ACTION_ITEM_GLOBAL_ALARM` | `l_ACTION_ITEM_GLOBAL_ALARM` | Код «общая тревога» |
| `ACTION_ITEM_KLAXON` | `l_ACTION_ITEM_KLAXON` | Код «клаксон» |
| `ACTION_ITEM_UNLOCK_DOOR` | `l_ACTION_ITEM_UNLOCK_DOOR` | Код «отпереть дверь» |
| `ACTION_ITEM_TOGGLE_LOCK` | `l_ACTION_ITEM_TOGGLE_LOCK` | Код «переключить замок» |
| `ACTION_ITEM_UNTRAP_DOOR` | `l_ACTION_ITEM_UNTRAP_DOOR` | Код «снять ловушку с двери» |
| `ACTION_ITEM_TOGGLE_PRESSURE_ITEMS` | `l_ACTION_ITEM_TOGGLE_PRESSURE_ITEMS` | Код «переключить нажимные предметы» |
| `ACTION_ITEM_MUSEUM_ALARM` | `l_ACTION_ITEM_MUSEUM_ALARM` | Код «сигнализация музея» |
| `ACTION_ITEM_BLOODCAT_ALARM` | `l_ACTION_ITEM_BLOODCAT_ALARM` | Код «сигнализация кровокотов» |

## Модуль: Миссии, ноутбук, e-mail

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `HireMerc` | `l_HireMerc` | Нанять наёмника (по параметрам: профиль, срок и т.д.) |
| `SetLaptopBroken` | `l_SetLaptopBroken` | Установить состояние «ноутбук сломан» |
| `SetEndMission` | `l_SetEndMission` | Пометить миссию завершённой |
| `SetStartMission` | `l_SetStartMission` | Пометить миссию начатой |
| `CheckMission` | `l_CheckMission` | Проверить состояние миссии по ID |
| `AddPreReadEmail` | `l_AddPreReadEmail` | Добавить письмо, помеченное как прочитанное |
| `AddEmail` | `l_AddEmail` | Добавить письмо в почтовый ящик игрока |
| `AddEmailMercAvailableXML` | `l_AddEmailXML` | Письмо «наёмник доступен» из XML-данных |
| `AddEmailMercLevelUpXML` | `l_AddEmailLevelUpXML` | Письмо о повышении уровня наёмника из XML |
| `AddEmailXML` | `l_AddEmailXML2` | Добавить письмо из XML-источника |
| `AddCustomEmail` | `l_AddCustomEmail` | Добавить письмо с произвольным текстом/параметрами |
| `CountNumberOfBobbyPurchasesThatAreInTransit` | `l_CountNumberOfBobbyPurchasesThatAreInTransit` | Количество заказов Bobby R в пути |
| `CheckForNewShipment` | `l_CheckForNewShipment` | Проверить, пришла ли новая поставка Bobby R |
| `SetBobbyRSiteCanBeAccessed` | `l_fBobbyRSiteCanBeAccessed` | Разрешить/запретить доступ к сайту Bobby R |
| `AddTransactionToPlayersBook` | `l_AddTransactionToPlayersBook` | Добавить финансовую транзакцию в банковскую книгу |
| `AddHistoryToPlayersLog` | `l_AddHistoryToPlayersLog` | Добавить запись в историю кампании |
| `AddFilesToPlayersLog` | `l_AddFilesToPlayersLog` | Добавить файлы (A.IM и т.п.) в историю |

## Модуль: Глобальные Lua-переменные (хранилище `gLuaGlobal[1000]`)

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `SetGlobalBool(index, val)` | `l_SetGlobalLuaBool` | Установить булеву глобальную переменную по индексу |
| `SetGlobalVal(index, val)` | `l_SetGlobalLuaVal` | Установить целочисленную глобальную переменную |
| `SetGlobalTrue(index)` | `l_SetGlobalLuaBoolTrue` | Установить глобальный булев флаг в TRUE |
| `SetGlobalFalse(index)` | `l_SetGlobalLuaBoolFalse` | Установить глобальный булев флаг в FALSE |
| `CheckGlobalBool(index)` | `l_CheckGlobalLuaBool` | Прочитать булеву глобальную переменную |
| `CheckGlobalVal(index)` | `l_CheckGlobalLuaVal` | Прочитать целочисленную глобальную переменную |
| `GetGlobalVal(index)` | `l_GetGlobalLuaVal` | Вернуть значение целочисленной глобальной переменной |

## Модуль: Время и дата

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `GetWorldMinutesInDay` | `l_GetWorldMinutesInDay` | Минуты, прошедшие с начала текущих игровых суток |
| `GetWorldHour` | `l_GetWorldHour` | Текущий час игровых суток |
| `GetWorldTotalMin` | `l_GetWorldTotalMin` | Общее время игры в минутах |
| `GetWorldTotalSeconds` | `l_GetWorldTotalSeconds` | Общее время игры в секундах |
| `GetWorldDay` | `l_GetWorldDay` | Текущий игровой день |
| `GetWorldDayInSeconds` | `l_GetWorldDayInSeconds` | Текущий день, выраженный в секундах |
| `GetWorldDayInMinutes` | `l_GetWorldDayInMinutes` | Текущий день, выраженный в минутах |
| `GetFutureDayInMinutes` | `l_GetFutureDayInMinutes` | Минутное значение для дня в будущем (смещение) |
| `GetMidnightOfFutureDayInMinutes` | `l_GetMidnightOfFutureDayInMinutes` | Полночь будущего дня в минутах |
| `DayTime` | `l_DayTime` | TRUE, если сейчас день |
| `NightTime` | `l_NightTime` | TRUE, если сейчас ночь |
| `AddFutureDayStrategicEvent` | `l_AddFutureDayStrategicEvent` | Запланировать стратегическое событие через N дней |
| `AddAdvancedStrategicEvent` | `l_AddAdvancedStrategicEvent` | Добавить стратегическое событие с расширенными параметрами |
| `AddSameDayStrategicEvent` | `l_AddSameDayStrategicEvent` | Запланировать событие на текущий день |
| `GetMercArrivalTimeOfDay` | `l_GetMercArrivalTimeOfDay` | Время суток прибытия наёмника |

## Модуль: Лояльность городов

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `IncrementTownLoyalty` | `l_IncrementTownLoyalty` | Повысить лояльность города |
| `IncrementTownLoyaltyEverywhere` | `l_IncrementTownLoyaltyEverywhere` | Повысить лояльность во всех городах |
| `DecrementTownLoyalty` | `l_DecrementTownLoyalty` | Понизить лояльность города |
| `SetTownLoyalty` | `l_SetTownLoyalty` | Установить лояльность города в заданное значение |
| `InTownSectorWithTrainingLoyalty` | `l_InTownSectorWithTrainingLoyalty` | Находится ли сектор в городе с тренируемой лояльностью |
| `SetGlobalLoyaltyEvent` | `l_SetHandleGlobalLoyaltyEvent` | Инициировать глобальное событие лояльности |
| `AffectAllTownsLoyaltyByDistanceFrom` | `l_AffectAllTownsLoyaltyByDistanceFrom` | Изменить лояльность всех городов в зависимости от удалённости |
| `GetTownLoyaltyLiberatedAlready` | `l_GetTownLoyaltyfLiberatedAlready` | Флаг «город уже освобождён» |
| `SetTownLoyaltyLiberatedAlready` | `l_SetTownLoyaltyfLiberatedAlready` | Установить флаг «город освобождён» |
| `GetTownLoyaltyStarted` | `l_GetTownLoyaltyfStarted` | Флаг «лояльность начала отслеживаться» |
| `SetTownLoyaltyStarted` | `l_SetTownLoyaltyfStarted` | Установить флаг старта лояльности |
| `GetTownLoyaltyRating` | `l_GetTownLoyaltyubRating` | Рейтинг лояльности города |
| `SetTownLoyaltyRating` | `l_SetTownLoyaltyubRating` | Установить рейтинг лояльности |
| `GetTownLoyaltyChange` | `l_GetTownLoyaltysChange` | Величина изменения лояльности |
| `SetTownLoyaltyChange` | `l_SetTownLoyaltysChange` | Установить изменение лояльности |
| `GetTownUsesLoyalty` | `l_GetgfTownUsesLoyalty` | Использует ли город систему лояльности |
| `SetTownUsesLoyalty` | `l_SetgfTownUsesLoyalty` | Включить систему лояльности для города |

## Модуль: Факты и квесты

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `CheckFact` | `l_CheckFact` | Проверить значение «факта» (fact) для профиля |
| `SetFactTrue` | `l_SetFactTrue` | Установить факт в TRUE |
| `SetFactFalse` | `l_SetFactFalse` | Установить факт в FALSE |
| `gubFact` | `l_gubFact` | Прямой доступ к массиву фактов (чтение/запись) |
| `GetgubFact` | `l_GetgubFact` | Получить значение факта |
| `CheckQuest` | `l_gubQuest` | Прочитать состояние квеста |
| `gubQuest` | `l_gubQuest` | Прямой доступ к массиву квестов |
| `StartQuest` | `l_StartQuest` | Начать квест (в секторе) |
| `EndQuest` | `l_EndQuest` | Завершить квест |
| `SetQuest` | `l_SetgubQuest` | Установить состояние квеста напрямую |
| `GiveQuestRewardPoint` | `l_GiveQuestRewardPoint` | Начислить очко награды за квест |
| `GetTimeQuestWasStarted` | `l_GetTimeQuestWasStarted` | Время начала квеста |

## Модуль: NPC — проверки состояния

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `CheckNPCSector` | `l_CheckNPCSector` | Проверить сектор NPC (X,Y,Z) |
| `CheckNPCSectorBool` | `l_CheckNPCSectorBool` | Булева проверка нахождения NPC в секторе |
| `CheckNPCinSector` | `l_CheckNPCSectorNew` | Проверка нахождения NPC в секторе (новый вариант) |
| `CheckLastDateSpokenTot` | `l_ubLastDateSpokenTot` | Дата последнего разговора с NPC |
| `CheckNPCData1` | `l_fNPCData` | Прочитать поле NPCData1 профиля |
| `CheckNPCData2` | `l_fNPCData2` | Прочитать поле NPCData2 профиля |
| `CheckNPCLife` | `l_bLife` | Текущее здоровье NPC |
| `CheckMoney` | `l_fuiMoney` | Количество денег в профиле NPC |
| `CheckMiscFlags1` | `l_ubMiscFlags1Check` | Прочитать MiscFlags1 профиля |
| `CheckMiscFlags2` | `l_ubMiscFlags2Check` | Прочитать MiscFlags2 профиля |
| `CheckMercIsDead` / `MercIsDead` | `l_Merc_Is_Dead` | Мёртв ли наёмник (два имени одной функции) |
| `IsMercDead` | `l_IsMercDead` | Проверка смерти наёмника по профилю |
| `IsMercHireable` | `l_IsMercHireable` | Доступен ли наёмник для найма |
| `CheckNPCWounded` | `l_CheckNPCWounded` | Ранен ли NPC |
| `CheckNPCWoundedByPlayer` | `l_CheckNPCWoundedbyPlayer` | Ранен ли NPC игроком |
| `CheckNPCInOkayHealth` | `l_CheckNPCInOkayHealth` | Здоров ли NPC достаточно хорошо |
| `CheckNPCBleeding` | `l_CheckNPCBleeding` | Истекает ли NPC кровью |
| `CheckNPCWithin` | `l_CheckNPCWithin` | Находится ли NPC в пределах N клеток от другого персонажа |
| `CheckGuyVisible` | `l_CheckGuyVisible` | Виден ли персонаж игроку |
| `CheckNPCAt` | `l_CheckNPCAt` | Находится ли NPC в указанной клетке |
| `CheckNPCIsEnemy` | `l_CheckNPCIsEnemy` | Является ли NPC врагом |
| `CheckNPCIsEPC` | `l_CheckNPCIsEPC` | Является ли NPC EPC (спасаемый) |
| `CheckNPCIsRPC` | `l_CheckNPCIsRPC` | Является ли NPC RPC (рекрутируемый) |
| `NPCInRoom` | `l_NPCInRoom` | Находится ли NPC в указанной комнате |
| `NPCInRoomRange` | `l_NPCInRoomRange` | Находится ли NPC в диапазоне комнат |
| `PCInSameRoom` | `l_PCInSameRoom` | Находится ли игровой персонаж с NPC в одной комнате |
| `InARoom` | `l_PlayerInARoom` | Находится ли игрок в какой-либо комнате |
| `FemalePresent` | `l_FemalePresent` | Присутствует ли женщина поблизости |
| `CheckPlayerHasHead` | `l_CheckPlayerHasHead` | Есть ли у игрока голова (квест Деша) |
| `AIMMercWithin` | `l_AIMMercWithin` | Находится ли наёмник A.I.M. в пределах N клеток |
| `CheckNPCCowering` | `l_CheckNPCCowering` | Пригнулся ли NPC (в панике) |
| `CheckNPCIsUnderFire` | `l_CheckNPCIsUnderFire` | Находится ли NPC под огнём |
| `NPCHeardShot` | `l_NPCHeardShot` | Услышал ли NPC выстрел |

## Модуль: NPC — изменение состояния, диалоги, действия

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `SetMercProfiles` | `l_SetMercProfiles` | Задать значения полей профиля наёмника |
| `AddNPC` | `l_AddNPC` | Добавить NPC в игру по ID профиля |
| `AddNPCtoSector` | `l_AddNPCTOSECTOR` | Разместить NPC в указанном секторе |
| `ChangeNpcToDifferentSector` | `l_ChangeNpcToDifferentSector` | Переместить NPC в другой сектор |
| `SetTDRUsedRecently` / `SetThreatenDefaultResponseUsedRecently` | `l_bThreatenDefaultResponseUsedRecently` | Флаг «угроза использована недавно» (два имени) |
| `SetRDRUsedRecently` / `SetRecruitDefaultResponseUsedRecently` | `l_bRecruitDefaultResponseUsedRecently` | Флаг «рекрутинг-ответ использован недавно» (два имени) |
| `SetFDRUsedRecently` / `SetFriendlyOrDirectDefaultResponseUsedRecently` | `l_bFriendlyOrDirectDefaultResponseUsedRecently` | Флаг «дружелюбный ответ использован недавно» (два имени) |
| `SetMiscFlags1` | `l_ubMiscFlags1Set` | Установить MiscFlags1 профиля |
| `SetMiscFlags2` | `l_ubMiscFlags2Set` | Установить MiscFlags2 профиля |
| `SetNPCData1` | `l_bNPCData` | Установить NPCData1 |
| `SetNPCData2` | `l_bNPCData2` | Установить NPCData2 |
| `SetNPCBalance` | `l_iBalance` | Установить баланс NPC (деньги на счету) |
| `SetMoney` | `l_SetuiMoney` | Установить количество денег в профиле |
| `SetMoneyInSoldierProfile` | `l_SetMoneyInSoldierProfile` | Установить деньги в профиле наёмника |
| `SetCharacterSectorZ` | `l_SetCharacterSectorZ` | Установить Z-сектор персонажа |
| `SetCharacterSectorX` | `l_SetCharacterSectorX` | Установить X-сектор персонажа |
| `SetCharacterSectorY` | `l_SetCharacterSectorY` | Установить Y-сектор персонажа |
| `SetAlertStatus` | `l_SetAlertStatus` | Установить уровень тревоги солдата |
| `SetNoiseVolume` | `l_SetNoiseVolume` | Установить громкость личного шума солдата |
| `SetNoiseGridno` | `l_SetNoiseGridno` | Установить клетку-источник шума |
| `SetNewSituation` | `l_SetNewSituation` | Пометить «новая ситуация» для солдата (сброс ИИ) |
| `SetNextActionData` | `l_SetNextActionData` | Установить данные следующего действия ИИ |
| `SetNextAction` | `l_SetNextAction` | Установить следующее действие ИИ |
| `SetProfileFaceData` | `l_SetProfileFaceData` | Установить данные лица профиля (портрет, глаза и т.д.) |
| `SetSoldierSide` | `l_SetSoldierSide` | Установить сторону (команду) солдата |
| `SetSoldierOrders` | `l_SetSoldierOrders` | Установить приказы солдата (STAND GROUND и т.п.) |
| `TriggerNPCRecord` | `l_TriggerNPCRecord` | Запустить диалоговую запись NPC (цитату) |
| `TriggerNPCRecordImmediately` | `l_TriggerNPCRecordImmediately` | Запустить запись NPC немедленно |
| `TriggerNPCWithIHateYouQuote` | `l_TriggerNPCWithIHateYouQuote` | Проиграть цитату «я вас ненавижу» |
| `RecruitRPC` | `l_RecruitRPC` | Завербовать RPC в отряд |
| `RecruitEPC` | `l_RecruitEPC` | Взять EPC под охрану |
| `UnRecruitEPC` | `l_UnRecruitEPC` | Освободить EPC |
| `NPCGotoGridNo` | `l_NPCGotoGridNo` | Отправить NPC в указанную клетку |
| `HandleNPCGotoGridNo` | `l_HandleNPCGotoGridNo` | Обработать прибытие NPC в клетку |
| `HandleNPCDoAction` | `l_HandleNPCDoAction` | Выполнить обработчик действия NPC (`scripts\HandleNPCDoAction\%03d.lua`) |
| `HandleNPCClosePanel` | `l_HandleNPCClosePanel` | Обработчик закрытия панели NPC |
| `HandleNPCTriggerNPC` | `l_HandleNPCTriggerNPC` | Обработчик «NPC триггерит NPC» |
| `TeleportSoldier` | `l_TeleportSoldier` | Телепортировать солдата в клетку |
| `DeadSoldier` | `l_EVENT_SoldierGotHit` | Обработчик смерти солдата (обёртка события попадания) |
| `CancelAIAction` | `l_CancelAIAction` | Отменить текущее действие ИИ солдата |
| `RESETTIMECOUNTER` | `l_RESETTIMECOUNTER` | Сбросить счётчик времени действия |
| `RemoveObjectFromSoldierProfile` | `l_RemoveObjectFromSoldierProfile` | Удалить предмет из профиля наёмника |
| `InitNewSoldierAnim` | `l_EVENT_InitNewSoldierAnim` | Запустить новую анимацию солдата |
| `ActionStopMerc` | `l_EVENT_StopMerc` | Остановить наёмника (снять действие) |
| `EnterShopKeeperInterfaceScreen` | `l_EnterShopKeeperInterfaceScreen` | Открыть интерфейс торговца |
| `SetMercStatus` | `l_Merc_Status` | Установить статус наёмника (условия контракта и т.п.) |
| `SetNPCLife` | `l_SetbLife` | Установить здоровье NPC |
| `TacticalCharacterDialogue` | `l_TacticalCharacterDialogue` | Проиграть тактическую реплику персонажа |
| `TacticalCharacterDialogueWithSpecialEvent` | `l_TacticalCharacterDialogueWithSpecialEvent` | Реплика со специальным событием |
| `DeleteMercInventory` / `DeleteSlotMercInventory` | `l_DeleteMercInventory` | Очистить инвентарь наёмника (два имени) |
| `SetSoldierNonNeutral` | `l_SetSoldierNonNeutral` | Сделать солдата не-нейтральным |
| `MercProfileSetBIGPOCK2POS` / `MercProfileSetPOCKPOS` | `l_gMercProfileGearset` | Установить предмет в карман профиля (два имени) |
| `SoldierGiveItem` / `ProfilGiveItem` | `l_SoldierGiveItem` | Выдать предмет солдату/профилю (два имени) |
| `AddItemToInventory` | `l_CreateItemInv` | Добавить предмет в инвентарь |
| `SetSalary` | `l_MercSalary` | Установить зарплату наёмника |
| `SetProfileStrategicInsertionData` | `l_ProfilesStrategicInsertionData` | Установить точку вставки профиля в сектор |

## Модуль: NPC — получение данных

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `GetDirection` | `l_GetDirection` | Направление между двумя клетками |
| `GetNPCGridNo` | `l_GetGridoProfileID` | Клетка, где находится NPC (по профилю) |
| `GetProfileFaceData` | `l_GetProfileFaceData` | Данные лица профиля |
| `GetMercStatus` | `l_Get_Merc_Status` | Статус наёмника |
| `GetNPCLifeMax` | `l_GetbLifeMax` | Максимальное здоровье NPC |
| `GetNPCBalance` | `l_GetiBalance` | Баланс NPC |
| `GetCharacterSectorZ` | `l_GetCharacterSectorZ` | Z-сектор персонажа |
| `GetCharacterSectorX` | `l_GetCharacterSectorX` | X-сектор персонажа |
| `GetCharacterSectorY` | `l_GetCharacterSectorY` | Y-сектор персонажа |
| `GetCharacterTown` | `l_GetgMercProfilesbTown` | Город персонажа (по профилю) |
| `SetCharacterTown` | `l_SetgMercProfilesbTown` | Установить город персонажа |
| `GetCharacterTotalDaysServed` | `l_GetusTotalDaysServed` | Дней службы наёмника |
| `SetCharacterTotalDaysServed` | `l_SetusTotalDaysServed` | Установить дней службы |
| `WhatKindOfMercAmI` | `l_ubWhatKindOfMercAmI` | Тип наёмника (MERC/RPC/EPC и т.д.) |

## Модуль: Солдаты — поиск и проверки

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `CheckSoldierSide` | `l_CheckSoldierSide` | Сторона (команда) солдата |
| `CheckSoldierNeutral` (имя 1) | `l_CheckSoldierNeutral` | Нейтрален ли солдат (по ID солдата) |
| `CheckSoldierNeutral` (имя 2) | `l_bNeutral` | Флаг нейтральности (второе переназначение имени) |
| `CheckSoldierAttitude` | `l_bAttitude` | Отношение (attitude) солдата |
| `CheckSoldierCivilianGroup` | `l_CheckSoldierCivilianGroup` | Гражданская группа солдата |
| `CheckSoldierubProfile` | `l_CheckSoldierubProfile` | ID профиля солдата |
| `CheckSoldierInSector` | `l_CheckSoldierInSector` | Находится ли солдат в секторе |
| `CheckSoldierAssignment` | `l_CheckSoldierAssignment` | Назначение солдата (транспорт и т.п.) |
| `CheckSoldierActive` | `l_CheckSoldierActive` | Активен ли солдат |
| `CheckForPotentialAddToBattleIncrement` / `CheckFPAddBattleIncrement` | `l_CheckForPotentialAddToBattleIncrement` | Возможен ли прирост подкреплений в бою (два имени) |
| `CheckAction` | `l_CheckAction` | Текущее действие ИИ солдата |
| `CheckSoldierNoiseVolume` | `l_CheckSoldierNoiseVolume` | Громкость шума солдата |
| `CheckSoldierAlertStatus` | `l_CheckSoldierAlertStatus` | Уровень тревоги солдата |
| `PythSpacesAway` | `l_PythSpacesAway` | Расстояние между клетками (по теореме Пифагора) |
| `FindSoldierByProfileID` | `l_FindSoldierByProfileID` | Найти ID солдата по профилю |
| `FindProfile` | `l_FindSoldierByProfileIDBool` | Булева проверка наличия солдата по профилю |
| `SoldierTo3DLocationLineOfSightTest` | `l_SoldierTo3DLocationLineOfSightTest` | Проверка линии видимости солдата до точки |
| `ClosestPC` | `l_ClosestPC` | Ближайший игровой персонаж к клетке |
| `TileIsOutOfBounds` | `l_TileIsOutOfBounds` | Находится ли клетка за пределами карты |
| `TileIsOutOfBoundsClosestPC` | `l_TileIsOutOfBoundsClosestPC` | Проверка выхода за границы для ближайшего PC |
| `FindItemSoldier` / `FindItemSoldierAddItemToPool` | `l_FindItemSoldier` | Найти солдата с предметом (опционально добавить в пул; два имени) |
| `FindItemProfile` | `l_FindItem2Soldier` | Найти предмет в профиле |
| `FindSoldierTeam` | `l_FindSoldierTeam` | Найти команду солдата |
| `GetSoldierTeam` | `l_GetSoldierTeam` | Команда солдата |
| `CheckSoldierBodyType` | `l_CheckSoldierBodyType` | Проверить тип тела солдата (регуляр/элита/админ и т.д.) |
| `GetSoldierBodyType` | `l_GetSoldierBodyType` | Получить тип тела солдата |
| `SetSoldierBodyType` | `l_SetSoldierBodyType` | Установить тип тела солдата |
| `IS_CIV_BODY_TYPE` | `l_IS_CIV_BODY_TYPE` | Является ли тип тела гражданским |
| `GetSoldierSectorZ` | `l_CheckSoldierSectorZ` | Z-сектор солдата |
| `GetSoldierSectorX` | `l_CheckSoldierSectorX` | X-сектор солдата |
| `GetSoldierSectorY` | `l_CheckSoldierSectorY` | Y-сектор солдата |
| `GetSoldierbGroupID` | `l_CheckSoldierbGroupID` | ID стратегической группы солдата |
| `SetSoldierSectorZ` | `l_SetSoldierSectorZ` | Установить Z-сектор солдата |
| `SetSoldierSectorX` | `l_SetSoldierSectorX` | Установить X-сектор солдата |
| `SetSoldierSectorY` | `l_SetSoldierSectorY` | Установить Y-сектор солдата |
| `SetSoldierGroupID` | `l_SetSoldierbGroupID` | Установить ID группы солдата |

## Модуль: Групповые операции над MercPtrs (массив активных солдатов)

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `DeadMercPtrs` | `l_AnimMercPtsrSoldierGotHit` | Обход «мёртвых» солдатов (обработчик попадания) |
| `AnimMercPtrs` | `l_AnimMercPtsrInSector` | Обход солдатов в секторе (для анимаций) |
| `CheckMercPtrsInCivilianGroup` | `l_CheckMercPtrsInCivilianGroup` | Принадлежит ли солдат гражданской группе |
| `CheckMercPtrsInActive` | `l_CheckMercPtrsActive` | Активен ли солдат (по индексу MercPtrs) |
| `CheckMercPtrsAssignment` | `l_CheckMercPtrsAssignment` | Назначение солдата |
| `CheckMercPtrsInSector` | `l_CheckMercPtsrInSector` | Находится ли солдат в секторе |
| `CheckMercPtrsID1SeenID2` | `l_CheckMercPtsrubIDSeenubID2` | Видел ли солдат 1 солдата 2 |
| `CheckMercPtrsLife` | `l_CheckMercPtrsLife` | Здоровье солдата (по индексу) |
| `MakeMercPtrsHostile` | `l_MakeMercPtrsHostile` | Сделать солдата враждебным |
| `ChangeMercPtrsTeam` | `l_ChangeMercPtrsTeam` | Сменить команду солдата |
| `GetMercPtrsGroup` | `l_GetMercPtrsGroup` | ID группы солдата |
| `MercPtsrTeleportToSector` | `l_CheckMercPtsrTeleportToSector` | Телепортация солдата в сектор |
| `GetMercPtsrProfileID` | `l_GetMercPtsrProfileID` | ID профиля солдата |
| `MercPtsrAIFlags` | `l_AnimMercPtsrfAIFlags` | Флаги ИИ солдата |
| `MercPtsrNextAction` | `l_AnimMercPtsrbNextAction` | Следующее действие ИИ |
| `MercPtsrNextActionData` | `l_AnimMercPtsrusNextActionData` | Данные следующего действия |
| `MercPtsrAbsoluteFinalDestination` | `l_AnimMercPtsrsAbsoluteFinalDestination` | Конечная точка маршрута |
| `MercPtsrStrategicInsertionCode` | `l_AnimMercPtsrubStrategicInsertionCode` | Код вставки в сектор |
| `MercPtsrStrategicInsertionData` | `l_AnimMercPtsrusStrategicInsertionData` | Данные вставки в сектор |
| `SetMercPtsrGroupID` | `l_SetMercPtsrubGroupID` | Установить группу солдата |
| `SetMercPtsrSectorX` / `SetMercPtsrSectorY` / `SetMercPtsrSectorZ` | `l_SetMercPtsrSectorX` / `l_SetMercPtsrSectorY` / `l_SetMercPtsrSectorZ` | Установить сектор солдата (X/Y/Z) |
| `GetMercPtsrSectorX` / `GetMercPtsrSectorY` / `GetMercPtsrSectorZ` | `l_CheckMercPtsrSectorX` / `l_CheckMercPtsrSectorY` / `l_CheckMercPtsrSectorZ` | Прочитать сектор солдата (X/Y/Z) |
| `GetMercPtsrGroupID` | `l_CheckSoldierbGroupID` | ID группы солдата (переиспользование) |
| `SetNewSituationMercPtrs` | `l_SetNewSituationMercPtsr` | «Новая ситуация» для солдата по индексу |

## Модуль: Тактический статус и команды

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `gTacticalStatus` | `l_gTacticalStatus` | Доступ к структуре тактического статуса |
| `CheckCombatMode` | `l_CheckCombatMode` | Режим боя команды |
| `GetTacticalStatusFirstID` | `l_GetFirstID` | Первый ID солдата в структуре статуса |
| `GetTacticalStatusLastID` | `l_GetLastID` | Последний ID солдата |
| `TacticalStatusTeamHuman` | `l_TacticalStatusTeamHuman` | Человеческая ли команда |
| `TacticalStatusTeamActive` | `l_TacticalStatusTeamActive` | Активна ли команда |
| `GetPlayerMercID` | `l_ubID` | ID наёмника игрока |
| `GetMapMercSlotID` | `l_ubID` | ID слота наёмника на карте |
| `GetTacticalStatusEnemyInSector` | `l_GetTacticalStatusEnemyInSector` | Есть ли враги в секторе |
| `NumberOfMercsOnPlayerTeam` | `l_NumberOfMercsOnPlayerTeam` | Количество наёмников в отряде игрока |
| `SetOffPanicBombs` | `l_SetOffPanicBombs` | Привести в действие «панические» бомбы |
| `WearGasMaskIfAvailable` | `l_WearGasMaskIfAvailable` | Надеть противогаз, если есть |
| `SetCivGroupHostile` | `l_SetCivGroupHostile` | Сделать гражданскую группу враждебной |
| `CheckCivGroupHostile` | `l_fCivGroupHostile` | Враждебна ли гражданская группа |
| `EnterTeamCombatMode` | `l_SetEnterCombatModeTeam` | Ввести команду в боевой режим |
| `EnterGroupCombatMode` | `l_SetEnterCombatMode` | Ввести группу в боевой режим |
| `MakeHostile` | `l_MakeHostile` | Сделать персонажа враждебным |
| `RecalculateOppCntsDueToNoLongerNeutral` | `l_RecalculateOppCntsDueToNoLongerNeutral` | Пересчитать «известных противников» после потери нейтральности |
| `AddToShouldBecomeHostileOrSayQuoteList` | `l_AddToShouldBecomeHostileOrSayQuoteList` | Добавить солдата в список «станет враждебным/произнесёт цитату» |
| `PlayerMercsInSector` | `l_PlayerMercsInSector` | Есть ли наёмники игрока в секторе (булево) |
| `GetPlayerMercsInSector` | `l_GetPlayerMercsInSector` | Получить наёмников игрока в секторе |
| `PlayerGroupsInSector` | `l_PlayerGroupsInSector` | Есть ли группы игрока в секторе |
| `GetPlayerGroupsInSector` | `l_GetPlayerGroupsInSector` | Получить группы игрока в секторе |
| `UpdateMercsInSector` | `l_UpdateMercsInSector` | Обновить список наёмников в секторе |
| `MakeNoise` | `l_MakeNoise` | Создать шум в клетке (слышимость для ИИ) |
| `SetPublicNoiseVolume` | `l_gubPublicNoiseVolume` | Установить публичную громкость шума |
| `SetPublicNoiseGridNo` | `l_gsPublicNoiseGridNo` | Установить клетку публичного шума |

## Модуль: Предметы, инвентарь, ключи

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `CreateItem` | `l_CreateItem` | Создать предмет (в инвентаре/пуле) |
| `CreateMoney` | `l_CreateMoney` | Создать стопку денег |
| `AddCreateItemToPool` | `l_CreateItemToPool` | Добавить созданный предмет в пул мира |
| `AddCreateItemsToUnLoadedSector` | `l_CreateToUnLoadedSector` | Добавить предмет в незагруженный сектор |
| `ItemExistsAtLocation` | `l_ItemExistsAtLocation` | Существует ли предмет в указанном месте |
| `ItemTypeExistsAtLocation` | `l_ItemTypeExistsAtLocation` | Существует ли предмет заданного типа в месте |
| `GuaranteeAtLeastXItemsOfIndex` | `l_GuaranteeAtLeastXItemsOfIndex` | Гарантировать минимум N предметов типа |
| `GuaranteeAtMostNumOfItemsForItem` | `l_GuaranteeAtMostNumOfItemsForItem` | Гарантировать максимум N предметов типа |
| `CreateKeyProfInv` | `l_CreateKeyProfInv` | Создать ключ в инвентаре профиля |
| `CreateKeyAndAddKeyToPool` | `l_CreateKeyAndAddItemToPool` | Создать ключ и добавить в пул |
| `CreateKeyProfInvAndAddKeyToPool` | `l_CreateKeyProfInvAndAddItemToPool` | Ключ в инвентарь профиля + в пул |
| `PlayerTeamFull` | `l_PlayerTeamFull` | Полон ли отряд игрока |
| `CurrentSquad` | `l_CurrentSquad` | Текущий активный отряд |
| `MoveItemPools` | `l_MoveItemPools` | Перенести пул предметов между клетками |
| `GetNumberOfWorldItemsFromTempItemFile` | `l_GetNumberOfWorldItemsFromTempItemFile` | Количество предметов во временном файле сектора |

## Модуль: Предметы в мире (world items)

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `SetWorldItemsExists` | `l_SetgWorldItemsExists` | Установить признак существования предметов в мире |
| `GetWorldItemsExists` | `l_gWorldItemsExists` | Существуют ли предметы в мире |
| `GetWorldItemsObjectItem` | `l_gWorldItemsObjectItem` | Тип предмета из массива мировых предметов |
| `GetWorldItemsObjectDataMoney` | `l_gWorldItemsObjectDataMoney` | Сумма денег у мирового предмета |

## Модуль: Карта, тайлы, ямы, ловушки

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `SetRender` | `l_SetRender` | Управление флагом перерисовки мира |
| `ConvertGridNoToXY` | `l_ConvertGridNoToXY` | Конвертировать gridno в координаты X/Y |
| `AddStructToHead` | `l_AddStructToHead` | Добавить структуру (тайл) в голову клетки |
| `RemoveStruct` | `l_RemoveStruct` | Удалить структуру из клетки |
| `ApplyMapChangesToMapTempFile` | `l_ApplyMapChangesToMapTempFile` | Применить изменения карты ко временному файлу |
| `RemoveGraphicFromTempFile` | `l_RemoveGraphicFromTempFile` | Удалить графику из временного файла карты |
| `AddRemoveObjectToUnLoadedMapTempFile` | `l_AddRemoveObjectToUnLoadedMapTempFile` | Добавить/удалить объект в temp-файл незагруженной карты |
| `RemoveStructFromUnLoadedMapTempFile` | `l_RemoveStructFromUnLoadedMapTempFile` | Удалить структуру из temp-файла |
| `AddObjectToUnLoadedMapTempFile` | `l_AddObjectToUnLoadedMapTempFile` | Добавить объект в temp-файл |
| `AddStructToUnLoadedMapTempFile` | `l_AddStructToUnLoadedMapTempFile` | Добавить структуру в temp-файл |
| `AddExitGridToWorld` | `l_AddExitGridToWorld` | Добавить выходную сетку (exit grid) в мир |
| `WorldLevelDataMapElementRevealed` | `l_WorldLevelDataMAPELEMENT_REVEALED` | Открыт ли элемент карты |
| `WorldLevelDataTerrainID` | `l_WorldLevelDataTerrainID` | ID рельефа клетки |
| `SearchForOtherMembersWithinPitRadiusAndMakeThemFall` | `l_SearchForOtherMembersWithinPitRadiusAndMakeThemFall` | Найти персонажей рядом с ямой и заставить их упасть |
| `Add3X3Pit` | `l_Add3X3Pit` | Создать яму 3×3 |
| `Add5X5Pit` | `l_Add5X5Pit` | Создать яму 5×5 |
| `TogglePressureActionItemsInGridNo` | `l_TogglePressureActionItemsInGridNo` | Переключить нажимные предметы в клетке |
| `ChangeSelectedMapSector` | `l_ChangeSelectedMapSector` | Сменить выбранный сектор на карте |

## Модуль: Сектора, группы, перемещения

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `GetGroup` | `l_GetGroup` | Получить группу по ID (дубликат из lua_strategic.cpp для этого состояния) |
| `SetGroupSectorValue` | `l_SetGroupSectorValue` | Поместить группу в указанный сектор |
| `SetGroupNextSectorValue` | `l_SetGroupNextSectorValue` | Установить следующий сектор группы |
| `PlaceGroupInSector` | `l_PlaceGroupInSector` | Разместить группу в секторе |
| `SetCurrentWorldSector` | `l_SetCurrentWorldSector` | Установить текущий мировой сектор |
| `EnterSector` | `l_EnterSector` | Войти в тактический сектор |
| `InternalLeaveTacticalScreen` | `l_InternalLeaveTacticalScreen` | Выйти из тактического экрана |
| `EnterTacticalInFinalSector` / `UB_EnterTacticalInFinalSector` | `l_EnterTacticalInFinalSector` | Войти в тактику финального сектора (два имени) |
| `SectorEnemy` | `l_SectorEnemy` | Есть ли враги в секторе |
| `SectorEnemyControlled` | `l_fEnemyControlled` | Контролируется ли сектор врагом |
| `SECTOR` | `l_SECTOR` | Служебная работа с кодом сектора |
| `CALCULATE_STRATEGIC_INDEX` | `l_CALCULATE_STRATEGIC_INDEX` | Вычислить стратегический индекс сектора |
| `GetSectorInfoBloodCats` | `l_SectorInfoBloodCats` | Количество кровокотов в секторе |
| `WhichBuddy` | `l_WhichBuddy` | Определить «напарника» NPC (квесты-диалоги) |
| `FindUnderGroundSector` | `l_FindUnderGroundSector` | Найти подземный сектор по координатам |
| `AddEnemyToUnderGroundSector` | `l_AddEnemyToUnderGroundSector` | Добавить врагов в подземный сектор |
| `FindUnderGroundSectorVisited` | `l_FindUnderGroundSectorVisited` | Посещался ли подземный сектор |
| `UnderGroundSectorVisited` | `l_UnderGroundSectorVisited` | Проверка посещения подземелья |
| `StrategicPythSpacesAway` | `l_StrategicPythSpacesAway` | Стратегическое расстояние между секторами |

## Модуль: Города и освобождение

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `VisibleTown` | `l_VisibleTown` | Сделать город видимым на карте |
| `HiddenTown` | `l_HiddenTown` | Скрыть город на карте |
| `GetTownIdForSector` | `l_GetTownIdForSector` | ID города для сектора |
| `CheckIfEntireTownHasBeenLiberated` | `l_CheckIfEntireTownHasBeenLiberated` | Полностью ли освобождён город |
| `CheckIfEntireTownHasBeenLost` | `l_CheckIfEntireTownHasBeenLost` | Полностью ли потерян город |
| `DidFirstBattleTakePlaceInThisTown` | `l_DidFirstBattleTakePlaceInThisTown` | Произошла ли первая битва в этом городе |
| `SetTheFirstBattleSector` | `l_SetTheFirstBattleSector` | Установить сектор первой битвы |
| `AdjustLoyaltyForCivsEatenByMonsters` | `l_AdjustLoyaltyForCivsEatenByMonsters` | Скорректировать лояльность после съедения мирных монстрами |
| `IsTownUnderCompleteControlByPlayer` | `l_IsTownUnderCompleteControlByPlayer` | Полный контроль игрока над городом |
| `GetNumberOfWholeTownsUnderControl` | `l_GetNumberOfWholeTownsUnderControl` | Количество городов под полным контролем |
| `CurrentPlayerProgressPercentage` | `l_CurrentPlayerProgressPercentage` | Прогресс игрока в процентах |
| `SetInvestigateSector` | `l_SetInvestigateSector` | Установить флаг «сектор под расследованием» |
| `CheckInvestigateSector` | `l_CheckInvestigateSector` | Проверить флаг расследования сектора |

## Модуль: Стратегические события, история, больница, вертолёт

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `ResetHistoryFact` | `l_ResetHistoryFact` | Сбросить запись истории |
| `SetHistoryFact` | `l_SetHistoryFact` | Установить запись истории |
| `HospitalTempBalance` | `l_HospitalTempBalance` | Промежуточный счёт больницы |
| `HospitalRefund` | `l_HospitalRefund` | Возврат средств больницей |
| `HospitalPriceModifier` | `l_HospitalPriceModifier` | Модификатор цены больницы |
| `PlayerTeamSawJoey` | `l_gfPlayerTeamSawJoey` | Флаг «отряд видел Джоуи» |
| `SetUpHelicopterForPlayer` | `l_SetUpHelicopterForPlayer` | Инициализировать вертолёт Скайдрайвера для игрока |
| `ExecuteStrategicAIAction` | `l_ExecuteStrategicAIAction` | Выполнить действие стратегического ИИ |
| `InitCreatureQuest` | `l_InitCreatureQuest` | Инициализировать квест креатур |
| `UpdateAndDamageSAMIfFound` | `l_UpdateAndDamageSAMIfFound` | Найти SAM-сайт и повредить его |
| `DoesSAMExistHere` | `l_DoesSAMExistHere` | Есть ли SAM-установка в секторе |
| `UpdateSAMDoneRepair` | `l_UpdateSAMDoneRepair` | Отметить SAM отремонтированной |
| `CheckForMissingHospitalSupplies` | `l_CheckForMissingHospitalSupplies` | Проверка нехватки больничных припасов |
| `CheckForKingpinsMoneyMissing` | `l_FunctionCheckForKingpinsMoneyMissing` | Проверка пропажи денег Кингпина |

## Модуль: Шахты и старатели

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `IssueHeadMinerQuote` | `l_IssueHeadMinerQuote` | Проиграть цитату главного старателя |
| `GetHeadMinersMineIndex` | `l_GetHeadMinersMineIndex` | Индекс шахты старателя |
| `PlayerSpokeToHeadMiner` | `l_PlayerSpokeToHeadMiner` | Говорил ли игрок со старателем |
| `SpokenToHeadMiner` | `l_SpokenToHeadMiner` | Флаг разговора со старателем |
| `IsHisMineRunningOut` | `l_IsHisMineRunningOut` | Истощается ли шахта |
| `IsHisMineEmpty` | `l_IsHisMineEmpty` | Пуста ли шахта |
| `IsHisMineDisloyal` | `l_IsHisMineDisloyal` | Нелояльна ли шахта |
| `IsHisMineInfested` | `l_IsHisMineInfested` | Заражена ли шахта креатурами |
| `IsHisMineLostAndRegained` | `l_IsHisMineLostAndRegained` | Была ли шахта потеряна и возвращена |
| `IsHisMineAtMaxProduction` | `l_IsHisMineAtMaxProduction` | Шахта на максимальной добыче |
| `ResetQueenRetookMine` | `l_ResetQueenRetookMine` | Сброс флага «королева отбила шахту» |
| `HasHisMineBeenProducingForPlayerForSomeTime` | `l_HasHisMineBeenProducingForPlayerForSomeTime` | Добывала ли шахта для игрока достаточно долго |
| `PredictDailyIncomeFromAMine` | `l_PredictDailyIncomeFromAMine` | Прогноз дневного дохода шахты |
| `GetIdOfMineForSector` | `l_GetIdOfMineForSector` | ID шахты для сектора |

## Модуль: Бокс

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `BoxerExists` | `l_BoxerExists` | Существует ли боксёр |
| `gubBoxerID` | `l_gubBoxerID` | ID боксёра |
| `ResetBoxers` | `l_ResetBoxers` | Сбросить состояние боксёров |
| `AnotherFightPossible` | `l_AnotherFightPossible` | Возможен ли ещё бой |
| `gfBoxerFought` | `l_SetgfBoxerFought` | Установить флаг «боксёр дрался» |
| `SetgfBoxersResting` | `l_SetgfBoxersResting` | Установить флаг отдыха боксёров |
| `SetgubBoxersRests` | `l_SetgubBoxersRests` | Установить длительность отдыха боксёров |

## Модуль: Бомбы, двери, переключатели

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `SetOffBombsByFrequency` / `SetOffBombs` | `l_SetOffBombsByFrequency` | Взвести бомбы по частоте (frequency; два имени) |
| `SetOffBombsInGridNo` | `l_SetOffBombsInGridNo` | Привести в действие бомбы в клетке |
| `ToggleActionItemsByFrequency` | `l_ToggleActionItemsByFrequency` | Переключить предметы-действия по частоте |
| `ActiveTimedBombExists` | `l_ActiveTimedBombExists` | Есть ли активная бомба с таймером |
| `RemoveAllActiveTimedBombs` | `l_RemoveAllActiveTimedBombs` | Удалить все активные бомбы с таймером |
| `ActivateSwitchInGridNo` | `l_ActivateSwitchInGridNo` | Активировать переключатель в клетке |
| `ActionDoor` | `l_Action_door` | Действие с дверью (общее) |
| `ActionDoorOpen` | `l_Action_door_open` | Открыть дверь |
| `ActionDoorClose` | `l_Action_door_close` | Закрыть дверь |

## Модуль: Звук, музыка, видео, экраны

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `PlayJA2Sample` | `l_PlayJA2Sample` | Проиграть звуковой сэмпл |
| `SetMusicMode` | `l_SetMusicMode` | Установить режим музыки |
| `MusicPlay` | `l_MusicPlay` | Проиграть музыку по параметрам |
| `MusicIdPlay` | `l_MusicPlayId` | Проиграть музыку по ID |
| `AddMusic` | `l_gAddMusic` | Добавить трек в плейлист |
| `SetMusicID` | `l_SetMusicID` | Установить ID музыки |
| `GetMusicID` | `l_GetMusicID` | Получить ID музыки |
| `MusicSetVolume` | `l_MusicSetVolume` | Громкость музыки |
| `MusicGetVolume` | `l_MusicGetVolume` | Прочитать громкость музыки |
| `SetSoundEffectsVolume` | `l_SetSoundEffectsVolume` | Громкость звуковых эффектов |
| `GetSoundEffectsVolume` | `l_GetSoundEffectsVolume` | Прочитать громкость эффектов |
| `SetSpeechVolume` | `l_SetSpeechVolume` | Громкость речи |
| `GetSpeechVolume` | `l_GetSpeechVolume` | Прочитать громкость речи |
| `DisplaySirtechSplashScreen` | `l_DisplaySirtechSplashScreen` | Показать заставку Sirtech |
| `SetEnteringMapScreen` | `l_gfEnteringMapScreen` | Установить флаг входа на карту |
| `SetIntroScreenExit` | `l_gfIntroScreenExit` | Флаг выхода из интро |
| `SetDoneWithSplashScreen` | `l_gfDoneWithSplashScreen` | Флаг завершения заставки |
| `SetIntroExitScreen` | `l_guiIntroExitScreen` | Экран выхода из интро |
| `SetIntroScreenMode` | `l_gbIntroScreenMode` | Режим экрана интро |
| `SetVideo` | `l_iStringToUse` | Задать проигрываемое видео |
| `StopVideo` | `l_StopVideo` | Остановить видео |
| `StartVideo` | `l_StartVideo` | Запустить видео |
| `FadeOutGameScreen` | `l_FadeOutGameScreen` | Затемнение экрана |
| `FadeInGameScreen` | `l_FadeInGameScreen` | Проявление экрана |
| `SetPendingNewScreen` | `l_SetPendingNewScreen` | Установить следующий экран |
| `PauseGame` | `l_PauseGame` | Пауза игры |
| `DeleteTalkingMenu` | `l_DeleteTalkingMenu` | Закрыть диалоговое меню |
| `TalkingMenuDialogue` | `l_TalkingMenuDialogue` | Реплика в диалоговом меню |
| `StartDialogueMessageBox` | `l_StartDialogueMessageBox` | Показать диалоговый message box |
| `SetScreenMsg` | `l_SetScreenMsg` | Вывести сообщение на экран |
| `InternalLocateGridNo` / `UB_InternalLocateGridNo` | `l_SetInternalLocateGridNo` | Навести камеру/локатор на клетку (два имени) |
| `DestroyedTimeCursor` | `l_DestroyedTimeCursor` | Флаг уничтоженного курсора времени |
| `SetTacticalTraversal` | `l_SetgfTacticalTraversal` | Установить флаг тактического перехода |
| `GetTacticalTraversal` | `l_GetgfTacticalTraversal` | Прочитать флаг тактического перехода |
| `EnvBeginRainStorm` | `l_EnvBeginRainStorm` | Начать дождь |
| `EnvEndRainStorm` | `l_EnvEndRainStorm` | Закончить дождь |

## Модуль: Поведение разговоров (проверки говорящего)

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `CheckTalkerStrong` | `l_CheckTalkerStrong` | Достаточно ли силён говорящий |
| `CheckTalkerFemale` | `l_CheckTalkerFemale` | Говорящий — женщина |
| `CheckTalkerUnpropositionedFemale` | `l_CheckTalkerUnpropositionedFemale` | Женщина, которой ещё не делали предложение |
| `NumMalesPresent` | `l_NumMalesPresent` | Количество мужчин поблизости |
| `NumWoundedMercsNearby` | `l_NumWoundedMercsNearby` | Количество раненых наёмников рядом |
| `NumMercsNear` | `l_NumMercsNear` | Количество наёмников рядом |

## Модуль: Деньги, сложность, прибытие

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `GetStartingCashNovice` | `l_GetStartingCashNovice` | Начальные деньги (новичок) |
| `GetStartingCashExperienced` | `l_GetStartingCashExperienced` | Начальные деньги (опытный) |
| `GetStartingCashExpert` | `l_GetStartingCashExpert` | Начальные деньги (эксперт) |
| `GetStartingCashInsane` | `l_GetStartingCashInsane` | Начальные деньги (безумный) |
| `GetFirstArrivalDelay` | `l_GetiFirstArrivalDelay` | Задержка первого прибытия |
| `SetFirstArrivalDelay` | `l_SetiFirstArrivalDelay` | Установить задержку первого прибытия |
| `GetDefaultArrivalSectorX` (2 варианта) | `l_GetubDefaultArrivalSectorX` / `l_GetDefaultArrivalSectorX` | X сектора прибытия (две регистрации одного имени) |
| `GetDefaultArrivalSectorY` (2 варианта) | `l_GetubDefaultArrivalSectorY` / `l_GetDefaultArrivalSectorY` | Y сектора прибытия (две регистрации одного имени) |
| `SetDefaultArrivalSectorX` | `l_SetubDefaultArrivalSectorX` | Установить X сектора прибытия |
| `SetDefaultArrivalSectorY` | `l_SetubDefaultArrivalSectorY` | Установить Y сектора прибытия |
| `SetDefaultArrivalSector` | `l_SetDefaultArrivalSector` | Установить сектор прибытия целиком |
| `GetDefaultArrivalSector` | `l_GetDefaultArrivalSector` | Получить сектор прибытия |
| `SetDefaultArrivalGridNo` | `l_SetMercArrivalLocation` | Установить клетку прибытия наёмника |
| `SetTurnTimeLimit` | `l_fTurnTimeLimit` | Ограничение времени хода |
| `ReStartingGame` | `l_ReStartingGame` | Флаг перезапуска игры |
| `RepairmanIsFixingItemsButNoneAreDoneYet` | `l_RepairmanIsFixingItemsButNoneAreDoneYet` | Ремонтник ещё чинит предметы |

## Модуль: Инициализация кампании/профиля (Initial*)

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `InitialProfile` | `l_InitProfile` | Первичная инициализация профиля |
| `InitialHeliGridNo1`..`InitialHeliGridNo7` | `l_InitMercgridNo0`..`l_InitMercgridNo6` | Ячейки десантирования вертолёта (1–7) |
| `InitialJerryGridNo` | `l_InitJerryGridNo` | Ячейка Джерри |
| `InitialLaptopQuest` | `l_setLaptopQuest` | Инициализация квеста с ноутбуком |
| `InitialHeliCrash` | `l_setInGameHeliCrash` | Флаг крушения вертолёта |
| `InitialJerryQuotes` | `l_setJerryQuotes` | Флаг реплик Джерри |
| `InitialJerry` | `l_setInJerry` | Флаг присутствия Джерри |
| `InitialHeli` | `l_setInGameHeli` | Флаг вертолёта в игре |
| `InitMercFace` | `l_InitFace` | Инициализация лица наёмника |
| `AddProfileToMap` | `l_InitMapProfil` | Добавить профиль на карту сектора |
| `SetKeyProfile` | `l_SetKeySoldier` | Установить «ключевого» солдата (тактика) |

## Модуль: UB / Ja25 (Unfinished Business)

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `UB_GetManuelID` | `l_Ja25MANUEL_UB` | ID персонажа Мануэль |
| `UB_GetBiggensID` | `l_Ja25BIGGENS_UB` | ID персонажа Биггенс |
| `UB_GetJohnID` | `l_Ja25JOHN_K_UB` | ID персонажа Джон К. |
| `UB_GetTexID` | `l_Ja25TEX_UB` | ID персонажа Текс |
| `UB_GetStogieID` | `l_Ja25STOGIE_UB` | ID персонажа Стиги |
| `UB_GetGastonID` | `l_Ja25GASTON_UB` | ID персонажа Гастон |
| `UB_GetJerryID` | `l_Ja2JERRY_MILO_UB` | ID персонажа Джерри Мило |
| `UB_GetPgmale4ID` | `l_Ja25PGMALE4_UB` | ID наёмника PGMALE4 |
| `UB_GetBettyID` | `l_Ja25BETTY_UB` | ID персонажа Бетти |
| `UB_GetRaulID` | `l_Ja25RAUL_UB` | ID персонажа Рауль |
| `UB_GetMorrisID` | `l_Ja25MORRIS_UB` | ID персонажа Моррис |
| `UB_GetRudyID` | `l_Ja25RUDY_UB` | ID персонажа Руди |
| `Ja25JohnKulbaIsInGame` / `UB_JohnKulbaIsInGame` | `l_Ja25SaveStructJohnKulbaIsInGame` | Джон Кульба в игре (два имени) |
| `Ja25CheckJohnKulbaIsInGame` / `UB_CheckJohnKulbaIsInGame` | `l_Ja25SaveCheckStructJohnKulbaIsInGame` | Проверка наличия Джона Кульбы (два имени) |
| `Ja25JohnKulbaInitialSectorX` / `UB_JohnKulbaInitialSectorX` | `l_Ja25SaveStructubJohnKulbaInitialSectorX` | Начальный сектор X Джона Кульбы (два имени) |
| `Ja25JohnKulbaInitialSectorY` / `UB_JohnKulbaInitialSectorY` | `l_Ja25SaveStructubJohnKulbaInitialSectorY` | Начальный сектор Y Джона Кульбы (два имени) |
| `SetNumberJa25EnemiesInSurfaceSector` / `UB_SetNumberJa25EnemiesInSurfaceSector` | `l_SetNumberJa25EnemiesInSurfaceSector` | Количество врагов в наземном секторе UB (два имени) |
| `SetNumberOfJa25BloodCatsInSector` / `UB_SetNumberOfJa25BloodCatsInSector` | `l_SetNumberOfJa25BloodCatsInSector` | Количество кровокотов в секторе UB (два имени) |
| `HasNpcSaidQuoteBefore` / `UB_HasNpcSaidQuoteBefore` | `l_HasNpcSaidQuoteBefore` | Произносил ли NPC цитату ранее (два имени) |
| `ShouldThePlayerStopWhenWalkingOnBiggensActionItem` / `UB_ShouldThePlayerStopWhenWalkingOnBiggensActionItem` | `l_ShouldThePlayerStopWhenWalkingOnBiggensActionItem` | Остановка игрока на предмете Биггенса (два имени) |
| `HandleSeeingPowerGenFan` / `UB_HandleSeeingPowerGenFan` | `l_HandleSeeingPowerGenFan` | Обнаружение вентилятора генератора (два имени) |
| `HandleSwitchToOpenFortifiedDoor` / `UB_HandleSwitchToOpenFortifiedDoor` | `l_HandleSwitchToOpenFortifiedDoor` | Открытие укреплённой двери (два имени) |
| `HandleSeeingFortifiedDoor` / `UB_HandleSeeingFortifiedDoor` | `l_HandleSeeingFortifiedDoor` | Обнаружение укреплённой двери (два имени) |
| `HandlePlayerHittingSwitchToLaunchMissles` / `UB_HandlePlayerHittingSwitchToLaunchMissles` | `l_HandlePlayerHittingSwitchToLaunchMissles` | Игрок активировал переключатель запуска ракет (два имени) |
| `HavePersonAtGridnoStop` / `UB_HavePersonAtGridnoStop` | `l_HavePersonAtGridnoStop` | Остановить персонажа в клетке (два имени) |
| `UB_InitialHeliGridNo1`..`UB_InitialHeliGridNo7` | `l_InitMercgridNo0`..`l_InitMercgridNo6` | Ячейки вертолёта UB (дубли Initial*) |
| `UB_InitialJerryGridNo` | `l_InitJerryGridNo` | Ячейка Джерри (UB) |
| `UB_InitialLaptopQuest` | `l_setLaptopQuest` | Квест ноутбука (UB) |
| `UB_InitialHeliCrash` | `l_setInGameHeliCrash` | Крушение вертолёта (UB) |
| `UB_InitialJerryQuotes` | `l_setJerryQuotes` | Реплики Джерри (UB) |
| `UB_InitialJerry` | `l_setInJerry` | Джерри в игре (UB) |
| `UB_InitialHeli` | `l_setInGameHeli` | Вертолёт (UB) |

## Модуль: Прочее

| Имя функции в Lua | Функция в C++ | Описание / Назначение |
| :--- | :--- | :--- |
| `WhoIsThere2` | `l_WhoIsThere2` | Кто находится в клетке (второй вариант) |
| `WhoIs` | `l_WhoIs` | Кто находится в клетке (основной вариант) |

---

## Примечания к части 2

1. Многие имена регистрируются **дважды** на разные C++-функции (например, `CheckSoldierNeutral` → `l_CheckSoldierNeutral` и `l_bNeutral`; `GetDefaultArrivalSectorX` → два варианта). При загрузке поздняя регистрация **перезаписывает** раннюю, поэтому фактически доступна последняя по порядку выполнения `IniFunction`.
2. Имена, вызываемые из C++ (обратные вызовы Lua-скриптов), перечислены в разделе «Функции, вызываемые из C++» выше.
3. Полный список `l_*`-реализаций находится в `Strategic\LuaInitNPCs.cpp` (~12 940 строк); регистрация — в `IniFunction()` (строка ~937) и в скриптовых обёртках (`LuaCheckFact`, `LetLuaInterfaceDialogue`, `LetLuaPerformItemAction`, `LetLuaHandleEarlyMorningEvents` и др.).
