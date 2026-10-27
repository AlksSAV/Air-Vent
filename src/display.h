#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <LiquidCrystal.h>
#include "config.h"
#include "sensors.h"
#include "fan_controller.h"
#include "settings.h"

// Экран переключается сам по таймеру либо клавишей Right.
enum Screen : uint8_t {
  SCREEN_CLIMATE = 0,  // температура и относительная влажность
  SCREEN_ABS,          // абсолютная влажность (ρ)
  SCREEN_SURFACE,      // температура критической поверхности и роса на ней
  SCREEN_BARO,         // влагосодержание (x)
  SCREEN_DELTA,        // dA и dT
  SCREEN_STATUS,       // состояние вентилятора, наработка, уставки
  SCREENS_COUNT
};

void display_init(LiquidCrystal& lcd);

// Обновляет обычный экран. Внутри держит таймер периода и текущий экран.
void display_show(LiquidCrystal& lcd, const SensorData& out,
                  const SensorData& in, const SurfaceData& floor,
                  const BaroData& baro, FanStatus fan, const Settings& st);

// Переключить экран вручную.
void display_next_screen();

// Продлить подсветку и вернуть экран после ручного нажатия: бездействие
// гасит подсветку через DISPLAY_BACKLIGHT_TIMEOUT_MS.
void display_wake();

// Погашена ли подсветка по таймауту бездействия.
bool display_is_asleep();

// Полный перерисов экрана (после выхода из меню).
void display_invalidate();

// Строка ровно на 16 символов с дополнением пробелами. Нужна меню настроек:
// без добивки на экране остаются хвосты от предыдущей страницы.
void display_text(LiquidCrystal& lcd, uint8_t row, const char* text);

// Продлить подсветку из меню: там свой таймаут бездействия, но гаснуть
// подсветка не должна, пока оператор смотрит на настройки.
void display_keep_awake();

#endif  // DISPLAY_H
