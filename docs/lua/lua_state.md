# Lua API — файл: lua\lua_state.cpp / lua_state.h

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md). Инфраструктура состояний Lua.

## Роль файла

Управление долгоживущими и короткоживущими Lua-состояниями. Не регистрирует игровых функций — предоставляет инфраструктуру для остальных модулей.

## Идентификаторы состояний (`namespace lua`)

| ID | Назначение |
| :--- | :--- |
| `LUA_STATE` | Основное состояние |
| `LUA_STATE_STRUCTURE` | Стратегический слой |
| `LUA_STATE_GUI` | Интерфейс |
| `LUA_STATE_STRATEGIC_MINES_AND_UNDERGROUND` | Подземелья и шахты (инициализируется в `Init.cpp`, ~строка 1485) |

## Класс `LuaState` — методы

| Метод | Описание |
| :--- | :--- |
| `LuaState(lua_State*, bool own)` | Принимает сырой `lua_State`; `own = true` — закрывать при уничтожении |
| `LuaState()` | Пустой конструктор (неинициализированное состояние) |
| `static INIT(lua::State, bool init_libs)` | Инициализация долгоживущего состояния по ID (с `luaL_openlibs` при `init_libs`) |
| `static INIT(bool init_libs)` | Создание и возврат короткоживущего состояния |
| `static CLOSE(lua::State)` / `static CLOSE(LuaState)` | Закрытие состояний |
| `static GET(lua::State)` | Получение уже инициализированного состояния |
| `EvalFile(filename)` | Загрузка и выполнение Lua-файла (с логированием ошибок) |
| `EvalString(str, len, id)` | Выполнение строки (ASCII и wchar_t-перегрузки; UTF-16 → UTF-8) |
| `CallFunction(name)` | Вызов глобальной Lua-функции без параметров (устарело; используйте `LuaFunction`) |
| `operator()()` | Доступ к сырому `lua_State*` |
| `registry()` | Доступ к `Registry` |

## Класс `LuaState::Registry`

| Метод | Описание |
| :--- | :--- |
| `CreateCallbackTable()` / `GetCallbackTable()` | Таблица колбэков в `LUA_REGISTRYINDEX` (`__CallBackRegistry`) |
| `SetCallback(name)` / `GetCallback(name)` | Сохранение/получение Lua-колбэка по имени |
| `CreateUserDataTable()` / `GetUserDataTable()` | Таблица пользовательских данных в реестре |

## Связанные классы

* `LuaScopeState` (`lua_state.h`) — RAII: создаёт короткоживущее состояние в конструкторе, закрывает в деструкторе. Используется обёртками скриптов в `Strategic\LuaInitNPCs.cpp`.
* `LuaFunction` (`lua_function.cpp/.h`) — fluent-вызов функций из C++: `Param<T>()` (перегрузки для `std::string`, `double`, `int`, `unsigned int`, `bool`), `TParam(index, value)`, `TableOpen()/TableClose()`, `Call(num_ret)`.
* `LuaTable` (`lua_table.cpp/.h`) — доступ к таблицам: `getValue<T>`, `setValue<T>`, `length()`, `is_valid()`.
* `LuaInterface` / `LuaUserObject` (`lua_class_interface.cpp/.h`) — ООП-экспорт классов в Lua: макросы `DECLARE_LUA_INTERFACE` / `IMPLEMENT_LUA_INTERFACE`, `luaExportClass`, `luaGetTypedUserData`, метаметоды `__index`/`__newindex` (`luaGetObject`/`luaSetObject`), макросы `DECLARE_LUA_CFUNCTION` / `DEFINE_LUA_CFUNCTION`.
