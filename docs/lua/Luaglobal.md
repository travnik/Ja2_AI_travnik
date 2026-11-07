# Lua API — файл: Strategic\Luaglobal.cpp

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md). Экспорт глобальных переменных C++ в Lua.

## Роль файла

Единственная функция `IniGlobalGameSetting(lua_State *L)` (строка 46) экспортирует ~120 глобальных переменных через `lua_pushinteger`/`lua_pushboolean` + `lua_setglobal`. Вызывается из всех скриптовых обёрток `Strategic\LuaInitNPCs.cpp` перед выполнением скрипта.

## Экспортируемые переменные

### Общее состояние

| Имя в Lua | Описание |
| :--- | :--- |
| `giNPCReferenceCount` | Счётчик ссылок на NPC |
| `gubWaitingForAllMercsToExitCode` | Код ожидания выхода всех наёмников |
| `gWorldSectorX` / `gWorldSectorY` / `gbWorldSectorZ` | Текущий мировой сектор |
| `sSelMapX` / `sSelMapY` / `iCurrentMapSectorZ` | Выбранный сектор на карте |
| `is_networked` | Признак сетевой игры |
| `gbPlayerNum` | Номер игрока (мультиплеер) |
| `guiNumWorldItems` | Количество предметов в мире |
| `guiDay` / `guiHour` / `guiMin` / `guiCurrentScreen` | Время и активный экран |
| `cHour` / `cDay` / `cMin` | Час/день/минута (`uiHourLua`, `uiDayLua`, `uiMinLua`) |

### Настройки игры (префикс `new*`, старые имена без префикса)

| Имя в Lua | Описание |
| :--- | :--- |
| `newGAME_STYLE` / `gameStyle` | Стиль игры (`gGameOptions.ubGameStyle`) |
| `newDIFFICULTY_LEVEL` / `difficultyLevel` | Уровень сложности |
| `difficultyLevel` *(JA2UB)* | Дубль сложности для UB |

### Настройки ja2_options.ini (префикс `ini*`)

| Имя в Lua | Описание |
| :--- | :--- |
| `iniENABLE_CREPITUS` / `enableCrepitus` | Креатуры включены |
| `iniENEMY_INVESTIGATE_SECTOR` | Расследование сектора врагом (не используется) |
| `iniMERC_WEBSITE_IMMEDIATELY_AVAILABLE` / `fMercDayOne` | AIM доступен с первого дня |
| `iniCHANCE_TONY_AVAILABLE` | Шанс доступности Тони |
| `iniSTARTING_CASH_NOVICE/_EXPERIENCED/_EXPERT/_INSANE` (+ старые `startingCash*`) | Начальные деньги по уровням |
| `iniCAN_TRUE_CIVILIANS_BECOME_HOSTILE` / `fCanTrueCiviliansBecomeHostile` | Враждебность мирных (не используется) |
| `iniNEW_MERC_ARRIVAL_LOCATION` | Стартовая ячейка прибытия наёмника |
| `iniDEFAULT_ARRIVAL_SECTOR_X/_Y` (+ старые `ubDefaultArrivalSectorX/Y`) | Сектор прибытия по умолчанию |

### Координаты мода (Mod Setting.ini, префикс `ini*`)

| Имя в Lua | Описание |
| :--- | :--- |
| `iniHIDEOUT_SECTOR_X/_Y/_Z`, `iniHIDEOUT_EXIT`, `iniHIDEOUT_SURFACE_X/_Y/_Z`, `iniHIDEOUT_ENTRY` | Убежище |
| `iniBASEMENT_ENTRY_1..10`, `iniBASEMENT_EXIT_1..12` | Входы/выходы подвала |
| `iniFINAL_CRATE_GRID`, `iniCRATE_TILE_DEF`, `iniTRAPDOOR_TILE_DEF` | Финальный ящик, тайлы ящика/люка |
| `iniPORN_SHOP_ROOM_HANS/_BRENDA/_TONY`, `iniHANS_POSTION`, `iniANGELS_LEATHERSHOP` | Сан-Мона: магазины |
| `iniDOOR_TO_BAMBIS_ROOM/_CARLAS_ROOM/_CINDYS_ROOM/_MARIAS_ROOM` | Двери комнат публичного дома |
| `iniFIRST_ROOM_IN_BROTHEL`, `iniLAST_ROOM_IN_BROTHEL`, `iniBROTHEL_GUARD_ROOM`, `iniBROTHEL_DOOR_1..3` | Публичный дом |
| `iniKINGPIN_MONEY_SECTOR_X/_Y/_Z` | Сектор денег Кингпина |
| `iniOMERTA_DROPOFF_X/_Y/_Z`, `iniOMERTA_DROPOFF_GRIDNO` | Выдача снаряжения в Омёрте |
| `iniROCKETRIFLE_POSITION` | Ракетная винтовка (Альма) |
| `iniHOSPITAL_SECTOR_X/_Y/_Z` | Больница |
| `iniADD_MADLAB_SECTOR_1_X/_Y .. 4_X/_Y` | Дополнительные сектора Мэдлаба |

