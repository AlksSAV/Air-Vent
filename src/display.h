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

// Курсор переключается циклически по пяти позициям: t_min, t_max, привод,
// импульс привода и сброс наработки. Пределы температур и параметры привода
// разнесены по страницам, чтобы связанные величины были видны вместе, а
// листание оставалось в пределах одной клавиши Left.
void display_edit(LiquidCrystal& lcd, int16_t t_min, int16_t t_max,
                  bool valve_enabled, uint16_t valve_pulse_s,
                  bool reset_runtime, uint8_t cursor);

#endif  // DISPLAY_H
