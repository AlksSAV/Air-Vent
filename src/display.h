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
  SCREEN_ABS,          // абсолютная влажность и дельта
  SCREEN_STATUS,       // состояние вентилятора, наработка, уставки
  SCREENS_COUNT
};

void display_init(LiquidCrystal& lcd);

// Обновляет обычный экран. Внутри держит таймер периода и текущий экран.
void display_show(LiquidCrystal& lcd, const SensorData& out,
                  const SensorData& in, FanStatus fan, const Settings& st);

// Переключить экран вручную.
void display_next_screen();

// Полный перерисов экрана (после выхода из меню).
void display_invalidate();

// Экран редактирования уставок: курсор 0 = t_min, 1 = t_max.
// Рисуется сразу, без внутреннего таймера, — вызывается на каждое
// изменение из обработчика клавиш.
void display_edit(LiquidCrystal& lcd, int16_t t_min, int16_t t_max,
                  uint8_t cursor);

#endif  // DISPLAY_H