### Состояние больницы/бокса/музыки/вертолёта

| Имя в Lua | Описание |
| :--- | :--- |
| `giHospitalTempBalance`, `gbHospitalPriceModifier`, `giHospitalRefund` | Больница |
| `gfBoxersResting`, `gubBoxersRests`, `gubBoxingMatchesWon` | Бокс |
| `uiMusicHandle`, `gubMusicMode`, `fMusicPlaying` | Музыкальный движок |
| `guiHelicopterSkyriderTalkState`, `guiTimeOfLastSkyriderMonologue` | Диалоги Скайдрайвера |
| `gubBloodcatLairSectorId`, `giLairID`, `gubSrcSoldierProfile` | Логова кровокотов, профиль-источник |
| `gfInvestigateSector` | Расследование сектора |
| `iFirstArrivalDelay` | Задержка первого прибытия |
| `gfEnteringMapScreen` | Вход на карту |
| `guiIntroExitScreen`, `gfIntroScreenExit`, `gbIntroScreenMode`, `gfDoneWithSplashScreen` | Экраны интро/заставки |

### Блок JA2UB (`#ifdef JA2UB`)

| Имя в Lua | Описание |
| :--- | :--- |
| `TestUB` | Отладочный флаг |
| `iniDEFAULT_END_SECTOR_X/_Y/_Z` и `UB_iniDEFAULT_END_SECTOR_X/_Y/_Z` | Финальный сектор (старые/новые имена) |
| `iniINITIALHELIGRIDNO1..7` и `UB_iniINITIALHELIGRIDNO1..7` | Ячейки вертолёта |
| `iniJERRYGRIDNO`/`UB_iniJERRYGRIDNO`, `iniLAPTOP_QUEST`/`UB_iniLAPTOP_QUEST` | Джерри, квест ноутбука |
| `iniJERRY`/`UB_iniJERRY`, `iniJERRYQUOTES`/`UB_iniJERRYQUOTES` | Джерри в игре, его реплики |
| `iniINGAMEHELICRASH`/`UB_iniINGAMEHELICRASH`, `iniINGAMEHELI`/`UB_iniINGAMEHELI` | Вертолёт/крушение |
| `enabledJohnAndTex`/`UB_enabledJohnAndTex`, `RandomManuelText`/`UB_RandomManuelText` | Джон и Текс, реплики Мануэля |
| `itemBARRETT_UB`, `itemCALICO_960_UB`, `itemPSG1_UB`, `itemL85_UB`, `itemTAR21_UB`, `itemVAL_SILENT_UB`, `itemMICRO_UZI_UB`, `itemCALICO_950_UB`, `itemCALICO_900_UB`, `itemCLIP_CANNON_BALL`, `itemMERC_UMBRELLA`, `itemHAND_CANNON`, `itemHARTFORD_6_SHOOTER`, `itemTEX_MOVIE_ATTACK_CLYDESDALES`, `itemTEX_MOVIE_WILD_EAST`, `itemTEX_MOVIE_HAVE_HONDA`, `itemLAPTOP_TRANSMITTER`, `itemCHE_GUEVARA_CANTEEN`, `itemMERC_WRISTWATCH`, `itemSAM_GARVER_COMBAT_KNIFE`, `itemMERC_UMBRELLA_OLD`, `itemMORRIS_INSTRUCTION_NOTE` | Номера предметов UB (старые имена) |
| `UB_itemBARRETT`, `UB_itemCALICO_960`, `UB_itemPSG1`, `UB_itemL85`, `UB_itemTAR21`, `UB_itemVAL_SILENT`, `UB_itemMICRO_UZI`, `UB_itemCALICO_950`, `UB_itemCALICO_900`, `UB_itemCLIP_CANNON_BALL`, `UB_itemMERC_UMBRELLA`, `UB_itemHAND_CANNON`, `UB_itemHARTFORD_6_SHOOTER`, `UB_itemTEX_MOVIE_*`, `UB_itemLAPTOP_TRANSMITTER`, `UB_itemCHE_GUEVARA_CANTEEN`, `UB_itemMERC_WRISTWATCH`, `UB_itemSAM_GARVER_COMBAT_KNIFE`, `UB_itemMERC_UMBRELLA_OLD`, `UB_itemMORRIS_INSTRUCTION_NOTE` | Те же предметы (новые имена) |
