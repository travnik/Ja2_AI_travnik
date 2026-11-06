# SQUAD_BIVOUAC — Техническое описание (для разработки)

_Документ для разработчиков. Дата: 06.11.2026. Соответствует ветке `merge/revision-3290` (v2.3)._
_Пользовательское описание фичи: см. [SQUAD_BIVOUAC.md](SQUAD_BIVOUAC.md)._

_Изменения 2.3: центрирование кнопок панели бивуака («Выгрузить всё» + «Готово») по
горизонтали и вертикали под сеткой слотов; блокировка тактического выхода из сектора
отрядом с непустым бивуаком (ранее выход «телепортировал» отряд мимо guard'а — мерк
спавнился в чужой точке вставки); автоматическая выгрузка бивуака в сектор при потере
отрядом последнего бойца — увольнение, истечение контракта (`StrategicRemoveMerc`) и
смерть последнего бойца (`StrategicHandlePlayerTeamMercDeath`) через новый метод
`AutoDumpBivouacOnSquadLoss()` (без YES/NO-диалога)._

_Изменения 2.2: кнопка «Выгрузить всё» на панели (DumpToFloor через UI); кнопка
открытия/закрытия бивуака в тулбаре секторного инвентаря; красная сетка на
ненастроенных слотах; независимые параметры штрафа пути/усталости; ограничение
передвижения отряда с непустым бивуаком (BIVOUAC_MOVE_MIN_MERCS); валидация кладки
по live-состоянию stash панели; исправлены потери предметов при одновременной работе
панели бивуака и инвентаря сектора; путь BivouacSlots.xml → TableData\Items\._

_Изменения 2.1: конфигурация слотов переведена на NIV-карманы. Вместо абстрактных
`usMaxSize`/`usAllowedClasses` каждый слот ссылается на шаблон кармана `<pPocketIndex>`
из Pockets.xml; вместимость по размеру предмета, маска классов и силуэт слота читаются
напрямую из `LBEPocketType` (структура NIV-карманов этой сборки)._

---

## Оглавление

