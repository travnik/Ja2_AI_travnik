# Lua API — файл: lua\lwstring.cpp

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md). Поддержка широких (UTF-16) строк.

## Регистрация

Таблица методов `WStringMethods[]` (тип `luaL_Reg`) регистрируется в `lua\lua.cpp` → `InitializeLua()` вызовом `CreateLuaType(L, "wstring", WStringMethods)` + `lua_setglobal(L, "wstring")`.

| Имя в Lua | Функция в C++ | Описание / Параметры |
| :--- | :--- | :--- |
| `wstring.fromUTF8(str)` | `LuaWStringFromUTF8` | Конструктор: создаёт wstring из UTF-8 строки (`MultiByteToWideChar`) |
| `s:byte([start[, stop]])` | `LuaWStringByte` | Коды символов диапазона (по умолчанию символ 1); вне границ — ничего не возвращает |
| `wstring.char(...)` | `LuaWStringChar` | Конструктор: собирает wstring из кодов символов |
| `s:find(...)` | `LuaWStringFind` | Заглушка (тело пустое, возвращает `nil`) — аналог `string.find` |
| `s:format(...)` | `LuaWStringFormat` | Заглушка — аналог `string.format` |
| `s:gmatch(...)` | `LuaWStringGMatch` | Заглушка — аналог `string.gmatch` |
| `s:gsub(...)` | `LuaWStringGSub` | Заглушка — аналог `string.gsub` |
| `s:len()` | `LuaWStringLen` | Длина строки в символах (`tw->len`) |
| `s:lower()` | `LuaWStringLower` | Нижний регистр (`towlower`) |
| `s:match(...)` | `LuaWStringMatch` | Заглушка — аналог `string.match` |
| `s:rep(n)` | `LuaWStringRep` | Повтор строки `n` раз (отрицательное `n` → 0) |
| `s:reverse()` | `LuaWStringReverse` | Перевёрнутая строка |
| `s:sub(...)` | `LuaWStringSub` | Заглушка — аналог `string.sub` |
| `s:upper()` | `LuaWStringUpper` | Верхний регистр (`towupper`) |
| `s:__tostring` | `LuaWStringToString` | Метаметод: wstring → обычная UTF-8 строка Lua (`WideCharToMultiByte`) |
| `s:__index` | `LuaWStringIndex` | Метаметод: поиск метода в метатаблице; имена `__*` возвращают `nil` |

Вспомогательная функция `luaWS_newlstr(lua_State*, const CHAR16*, size_t)` создаёт userdata `TWString` с метатаблицей `wstring` — используется всеми конструкторами и кодом тактического модуля (`LuaGetSoldierName` и др.).
