# Lua API — файл: lua\lua_env.cpp

> Часть справочника [Lua_API_Reference.md](../Lua_API_Reference.md). Модуль окружения интерпретатора.

## Регистрация

`LuaEnvironmentSetup(lua_State *L)` (вызывается из `lua\lua.cpp` → `InitializeLua()`):
создаётся пустая таблица, ей назначается метатаблица класса `ja2_EnvironmentClass` (`CreateLuaClass`), таблица публикуется как глобальный объект **`Environment`**.

## Свойства и методы объекта `Environment`

| Имя в Lua | Функция в C++ | Тип | Описание / Параметры |
| :--- | :--- | :--- | :--- |
| `Environment.rain` | `LuaGetRainVal` / `LuaSetRainVal` | свойство (чт./зап.) | Дождь `guiEnvWeather`; запись: 0 → `EnvEndRainStorm()`, 1..2 → `EnvBeginRainStorm(level-1)` |
| `Environment.ambient` | `LuaGetAmbient` / `LuaSetAmbient` | свойство (чт./зап.) | Освещённость `gubEnvLightValue` (0..255); вне подвала/пещер — `gfDoLighting = TRUE` |
| `Environment.nightlights(on)` | `LuaSetNightLights` | метод | `on != 0` → `TurnOnNightLights()`, иначе `TurnOffNightLights()` |
| `Environment.primelights(on)` | `LuaSetPrimeLights` | метод | `on != 0` → `TurnOnPrimeLights()`, иначе `TurnOffPrimeLights()` |