1. [Компоновка кода](#компоновка-кода)
2. [Модель данных](#модель-данных)
3. [API менеджера](#api-менеджера)
4. [Правила слотов](#правила-слотов)
5. [UI: панель бивуака](#ui-панель-бивуака)
6. [Карта интеграционных хуков](#карта-интеграционных-хуков)
7. [Выгрузка на землю (DumpToFloor)](#выгрузка-на-землю-dumptofloor)
8. [Сериализация](#сериализация)
9. [Инварианты и подводные камни](#инварианты-и-подводные-камни)
10. [Точки расширения / roadmap](#точки-расширения--roadmap)
11. [Чек-лист тестирования](#чек-лист-тестирования)

---

## Компоновка кода

```
Tactical/
  SquadBivouacManager.h        // класс, BivouacData, BivouacSlotConfig, SQBM()
  SquadBivouacManager.cpp      // вся логика фичи (~1300 строк)
  XML_BivouacSlots.cpp         // парсер TableData\Items\BivouacSlots.xml
  Squads.cpp / Squads.h        // хуки: Initialize, перехват роспуска, Save/Load,
                               // gBivouacAllowRemovalOnce, WouldRemovalTriggerBivouacGuard()
  Strategic Exit GUI.cpp       // хук: блокировка тактического выхода (InitSectorExitMenu)
  Tactical All.h               // include SquadBivouacManager.h
Strategic/
  Strategic Movement.cpp       // хуки: штраф времени пути (GetSectorMvtTimeForGroup),
                               // блокировка движения (AddWaypointToPGroup)
  Assignments.cpp              // хуки: усталость в пути (FatigueCharacter),
                               // предпроверки смены назначения + replay
  Merc Contract.cpp            // хук: авто-дамп при уходе насовсем (StrategicRemoveMerc)
  Strategic Merc Handler.cpp   // хук: авто-дамп при смерти (StrategicHandlePlayerTeamMercDeath)
  Map Screen Interface Map Inventory.cpp  // панель бивуака (RenderBivouacPanel, слоты, callback'и)
  mapscreen.cpp                // клавиша B, CreateDestroyBivouacPanel в handle-фазе,
                               // подавление team-list при открытой панели
GameSettings.h / .cpp          // опции BIVOUAC_* в GAME_EXTERNAL_OPTIONS
GameVersion.h                  // SQUAD_BIVOUAC_SAVEGAME = 152
gameloop.cpp                   // SQBM().LoadSlotRules() в InitializeGame()
Utils/_RussianText.cpp         // gzLateLocalizedString[67..82]
Utils/_EnglishText.cpp         // то же, EN
Utils/ExportStrings.cpp        // диапазон экспорта LateLocalized = 82
Tactical/Tactical_VS2010.vcxproj / Tactical_VS2013.vcxproj (+ .filters)  // XML_BivouacSlots.cpp
```

Проекты решения: бивуак-менеджер и его XML-парсер компилируются в **Tactical.lib**; Strategic и ja2 ссылаются на него через заголовок. Include-путь `..\Tactical` уже есть у всех проектов.

⚠️ При сборке отдельного vcxproj вне решения MSBuild подставляет `$(SolutionDir)` = папка проекта: lib ложится в `Tactical\lib\...`, а ja2 линкуется с `lib\VS2013\...`. Собирать с явным `/p:SolutionDir=...`.

---

## Модель данных

### BivouacData (POD, сериализуемый)

```cpp
// SquadBivouacManager.h
#define BIVOUAC_NUM_SLOTS               30
#define BIVOUAC_SLOT_POCKET_NONE        0xFF     // слот не настроен (отклоняет всё)

struct BivouacData
{
    OBJECTTYPE Slots[ BIVOUAC_NUM_SLOTS ];  // .initialize() обнуляет
    void initialize();
};

// per-slot конфигурация: ссылка на шаблон кармана NIV
struct BivouacSlotConfig
{
    UINT8 pPocketIndex;         // pIndex из Pockets.xml; 0xFF = слот не настроен
};

// глобальный per-slot конфиг (определён в SquadBivouacManager.cpp)
extern BivouacSlotConfig gBivouacSlotsConfig[ BIVOUAC_NUM_SLOTS ];
```

- Один экземпляр BivouacData на отряд. `OBJECTTYPE` — стандартный JA2-тип предмета (стеки, насадки, LBE — всё внутри).
- «Слот занят» = `Slots[i].exists()`.
- **Не хранится**: позиция предметов, ownership, флаги мира, конфигурация слотов (конфиг — не игровые данные).
- Конфигурация слотов — **глобальный статический массив**, не член класса: она одинакова для всех отрядов и не сериализуется.

### NIV-карманы (системная структура)

В сборке 7609+AI шаблоны карманов NIV — это `std::vector<POCKETTYPE> LBEPocketType`
(`Tactical/Item Types.h`, заполняется из `TableData\Items\Pockets.xml` в `Init.cpp`,
`LoadExternalGameplayData`). Используемые поля:

| Поле POCKETTYPE | Тип | Роль в бивуаке |
|---|---|---|
| `pIndex` | UINT16 | индекс, на который ссылается `<pPocketIndex>` слота |
| `ItemCapacityPerSize` | `std::vector<UINT8>` | вместимость по размеру предмета: `ItemCapacityPerSize[size]`; 0 = предмет такого размера не помещается |
| `pRestriction` | UINT32 | маска классов IC_*; 0 = любые классы |
| `pSilhouette` | UINT8 | индекс спрайта в `INTERFACE\Silhouettes.sti` (guiSILHOUETTE) для фона пустого слота |

Внимание: `ItemCapacityPerSize` индексируется размером предмета (`Item[].ItemSize`,
клампится к `gGameExternalOptions.guiMaxItemSize`, как во всём NIV-коде). В текущем
Data набор карманов в основном одномерный (`ItemCapacityPerSize1 = 1`): предмет размера 1
помещается 1 шт., остальные — 0. Желаемые вместимости («17 для АК, 66 для патронов»)
задаются значениями `ItemCapacityPerSizeN` в самих шаблонах Pockets.xml.

### SquadBivouacManager (singleton)

```cpp
class SquadBivouacManager
{
    BivouacData     m_Bivouac[ NUMBER_OF_SQUADS ];   // NUMBER_OF_SQUADS = enum в Squads.h (20)
    BOOLEAN         m_fBivouacUIMode;                // UI-режим включён
    INT32           m_iPendingDisbandSquad;          // отряд в диалоге роспуска (-1 = нет)
    UINT16          m_usPendingDisbandSoldierID;
    PendingAssignChange m_PendingAssignChange;       // отложенная смена назначения
    INT32           m_iActiveUISquad;                // отряд, показанный в панели (-1 = нет)
    INT32           m_iValidationSquad;              // отряд для проверки слота в CanItemFitInBivouacSlot (-1 = m_iActiveUISquad)
};
```

Доступ: `SQBM()` → `SquadBivouacManager::Instance()`.

### Конфигурация (gGameExternalOptions, читается в LoadGameExternalOptions)

| Поле | Тип | ini-ключ | default |
|---|---|---|---|
| `usBivouacMinMercsRequired` | UINT16 | `BIVOUAC_MIN_MERCS_REQUIRED` | 1 |
| `uiBivouacWeightCapacity` | UINT32 | `BIVOUAC_WEIGHT_CAPACITY` | 2000 |
| `usBivouacMovementPenaltyMultiplier` | UINT16 | `BIVOUAC_MOVEMENT_PENALTY_MULTIPLIER` | 15 |
| `usBivouacBreathPenaltyMultiplier` | UINT16 | `BIVOUAC_BREATH_PENALTY_MULTIPLIER` | 15 |
| `usBivouacMoveMinMercs` | UINT16 | `BIVOUAC_MOVE_MIN_MERCS` | 2 |
| `usBivouacSlotDefaultPocket` | UINT16 | `BIVOUAC_SLOT_DEFAULT_POCKET` | 255 |

Старые ключи `BIVOUAC_SLOT_DEFAULT_MAX_SIZE` и `BIVOUAC_SLOT_DEFAULT_CLASSES` удалены —
их заменила ссылка на шаблон кармана (ограничения размера и классов теперь живут в
Pockets.xml).

---

## API менеджера

```cpp
// --- Жизненный цикл ---
void    Initialize();                 // wipe всех отрядов + сброс UI-состояния (НЕ читает ini)

// --- Инвентарь ---
BOOLEAN IsEmpty( INT32 iSquadID );
BOOLEAN IsAccessAllowed( INT32 iSquadID );
OBJECTTYPE* GetInventoryPointer( INT32 iSquadID );  // &Slots[0] (задел под UI)
void    DumpToFloor( INT32 iSquadID, INT16 sMapX, INT16 sMapY, INT8 bMapZ, INT32 sGridNo );

// --- Правила слотов (NIV) ---
void    LoadSlotRules();     // ini-дефолт + TableData\Items\BivouacSlots.xml (вызов из InitializeGame)
BOOLEAN IsSlotConfigured( UINT32 uiSlot ) const;        // слот ссылается на валидный карман
UINT8   GetSlotCapacityForSize( UINT32 uiSlot, UINT8 ubItemSize ) const; // ItemCapacityPerSize[size]
BOOLEAN	CanSlotAcceptItem( INT32 iSquadID, UINT32 uiSlot, OBJECTTYPE* pObj ) const;
int     CanItemFitInBivouacSlot( UINT8 ubSlotIndex, OBJECTTYPE* pObject, UINT8 ubCountToAdd, OBJECTTYPE* pCurrentSlotItem );
BOOLEAN CanAnySlotAcceptItem( INT32 iSquadID, OBJECTTYPE* pObj ) const;
BivouacSlotConfig GetSlotConfig( UINT32 uiSlot ) const;
void    SetSlotConfig( UINT32 uiSlot, const BivouacSlotConfig& config );  // XML-загрузчик
void    SetValidationSquad( INT32 iSquadID );   // отряд для проверки состояния слота
BOOLEAN CanSquadMoveWithBivouac( INT32 iSquadID );   // пустой бивуак = TRUE; непустой требует BIVOUAC_MOVE_MIN_MERCS бойцов

// --- Перегруз ---
UINT32  GetTotalWeight( INT32 iSquadID );       // Σ CalculateObjectWeight()
UINT32  GetWeightCapacity( INT32 iSquadID );    // ini-база + Σ свободной грузоподъёмности бойцов
UINT32  GetMercsMaxCarryWeight( INT32 iSquadID );   // информационные
UINT32  GetMercsCarriedWeight( INT32 iSquadID );
UINT32  GetMercsFreeWeight( INT32 iSquadID );
FLOAT   GetOverloadRatio( INT32 iSquadID );         // вес/ёмкость, clamp >= 1.0
BOOLEAN IsOverloaded( INT32 iSquadID );
INT32   GetOverloadBreathDrain( INT32 iSquadID );   // 0 если нет перегруза
UINT32  ApplyMovementTimePenalty( UINT32 uiBaseTime, INT32 iSquadID );

// --- Роспуск ---
BOOLEAN HandleLastMercLeavingSquad( INT32 iSquadID, SOLDIERTYPE* pLastSoldier ); // FALSE = блокировать
static void BivouacDisbandMessageBoxCallBack( UINT8 bExitValue );
void    SetPendingDisbandSquad( INT32 iSquadID, UINT16 usSoldierID );
INT32   GetPendingDisbandSquad();

// --- Отложенная смена назначения (replay после YES) ---
struct PendingAssignChange { UINT16 usSoldierID; INT8 bAssignment; INT32 iParam1..3; BOOLEAN fValid; };
void    SetPendingAssignmentChange( SOLDIERTYPE*, INT8 bAssignment, INT32 p1, INT32 p2, INT32 p3 );
BOOLEAN TakePendingAssignmentChange( UINT16&, INT8&, INT32&, INT32&, INT32& );

// --- UI-мост ---
void    BuildBivouacStash( std::vector< WORLDITEM >& vec ); // бивуак -> панель
void    StoreBivouacStash( std::vector< WORLDITEM >& vec ); // панель -> бивуак
BOOLEAN IsBivouacUIModeActive();  void SetBivouacUIMode( BOOLEAN ); void ToggleBivouacUIMode();
INT32   GetBivouacUISquadID();      // из bSelectedInfoChar -> bAssignment
BOOLEAN UpdateBivouacUIModeForSelectedChar( INT8 bSelectedInfoChar );
INT32   GetActiveUISquadID();     void SetActiveUISquadID( INT32 );

// --- Сериализация ---
BOOLEAN Save( HWFILE hFile );  BOOLEAN Load( HWFILE hFile );
```

### Приватные помощники

```cpp
INT32   SquadToDataIndex( INT32 iSquadID ) const;
UINT16  CountLiveMercsInSquad( INT32 iSquadID ) const;
BOOLEAN IsSectorPeacefulAndOurs( INT32 iSquadID ) const;
UINT32  GetMercMaxCarryWeight( SOLDIERTYPE* pSoldier ) const;  // сила + Bodybuilding + фон
```

`IsSectorPeacefulAndOurs`: `SectorSquadIsIn()` → локация; `NumHostilesInSector() > 0` → FALSE; для загруженного сектора — `INCOMBAT` / `fEnemyInSector` → FALSE. Факт «сектор наш» **неявно следует** из того, что отряд игрока стоит в секторе (отдельной проверки контроля нет — при рефакторинге учесть).

`GetMercMaxCarryWeight` воспроизводит обратную сторону `CalculateCarriedWeight()` на пороге 100%: `maxWeight = (5 * strength) / iStrengthToLiftHalfKilo`, включая бонус силы >80, Bodybuilding и carry-фон.

---

## Правила слотов (NIV-карманы)

### Семантика

Каждый слот — это NIV-карман: вместимость по размеру, ограничение классов и силуэт
берутся из шаблона `LBEPocketType[pPocketIndex]`. Полная цепочка проверки:

```cpp
// SquadBivouacManager.cpp: CanItemFitInBivouacSlot( ubSlotIndex, pObject, ubCountToAdd )
CanItemFitInBivouacSlot =
     ubSlotIndex < 30 && pObject != NULL && pObject->exists()          // валидность
  && IsSlotConfigured( ubSlotIndex )                                   // карман назначен и существует
  && LBEPocketType[ubPocketType].ItemCapacityPerSize[ size ] != 0      // габарит помещается
  && ( LBEPocketType[ubPocketType].pRestriction == 0                   // классы не ограничены
       || ( Item[usItem].usItemClass & pRestriction ) )
  && ( слот пуст:  ubCountToAdd <= ItemCapacityPerSize[size]           // вместимость
       || тот же usItem: current + ubCountToAdd <= ItemCapacityPerSize[size]
       || другой предмет: false )
```

где:

- `ubPocketType = gBivouacSlotsConfig[ubSlotIndex].pPocketIndex`
- `size = min( Item[pObject->usItem].ItemSize, gGameExternalOptions.guiMaxItemSize )` —
  кламп идентичен NIV-коду (`Items.cpp`, `ItemSlotLimit`)

Проверяется **базовый предмет** стека (`Item[usItem]`), не вес и не насадки. Занятый слот
правилом не «выталкивается» — правила только на вход (кроме стека того же предмета,
который ограничен остатком вместимости).

`CanSlotAcceptItem( iSquadID, uiSlot, pObj )` — обёртка над `CanItemFitInBivouacSlot`:
передаёт весь стек курсора (`pObj->ubNumberOfObjects`) как `ubCountToAdd` и временно
ставит `m_iValidationSquad = iSquadID`, чтобы проверка состояния слота шла по нужному
отряду (после проверки сбрасывается в -1 → откат на `m_iActiveUISquad`).

### Источники конфигурации (LoadSlotRules, gameloop.cpp → InitializeGame)

1. Все 30 слотов = ini-дефолт `BIVOUAC_SLOT_DEFAULT_POCKET` (255 = слот не настроен).
2. Если существует `TableData\Items\BivouacSlots.xml` — `ReadInBivouacSlotRules()`
   (Tactical/XML_BivouacSlots.cpp) переопределяет слоты по `<uiIndex>` тегом
   `<pPocketIndex>`. Ошибка парсинга → ScreenMsg + ini-дефолты.
3. Конструктор менеджера инициализирует все слоты значением 0xFF (не настроены до LoadSlotRules).

Парсер написан по образцу `XML_LBEPocket.cpp` (expat, PARSE_STAGE); `SetSlotConfig`
пишет напрямую в глобальный `gBivouacSlotsConfig`.

Порядок инициализации критичен: `LoadSlotRules()` выполняется в `InitializeGame()`
(gameloop.cpp:160) — **после** `InitializeJA2()` → `LoadExternalGameplayData()`
(Init.cpp:1505), т.е. `LBEPocketType` уже заполнен из Pockets.xml. Ссылки на карманы
проверяются по `LBEPocketType.size()` в рантайме (`IsSlotConfigured`), а не при загрузке.

Формат BivouacSlots.xml:

```xml
<BIVOUACSLOTLIST>
    <BIVOUACSLOT>
        <uiIndex>0</uiIndex>            <!-- номер слота бивуака 0..29 -->
        <pPocketIndex>4</pPocketIndex>  <!-- pIndex кармана из Pockets.xml -->
    </BIVOUACSLOT>
    <!-- слоты, отсутствующие в файле, получают ini-дефолт -->
</BIVOUACSLOTLIST>
```

### Точки применения в UI (Map Screen Interface Map Inventory.cpp)

| Точка | Поведение |
|---|---|
| `BivouacPanelSlotsCallback`, ЛКМ-кладка в слот | валидация `CanItemFitInBivouacSlot` с live-содержимым из stash панели (`pLiveSlotItem`); запрет с сообщением [76]/[77]/[78]/[79] по причине; предмет остаётся на курсоре; при swap курсор обновляется (`guiExternVo` + `EXTERN_CURSOR`) |
| `BivouacTryPlaceCursorItem` (CTRL+ЛКМ) | стек/кладка только в слоты, прошедшие валидацию (live-состояние stash); иначе [72] «полон» |
| `RenderBivouacPanel`, пустые слоты | фон-силуэт: `LBEPocketType[pPocketIndex].pSilhouette` из `guiSILHOUETTE` (`Silhouettes.sti`), центрирование как в `INVRenderSilhouette()`; не настроенный слот силуэта не имеет |
| `RenderBivouacPanel`, недоступные слоты | тёмный тон (RGB 70,30,25) + приглушённая красная сетка 1px/шаг 5px (RGB 110,45,45) через `ColorFillVideoSurfaceArea` |
| `BivouacSlotDisabled(iSlot)` | TRUE = pPocketIndex == 0xFF или >= LBEPocketType.size(); такие слоты: без silhouette/hover, клик игнорируется (кроме показа [76] с предметом на курсоре) |

---

## UI: панель бивуака

### Архитектура

Панель — **собственный компактный UI** (не fullscreen Sector Inventory):

- `std::vector<WORLDITEM> pBivouacPanelList` — отдельный stash панели (**не static**: `DumpToFloor` читает его для синхронизации правок перед выгрузкой); **не** пересекается с `pInventoryPoolList` секторного инвентаря. Панель и fullscreen-инвентарь могут быть открыты одновременно.
- `fShowBivouacPanel` (extern в Map Screen Interface Map Inventory.h) — флаг панели.
- `gfBivouacPanelCreated` — панель реально построена (flag ставится на кадр раньше построения).

⚠️ **`pBivouacPanelList` ≠ `pInventoryPoolList`.** Хранилища панели бивуака и секторного инвентаря строго раздельны. Синхронизация панели с менеджером — только через `pBivouacPanelList`; использование `pInventoryPoolList` для этого затирает слоты бивуака предметами сектора (этот баг был пойман и исправлен).

### Кнопки панели

Обе кнопки образуют одну группу, центрированную под сеткой слотов **по горизонтали и
вертикали**. Геометрия (`BivouacPanelCreateSlots` + `BivouacPanelGetButtonsY`):

- группа: `[Выгрузить всё 28px] + 6px + [Готово 35px] = 69px`, старт X = `(MAP_BORDER_X − 69) / 2`;
- центр группы совпадает с центром сетки слотов (сетка центрируется той же формулой
  в `BivouacPanelGetSlotCoords`);
- вертикаль: `BivouacPanelGetButtonsY()` вычисляет низ сетки слотов той же формулой,
  что и `BivouacPanelGetSlotCoords`, и центрирует полосу кнопок (28px) между низом
  сетки и нижним краем панели (`BIVOUAC_PANEL_END_Y`); каждая кнопка центрируется по
  своей реальной высоте (28 vs 27) на общей средней линии;
- размеры спрайтов задаются константами `BIVOUAC_PANEL_*_BTN_WIDTH/HEIGHT` — при
  замене графики кнопок править только их.

| Кнопка | Графика | Позиция | Действие |
|---|---|---|---|
| Done | `INTERFACE\itemdescdonebutton.sti` (35×27) | низ панели, правая часть центрированной группы | `CloseBivouacPanel(TRUE)` |
| «Выгрузить всё» | `sector_inventory_buttons.sti` idx 16/14/15 (иконка Stack&Merge, 28×28) | низ панели, левая часть центрированной группы | `BivouacPanelDropAllBtn` → `DumpToFloor` активного отряда |

### Кнопка бивуака в тулбаре секторного инвентаря

`MapInventoryBivouacToggleBtn` — слот 12 массива `guiMapInvenFilterButton` (`MAP_INVENTORY_FILTER_BUTTONS` = 13), графика idx 0/0/1 (иконка Zoom), позиция `INVEN_POOL_X + 441`. Повторяет логику хоткея 'B' из mapscreen.cpp: close при открытой панели, иначе `IsBivouacPanelAllowedForSelectedChar()` → open. Создаётся/удаляется вместе с остальными кнопками секторного инвентаря.

### Жизненный цикл

```
'B' (mapscreen.cpp GetMapKeyboardInput) ИЛИ MapInventoryBivouacToggleBtn (тулбар сектора):
  fShowBivouacPanel ? CloseBivouacPanel(TRUE)
                    : IsBivouacPanelAllowedForSelectedChar() -> fShowBivouacPanel = TRUE
                      + SetBivouacUIMode(TRUE) + SetActiveUISquadID(GetBivouacUISquadID())

CreateDestroyBivouacPanel()  // вызов из HandleMapScreen, рядом с CreateDestroyMapInventoryPoolButtons
  open:  BuildBivouacPanelStash() -> SQBM().BuildBivouacStash(pBivouacPanelList)
         + фон (тайлинг чистой wood-полосы iteminfoc.sti, rows 165..255) + слоты + кнопки
  close: SQBM().StoreBivouacStash(pBivouacPanelList) + SetBivouacUIMode(FALSE)
         + SetActiveUISquadID(-1) + закрытие popup'ов
  каждый кадр (пока создана):
    - маска региона отключается при открытом stack popup / description box
    - авто-закрытие при потере доступа: IsAccessAllowed == FALSE
      или GetBivouacUISquadID() != активный отряд

RenderBivouacPanel()  // из RenderMapRegionBackground, каждый кадр (fMapPanelDirty форсится)
  crash-guard: !gfBivouacPanelCreated || pBivouacPanelList.size() < 30 -> return
  подавление при открытом popup; backdrop, рамка, 30 слотов (guiMapInventoryPoolSlot),
  INVRenderItem + status bar + ShortItemName,
  шапка: [73] "Бивуак отряда N" + вес: [75] prefix + cur (зелёный/красный) + "/max"
  (вес считается по pBivouacPanelList — отражает правки до закрытия панели)
```

### Взаимодействие (BivouacPanelSlotsCallback)

| Действие | Реализация |
|---|---|
| ЛКМ по предмету | `BeginInventoryPoolPtr` (стандартный pickup, SHIFT = весь стек); `iCurrentlyPickedUpBivouacItem = iCounter` |
| ЛКМ кладка в слот | проверка правил слота → `PlaceObjectInInventoryStash(&pWI->object, gpItemPointer, -1, -1)` |
| CTRL+ЛКМ по пустому/занятому слоту | `BivouacTryPlaceCursorItem()` — разложить весь стек курсора по подходящим слотам |
| CTRL+ЛКМ по предмету в бивуаке | перенос в сектор: загруженный → `AddItemToPool` у позиции бойца; незагруженный → `AddWorldItemsToUnLoadedSector` (напрямую, минуя возможно пустой pInventoryPoolList) |
| ПКМ | описание (`MAPInternalInitItemDescriptionBox`) или stack popup |
| DEL(+SHIFT) | debug-удаление |
| CloseBivouacPanel(TRUE) | предмет с курсора возвращается в stash (или `AutoPlaceObjectInInventoryStash` при переполнении), затем stash → слоты |

`iDestSlot/iSrcSlot = -1` в PlaceObjectInInventoryStash обязательны: эти параметры поддерживают ownership в pInventoryPoolList (секторном), к которому панель отношения не имеет.

### mapscreen.cpp — подавление team-list

- `RenderMapRegionBackground`: при открытой панели `fMapPanelDirty = TRUE` каждый кадр (панель постоянно перекрашивается поверх team list).
- `RenderTeamRegionBackground`: при открытой панели team list не рисуется вовсе.

---

## Карта интеграционных хуков

Все хуки помечены комментарием `// SQUAD_BIVOUAC`.

### 1. Tactical/Squads.cpp

```cpp
// InitSquads() — конец функции
SQBM().Initialize();
```
При загрузке сейва Initialize НЕ вызывается (данные из Load).

```cpp
// RemoveCharacterFromSquads(), внутри найденного слота, ДО обнуления Squad[][]
if ( gBivouacAllowRemovalOnce )
{
    gBivouacAllowRemovalOnce = FALSE;   // one-shot bypass от YES-колбэка
}
else if ( !( gTacticalStatus.uiFlags & LOADING_SAVED_GAME ) &&
          pCharacter->stats.bLife > 0 &&
          NumberOfPeopleInSquad( (INT8)iCounterA ) == 1 &&
          !SQBM().HandleLastMercLeavingSquad( iCounterA, pCharacter ) )
{
    return( FALSE );
}
```

Там же:
```cpp
BOOLEAN gBivouacAllowRemovalOnce = FALSE;                    // Squads.cpp/h
BOOLEAN WouldRemovalTriggerBivouacGuard( SOLDIERTYPE* );     // предпроверка для Assignments.cpp
```

```cpp
// SaveSquadInfoToSavedGameFile() / LoadSquadInfoFromSavedGameFile() — после SquadMovementGroups
if( !SQBM().Save( hFile ) ) return( FALSE );   // симметрично Load
```

### 2. Strategic/Assignments.cpp — предпроверки + replay

Каждая точка смены назначения, удаляющая бойца из отряда, защищена **до** изменения состояния:

```cpp
if ( WouldRemovalTriggerBivouacGuard( pSoldier ) )
{
    SQBM().SetPendingAssignmentChange( pSoldier, bAssignment, iParam1, iParam2, iParam3 );
    SQBM().HandleLastMercLeavingSquad( SquadCharacterIsIn( pSoldier ), pSoldier );
    return;
}
```

Защищённые точки (все помечены `SQUAD_BIVOUAC: pre-check`): `SetSoldierAssignment` (центральная: DOCTOR, PATIENT, TRAIN_*, REPAIR, FACILITY_*, SNITCH_*, MOVE_EQUIPMENT, VEHICLE), `SetAssignmentForList`, `SquadMenuBtnCallback`, `AssignmentMenuBtnCallback`, `TrainingMenuMvtCallback`/`AttributeMenuBtnCallback`, `RepairMenuBtnCallback`, `SnitchMenuBtnCallback`, `FacilityAssignmentMenuBtnCallback`, `MoveItemMenuBtnCallback`.

Replay — `BivouacReplayPendingAssignmentChange()` (Assignments.cpp): YES-колбэк ставит `gBivouacAllowRemovalOnce = TRUE`, затем replay вызывает `RemoveCharacterFromSquads + AddCharacterToSquad` (если цель — отряд) или `SetSoldierAssignment(...)`.

⚠️ Без предпроверки заблокированное удаление (`return FALSE`) игнорировалось вызывающим кодом: `bAssignment` менялся, боец оставался в `Squad[][]` — рассинхрон.

### 3. Strategic/Strategic Movement.cpp — GetSectorMvtTimeForGroup()

```cpp
// в конце, перед return iBestTraverseTime
if( pGroup->fPlayer && fFoot && !fCar && !fTruck && !fTracked && !fAir )
{
    for( INT32 iSquad = 0; iSquad < NUMBER_OF_SQUADS; ++iSquad )
    {
        if( SquadMovementGroups[ iSquad ] == pGroup->ubGroupID )
        {
            if( SQBM().IsOverloaded( iSquad ) )
                iBestTraverseTime = (INT32)SQBM().ApplyMovementTimePenalty( (UINT32)iBestTraverseTime, iSquad );
            break;
        }
    }
}
```

### 4. Strategic/Assignments.cpp — FatigueCharacter()

```cpp
// внутри блока if( pSoldier->flags.fBetweenSectors && pSoldier->bAssignment != VEHICLE )
if( pSoldier->bAssignment < ON_DUTY )
{
    INT32 iBivouacDrain = SQBM().GetOverloadBreathDrain( pSoldier->bAssignment );
    if( iBivouacDrain > 0 )
        bMaxBreathLoss = (float)min( 127, bMaxBreathLoss + iBivouacDrain );
}
```

### 5. Strategic/mapscreen.cpp

- `HandleMapScreen`: `CreateDestroyBivouacPanel()` рядом с `CreateDestroyMapInventoryPoolButtons`.
- `GetMapKeyboardInput` case 'b': открытие/закрытие (см. раздел UI).
- Прочие точки: закрытие панели при выходе с карты, входе в бой/тактику, открытии PreBattle и т.п. (`CloseBivouacPanel(TRUE)`, поиск по `fShowBivouacPanel`).

### 5a. Strategic/Strategic Movement.cpp — передвижение с бивуаком

```cpp
// AddWaypointToPGroup(), сразу после валидации pGroup — ДО добавления waypoint'а
if( pGroup->fPlayer && !pGroup->fBetweenSectors && !pGroup->pWaypoints )
{
    for( INT32 iSquad = 0; iSquad < NUMBER_OF_SQUADS; ++iSquad )
    {
        if( SquadMovementGroups[ iSquad ] == pGroup->ubGroupID )
        {
            if( SQBM().CanSquadMoveWithBivouac( iSquad ) == FALSE )
            {
                MapScreenMessage( ..., gzLateLocalizedString[ 80 ], gGameExternalOptions.usBivouacMoveMinMercs );
                return( FALSE );
            }
            break;
        }
    }
}
```
Блокируется только первый waypoint (инициирующий движение); отряд в пути не затрагивается.
⚠️ MapScreenMessage (не ScreenMsg): [80] содержит `%d`, MapScreenMessage форматирует сам;
в `DoMessageBox` строку нужно форматировать через `swprintf` заранее (см. хук 10).

### 6. gameloop.cpp — InitializeGame()

```cpp
SQBM().LoadSlotRules();   // после всех ini-загрузок
```

### 7. GameSettings.cpp — LoadGameExternalOptions()

Шесть `iniReader.ReadInteger("Extended Options", "BIVOUAC_*", ...)` в конце функции.

### 8. SaveLoadGame.cpp

`CloseBivouacPanel(TRUE)` перед сохранением/загрузкой (панель не должна переживать смену мира).

### 9. Map Screen Interface Map Inventory.cpp — тулбар сектора

`CreateMapInventoryButtons()`: кнопка бивуака `guiMapInvenFilterButton[12]`
(`MAP_INVENTORY_FILTER_BUTTONS` = 13) — тоггл панели бивуака, см. раздел UI.

### 10. Tactical/Strategic Exit GUI.cpp — тактический выход из сектора

`InitSectorExitMenu()` — **ранняя проверка до открытия меню выхода** (и до того, как
мерки начнут уходить с карты):

```cpp
if( gusSelectedSoldier != NOBODY && MercPtrs[ gusSelectedSoldier ] != NULL )
{
    extern INT8 SquadMovementGroups[ ];   // локальный extern, как в остальных пользователях

    if( pExitSoldier->bAssignment >= 0 && pExitSoldier->bAssignment < ON_DUTY &&
        pExitSoldier->flags.fBetweenSectors == FALSE &&
        SquadMovementGroups[ pExitSoldier->bAssignment ] != 0 &&
        SQBM().CanSquadMoveWithBivouac( pExitSoldier->bAssignment ) == FALSE )
    {
        // сообщение [80] содержит %d — форматируется через swprintf ДО DoMessageBox
        // (DoMessageBox не форматирует: «голый» %d читает мусор из стека)
        DoMessageBox( MSG_BOX_BASIC_STYLE, zBivouacMoveMsg, GAME_SCREEN, MSG_BOX_FLAG_OK, ... );
        return( TRUE );   // меню выхода не открывается, мерк остаётся на месте
    }
}
```

Без этого хука гард в `AddWaypointToPGroup()` срабатывал слишком поздно: в
`AllMercsHaveWalkedOffSector()` его FALSE игнорировался, и `SetGroupSectorValue()`
телепортировал отряд в соседний сектор → мерк спавнился в точке вставки по
направлению движения вместо своей позиции.

⚠️ Файл собирается с **отключённым PCH** (`PRECOMPILEDHEADERS` не определён) — include
`SquadBivouacManager.h` добавлен в `#else`-ветку включений (см. инвариант 8). Там же
подключён `Tactical All.h`-путь: include добавлен и в `Tactical All.h`.

### 11. Потеря отрядом последнего бойца — авто-дамп бивуака

Центральные точки ухода бойца «насовсем», в начале каждой добавлен авто-дамп:

| Точка | Файл | Покрывает |
|---|---|---|
| `StrategicRemoveMerc()` | Strategic/Merc Contract.cpp | увольнение, истечение контракта, cleanup мёртвых |
| `StrategicHandlePlayerTeamMercDeath()` | Strategic/Strategic Merc Handler.cpp | смерть в тактике / на карте / в авто-резолве |

```cpp
// StrategicRemoveMerc(), в самом начале, ДО RemoveCharacterFromSquads():
if( pSoldier != NULL && pSoldier->bActive &&
    !( gTacticalStatus.uiFlags & LOADING_SAVED_GAME ) &&
    pSoldier->bAssignment >= 0 && pSoldier->bAssignment < ON_DUTY )
{
    if( NumberOfPeopleInSquad( bSquad ) == 1 && SQBM().IsEmpty( bSquad ) == FALSE )
        SQBM().AutoDumpBivouacOnSquadLoss( bSquad );
}

// StrategicHandlePlayerTeamMercDeath(), в самом начале:
// то же, но считает ТОЛЬКО ЖИВЫХ членов отряда (iLive <= 1) и дополнительно
// требует pSoldier->flags.fBetweenSectors == FALSE
```

`AutoDumpBivouacOnSquadLoss(iSquadID)` (новый публичный метод менеджера): пустой
бивуак → тихий выход; отряд между секторами → выход без дампа (нет осмысленного
места сброса, предметы остаются в скрытом бивуаке отряда); иначе —
`DumpToFloor(iSquadID, sX, sY, bZ, -1)` + сообщение [68].

Мотивация: для уходящего «насовсем» бойца YES/NO-диалог не нужен — вещи обязаны
остаться в секторе. Ранний дамп (до `RemoveCharacterFromSquads()`) заодно опустошает
бивуак, поэтому disband-guard внутри проходит молча, а раньше `StrategicRemoveMerc`
игнорировал FALSE от guard'а и продолжал удаление → бивуак оставался у осиротевшего
отряда (предметы «исчезали»). Для мёртвых гард не срабатывал вообще (`bLife > 0`).

---

## Выгрузка на землю (DumpToFloor)

`DumpToFloor(iSquadID, sMapX, sMapY, bMapZ, sGridNo)` — путь «бивуак → мир» (роспуск и кнопка «Выгрузить всё»; CTRL+ЛКМ в панели идёт своим путём). Особые случаи, отработанные в порядке проверки:

1. **Панель бивуака показывает этот отряд** (`fBivouacUIActive && m_iActiveUISquad == iSquadID`):
   - перед дампом `StoreBivouacStash(pBivouacPanelList)` — синхронизировать правки игрока из панели в слоты (**только через pBivouacPanelList, не pInventoryPoolList**);
   - после дампа `BuildBivouacStash(pBivouacPanelList)` — панель показывает пустые слоты, закрытие не «воскресит» выгруженное.
2. **Fullscreen-инвентарь сектора открыт и показывает сектор дампа** (`fShowMapInventoryPool && sSelMapX/Y/Z совпадают` — БЕЗ проверки бивуак-режима, панель бивуака имеет свой stash) — секторное хранилище **затенено** снимком `pInventoryPoolList` (закрытие панели перезапишет хранилище снимком). Предметы добавляются прямо в снимок:
   - заполнение **первых свободных слотов** (не append в конец — тогда предметы видны на текущей странице сразу);
   - загруженный сектор: реальный gridno (`RandomGridNo()` при -1) — `AddItemToPool` спамит beta-warning на gridno=-1; незагруженный: gridno=-1 + `WORLD_ITEM_GRIDNO_NOT_SET_USE_ENTRY_POINT`;
   - `pUnSeenItems.resize(pInventoryPoolList.size())` — не-INVFIX_Moa код требует одинаковых размеров (иначе CTD в `RefreshSeenAndUnseenPools`);
   - **синхронный `ResizeInventoryList()`** — пересчёт страниц/размера до любого закрытия панели: окно сохранения `SaveSeenAndUnseenItems` покрывает только первые `SLOT_COUNT*(iLastInventoryPoolPage+1)` записей, а счётчик страниц обычно обновляется только при рендере;
   - `fMapPanelDirty = TRUE` — немедленная перерисовка;
   - **ранний `return` — перед ним ОБЯЗАТЕЛЬНО перестроить `pBivouacPanelList`**, если панель бивуака показывает этот отряд (иначе закрытие панели бивуака вернёт выгруженные предметы в слоты).
3. **Загруженный сектор** (без затенения): `AddItemToPool(sGridNo|RandomGridNo(), WORLD_ITEM_REACHABLE)`.
4. **Незагруженный сектор**: копия стеков во временный массив (`AddItemsToUnLoadedSector` консьюмит объекты), затем bulk-add; при gridno=-1 — флаг entry point, **не** FIND_SWEETSPOT (pathing от -1 теряет предметы).

---

## Сериализация

```
[UINT32 marker = 1] [UINT32 numSquads] { [UINT32 slotCount=30] [30 × OBJECTTYPE::Save(hFile, false)] } × numSquads
```

- `OBJECTTYPE::Save/Load` — самоописывающий формат стека.
- `Load()`: сначала wipe всех бивуаков, затем строгие проверки (marker, numSquads, slotCount). Любая ошибка → FALSE (данные уже опустошены = безопасный результат).
- Блок вставлен после `SquadMovementGroups`. **Позиция зафиксирована.** Версия сейва с блоком: `SQUAD_BIVOUAC_SAVEGAME = 152` (GameVersion.h).
- Конфигурация слотов (ссылки на NIV-карманы) в сейв не пишется — это конфигурация (ini + BivouacSlots.xml + Pockets.xml), а не игровые данные.

---

## Инварианты и подводные камни

1. **Отряд ↔ группа движения** — через `SquadMovementGroups[iSquad]`.
2. **Типы координат.** `SectorSquadIsIn(INT8, INT16*, INT16*, INT16*)` — четвёртый параметр **INT16** (ловили C2664).
3. **NUMBER_OF_SQUADS — enum**, определён в `Squads.h`; заголовок менеджера включает `Squads.h`.
4. **Роспуск: re-entrancy.** `DoMapMessageBox` асинхронный; предпроверка гарантирует «ничего не изменено до диалога»; `TakePendingAssignmentChange` потребляет запрос до вызова `SetSoldierAssignment`. `PauseGame/LockPauseState(9)` ↔ `UnLockPauseState/UnPauseGame` в колбэке; reason id 9 — проверить при конфликтах.
5. **Снимок панели сектора.** Пока fullscreen-инвентарь открыт, temp-файл/gWorldItems сектора «не существуют» — все записи только через `pInventoryPoolList` (см. DumpToFloor п.2). Размеры `pInventoryPoolList`/`pUnSeenItems` обязаны совпадать.
6. **Окно сохранения панели сектора.** Non-INVFIX_Moa `SaveSeenAndUnseenItems` сохраняет первые `SLOT_COUNT*(iLastInventoryPoolPage+1)` записей — после любого добавления в снимок нужен синхронный `ResizeInventoryList()`.
7. **Копи-семантика стека.** `StoreBivouacStash` присваивает `Slots[i] = vec[i].object` — OBJECTTYPE копирует насадки корректно.
8. **PCH.** SquadBivouacManager.cpp и XML_BivouacSlots.cpp собираются с `"Tactical All.h"`; extra-includes дублировать в `#else`-ветке.
9. **`$(SolutionDir)` при внешней сборке Tactical.vcxproj** — lib уходит в `Tactical\lib\...`, а линкер ждёт `lib\VS2013\...` (см. Компоновка).
10. **Рендер в guiSAVEBUFFER.** Весь текст/предметы панели рисуются после `SetFontDestBuffer(guiSAVEBUFFER, ...)`, иначе теряются при финальном blit (паттерн `RenderItemInPoolSlot`).
11. **Индексация ItemCapacityPerSize.** Всегда клампить размер к `guiMaxItemSize` (паттерн NIV). Вектор заполняется в `Init.cpp` размером `guiMaxItemSize+1`; обращение с большим индексом = UB. Проверка границ кармана (`IsSlotConfigured`) обязательна перед каждым обращением.
12. **m_iValidationSquad — one-shot.** `CanSlotAcceptItem` ставит его перед вызовом `CanItemFitInBivouacSlot` и сбрасывает после; прямые вызовы `CanItemFitInBivouacSlot` без установки валидируются по `m_iActiveUISquad`.
13. **Конфиг слотов глобальный, не сериализуется.** `gBivouacSlotsConfig` общий для всех отрядов и всех сейвов; смена конфигурации действует на существующие сохранённые бивуаки немедленно.
14. **Live-состояние слотов — в stash панели.** Пока панель открыта, менеджер не знает о правках. Валидация кладки обязана получать текущее содержимое слота параметром `pCurrentSlotItem` (объект из `pBivouacPanelList`), а не из `m_Bivouac`.
15. **DumpToFloor: два возвращения.** Ветка затенения секторного инвентаря делает ранний `return` — перед ним ОБЯЗАТЕЛЬНО обновить `pBivouacPanelList` (BuildBivouacStash), иначе закрытие панели бивуака вернёт выгруженные предметы обратно в слоты.
16. **Путь BivouacSlots.xml** — `TableData\Items\BivouacSlots.xml` (как у всех предметных XML), НЕ `Items\...`. VFS не поднимает второй путь — файл бы молча не загружался.
17. **Курсор после swap.** После `PlaceObjectInInventoryStash` с обменом нужно обновить `guiExternVo`/`gusExternVoSubIndex` + `EXTERN_CURSOR`, иначе в руке рисуется старый предмет.
18. **[80] содержит `%d`.** `MapScreenMessage` форматирует сам; `DoMessageBox` / `ScreenMsg` — нет: неформатированный `%d` читает мусор из стека (ловили: «нужно минимум 508487964 бойцов»). В `InitSectorExitMenu` строка форматируется через `swprintf` с `gGameExternalOptions.usBivouacMoveMinMercs` ДО `DoMessageBox`.
19. **Тактический выход — блокировать ДО начала ухода.** Гард в `AddWaypointToPGroup()` не работает для тактического выхода: `AllMercsHaveWalkedOffSector()` игнорирует его FALSE и в любом случае вызывает `RemoveSoldierFromTacticalSector` + `SetGroupSectorValue` (телепорт в соседний сектор). Блокировка — в `InitSectorExitMenu()` до открытия меню.
20. **Авто-дамп при потере последнего бойца — до удаления из отряда.** `AutoDumpBivouacOnSquadLoss` вызывается в начале `StrategicRemoveMerc` / `StrategicHandlePlayerTeamMercDeath`, пока `Squad[][]` и локация отряда ещё валидны. Опустошение бивуака до `RemoveCharacterFromSquads()` делает disband-guard молчаливым. Для мёртвых гард не срабатывал (`bLife > 0`) — потому отдельный хук в смерти со счётом живых членов.
21. **Edge case «отряд в пути»:** `AutoDumpBivouacOnSquadLoss` не выгружает бивуак отряда между секторами (нет осмысленного места сброса) — предметы остаются в скрытом бивуаке осиротевшего отряда до его повторного использования.

---

## Точки расширения / roadmap

### Краткосрочные

**A. BIVOUAC_ENABLED (выключатель фичи)** — флаг в ini + ранняя проверка в IsAccessAllowed/хуке клавиши B.

**B. Логирование Load** — ScreenMsg/DebugMsg при отказе Load() (сейчас тихо).

**C. Тултипы правил слота** — `GetSlotConfig` уже доступен UI; показать в подсказке слота название кармана (`LBEPocketType[p].pName`) и вместимости по размерам.

**D. Индикатор занятости слота по правилу** — ✅ сделано (красная сетка + тон, `BivouacSlotDisabled`).

**K. Валидация ссылок на карманы при LoadSlotRules** — ScreenMsg для слотов, чей `pPocketIndex >= LBEPocketType.size()` (сейчас такие слоты просто отклоняют всё молча).

**L. Скрывать ненастроенные слоты** — сейчас они видны с красной сеткой; вариант — динамическая компоновка только из настроенных слотов (требует пересчёта сетки и mouse-регионов).

### Средний срок

**E. Тактическое представление бивуака** — доступ через диалог наёмника, те же 30 слотов; переиспользует стековую UI, не трогает мир.

**F. Влияние веса на авторезолв** — хук в Auto Resolve.cpp.

**G. Валидация существующих предметов при загрузке правил** — сейчас ужесточение правил не трогает уже лежащие предметы (намеренно); опционально — выгрузка «неправильных» предметов при следующем открытии.

### Долгосрочные

**H. Дифференцированные штрафы по транспорту** — маска в ApplyMovementTimePenalty.

**I. Расширяемые слоты** — `std::vector<OBJECTTYPE>` + slotCount в сейве (формат уже готов).

**J. Бивуак militia/врагов** — вынос storage из singleton по owner-id, новый формат сейва.

### Что не трогать

- Позицию BIVOUAC-блока в сейве относительно SquadMovementGroups.
- Индексную симметрию Build/Store stash (панель не сортирует содержимое).
- Пару PauseGame/LockPauseState ↔ UnLock/UnPause в диалоге роспуска.
- `-1` в iDestSlot/iSrcSlot вызова PlaceObjectInInventoryStash из панели.
- Кламп размера предмета к `guiMaxItemSize` перед индексацией ItemCapacityPerSize.
- One-shot семантику m_iValidationSquad в CanSlotAcceptItem (set → check → reset).

---

## Чек-лист тестирования

### Базовый цикл
- [ ] New game → B на персонаже без отряда — ничего не происходит
- [ ] Персонаж в отряде, мирный сектор → B открывает компактную панель, 30 слотов, шапка с весом
- [ ] Положить/взять предметы, стеки, насадки → закрыть → открыть → всё на месте
- [ ] Вес в шапке меняется сразу при добавлении/удалении; красный при перегрузе
- [ ] ESC закрытие, повторное открытие — корректно
- [ ] Панель + fullscreen-инвентарь сектора открыты одновременно — не мешают друг другу
- [ ] Stack popup / описание поверх панели — не перекрашиваются, маска не глотает клики

### Правила слотов (NIV-карманы)
- [ ] ini `BIVOUAC_SLOT_DEFAULT_POCKET = 255` (дефолт), без XML → все слоты не настроены, любое помещение отклоняется ([76])
- [ ] ini = валидный pIndex (например 4, General Large) → все слоты без XML принимают то, что принимает карман 4
- [ ] BivouacSlots.xml: слот с pPocketIndex=10 (AR Mag) принимает предмет размера 1, отклоняет предмет размера 2+ ([78])
- [ ] Слот с карманом, чей ItemCapacityPerSize[size] = 0, отклоняет предмет этого размера ([78])
- [ ] Слот с карманом с pRestriction != 0 отклоняет предмет чужого класса ([77])
- [ ] Стек: положить в пустой слот вместимости N больше N предметов нельзя; доложить к стеку того же предмета можно до N ([79])
- [ ] Swap: положить другой предмет в занятый слот — предметы меняются местами, курсор показывает предмет из бивуака
- [ ] Деньги кладутся в любой настроенный слот
- [ ] CTRL+ЛКМ кладёт только в подходящие слоты; если нет подходящих — [72]
- [ ] Предмет, лежащий в слоте до смены конфигурации, остаётся на месте
- [ ] Ссылка на несуществующий карман (pPocketIndex >= LBEPocketType.size()) — слот отклоняет всё, без краша
- [ ] Отсутствие BivouacSlots.xml → ini-дефолты; битый XML → сообщение + ini-дефолты

### Силуэты и недоступные слоты
- [ ] Пустой настроенный слот показывает силуэт кармана из Silhouettes.sti
- [ ] Силуэт центрирован в ячейке, не перекрывает соседние слоты
- [ ] Ненастроенный слот (0xFF) — без силуэта, с тёмным тоном + красной сеткой
- [ ] Клик по ненастроенному слоту игнорируется; с предметом на курсоре — сообщение [76]
- [ ] Hover-подсветка на ненастроенном слоте не срабатывает
- [ ] Занятый слот — предмет поверх фона, силуэт не виден

### Доступ
- [ ] Отряд в движении → B не открывает; открытая панель закрывается сама
- [ ] Враги в секторе / бой → не открывает / закрывается
- [ ] BIVOUAC_MIN_MERCS_REQUIRED = 3, в отряде 2 → не открывает
- [ ] Переключение выбранного персонажа на другой отряд → панель закрывается

### Перегруз
- [ ] Ёмкость = база + свободная грузоподъёмность бойцов (проверить на силаче и доходяге)
- [ ] Загрузить > ёмкости: время пути растёт по формуле
- [ ] Транспорт в отряде → замедления нет
- [ ] Усталость в пути растёт при перегрузе
- [ ] MOVEMENT_PENALTY_MULTIPLIER = 0 → замедления нет; BREATH_PENALTY_MULTIPLIER = 0 → усталости нет (независимо)

### Передвижение с бивуаком
- [ ] Непустой бивуак, отряд из 1 бойца, MOVE_MIN_MERCS = 2 → движение блокируется ([80])
- [ ] Добавить второго бойца → движение проходит
- [ ] Пустой бивуак → движение без ограничений
- [ ] Отряд уже в пути → движение продолжается без блокировки

### Кнопка «Выгрузить всё»
- [ ] Бивуак закрыт (кнопка Done), секторный инвентарь закрыт → все предметы на земле сектора
- [ ] Бивуак открыт, секторный инвентарь закрыт → всё выгружено, панель пустеет, закрытие панели ничего не возвращает
- [ ] Бивуак открыт, секторный инвентарь открыт → предметы в списке сектора сразу, панель бивуака пустеет, секторные предметы не теряются
- [ ] Сообщение [68] показывается, вес в шапке обнуляется

### Кнопка бивуака в тулбаре сектора
- [ ] Кнопка видна в тулбаре инвентаря сектора (правее Gear Template)
- [ ] Клик при закрытой панели бивуака → открывает (если доступ разрешён), сообщение [69]
- [ ] Клик при открытой панели → закрывает, содержимое сохранено
- [ ] Отказ доступа (враги/в пути/мало бойцов) → тихо, без открытия

### Роспуск
- [ ] Непустой бивуак, переназначить последнего бойца → диалог
- [ ] → YES → предметы в секторе, боец сразу переназначен
- [ ] → NO → боец и назначение без изменений, предметы в бивуаке
- [ ] Роспуск при открытой панели бивуака → панель синхронизирована, всё выгружено, панель пустеет
- [ ] Роспуск при открытом fullscreen-инвентаре сектора → предметы появляются в панели сектора сразу (без переоткрытия), после закрытия панели остаются в секторе
- [ ] Роспуск незагруженного сектора с открытым инвентарём → предметы в temp-файле после закрытия
- [ ] Пустой бивуак → без диалога
- [ ] Смерть последнего бойца в бою → без диалога, бивуак цел

### Увольнение / контракт / смерть (авто-дамп)
- [ ] Уволить последнего бойца отряда с непустым бивуаком → вещи на земле сектора, сообщение [68], YES/NO-диалога нет
- [ ] Истечение контракта последнего бойца → то же самое
- [ ] Смерть последнего бойца в бою → вещи в секторе, сообщение [68]
- [ ] Увольнение бойца из отряда из 2+ человек → бивуак остаётся у отряда, без дампа
- [ ] Пустой бивуак → ничего не происходит, без сообщений
- [ ] Контракт истёк у отряда в пути → вещи остаются в бивуаке (edge case)
- [ ] После авто-дампа вещи видны в секторном инвентаре / на земле при следующем заходе

### Тактический выход с бивуаком
- [ ] Мерк с непустым бивуаком один в отряде → у края сектора OK в меню выхода → messagebox [80] с правильным числом (не «мусорное»), мерк остаётся в секторе
- [ ] Сообщение подставляет значение BIVOUAC_MOVE_MIN_MERCS из ini (по умолчанию 2)
- [ ] Добавить второго бойца → выход через край сектора работает
- [ ] Пустой бивуак → тактический выход без ограничений
- [ ] Выход через exit-grid (подземелья) → тоже блокируется
- [ ] Мерк остался на своей позиции (не в точке вставки по краю) при повторном открытии сектора

### Кнопки панели (компоновка)
- [ ] «Выгрузить всё» + «Готово» стоят по центру панели по горизонтали (группа 28+6+35=69px, старт (261−69)/2)
- [ ] Кнопки центрированы по вертикали между нижним рядом слотов и нижним краем панели
- [ ] Кнопки не перекрывают слоты и нижнюю рамку панели при разных разрешениях (640×480 / 800×600 / 1024×768)
- [ ] Клик по обеим кнопкам работает, hover-подсветка корректна

### Сейвы
- [ ] Сохранить/загрузить — бивуаки восстановлены
- [ ] Старый сейв (<152) → загрузка без краша, бивуаки пусты
- [ ] Save в бою / в движении — бивуак не теряется

### Регресс
- [ ] Обычный Sector Inventory — не затронут
- [ ] InventoryPoolQ — не смешивается с бивуаком
- [ ] Team list рисуется корректно при закрытой панели
- [ ] Кнопки тулбара сектора (zoom/sort/filter/template) работают как раньше
