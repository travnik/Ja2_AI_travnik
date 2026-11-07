# Lua API — файл: lua\lua.cpp

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md). Ядро устаревшего интерпретатора «Lua Interpreter».

## Роль файла

* Точка входа `InitializeLua()`: создаёт глобальный `lua_State *L` (`lua_open()` + `luaL_openlibs`), затем вызывает `LuaTacticalSetup`, `LuaStrategicSetup`, `LuaEnvironmentSetup`.
* Вызывается из `Tactical\Overhead.cpp` (~строка 797); вызов из `gameloop.cpp` закомментирован.
* Содержит систему регистрации классов (не `lua_register`):

| Механизм | Назначение |
| :--- | :--- |
| `CreateLuaType(L, TypeName, luaL_Reg[])` | Создаёт метатаблицу-тип (используется для `wstring`) |
| `CreateLuaClass(L, ClassName, LuaAttrib[])` | Метатаблица класса с `__index` (`LuaGetObject`) / `__newindex` (`LuaSetObject`); атрибуты описываются структурой `LuaAttrib { name; get; set; call; staticcall; }` |
| `NewLuaObject(L, ClsName, void* Ptr)` | Оборачивает указатель на C++-структуру в userdata с метатаблицей класса |
| `EvalLua(const wchar_t*)` | Ввод/выполнение Lua-кода (консоль), конвертация UTF-16 → UTF-8 |
| `ShutdownLua()` | Закрытие Lua-состояния |

## Функции, зарегистрированные в этом файле

| Имя в Lua | Функция в C++ | Описание |
| :--- | :--- | :--- |
| `wstring` (тип) | `CreateLuaType(L, "wstring", WStringMethods)` | Регистрация типа wstring; таблица методов — в `lua\lwstring.cpp` (см. [lua_lwstring.md](lua_lwstring.md)) |
| `__index` (метаметод классов) | `LuaGetObject` | Поиск атрибута по имени/индексу; для `call`-атрибутов возвращает замыкание метода |
| `__newindex` (метаметод классов) | `LuaSetObject` | Запись атрибута; если атрибут без `set` — ошибка «The attribute is not writable» |

## Регистрируемые глобальные объекты

| Имя в Lua | Источник | Описание |
| :--- | :--- | :--- |
| `wstring` | `lua\lwstring.cpp` | Тип широких строк |
| `Soldiers` | `lua\lua_tactical.cpp` (`LuaTacticalSetup`) | Таблица бойцов `MercPtrs[i]` |
| `Sectors` | `lua\lua_strategic.cpp` (`LuaStrategicSetup`) | Таблица 256 секторов `SectorInfo[i]` |
| `MobileGroups` | `lua\lua_strategic.cpp` (`LuaStrategicSetup`) | Класс мобильных групп |
| `Environment` | `lua\lua_env.cpp` (`LuaEnvironmentSetup`) | Объект окружения (дождь, освещение) |
