#include "display.h"
#include "humidity.h"
#include <stdio.h>
#include <string.h>

static Screen s_screen = SCREEN_CLIMATE;
static unsigned long s_last_draw_ms = 0;
static bool s_force_redraw = true;

// avr-libc печатает float только при линковке с -u _printf_float, что стоит
// около 2 КБ flash. Дроби собираем целочисленной арифметикой: тот же вывод
// заметно дешевле.
static void fmt_1dp(char* dst, size_t size, float value, bool force_sign) {
  float magnitude = value < 0.0f ? -value : value;

  // +0.5 — арифметическое округление к ближайшему десятую.
  long tenths = (long)(magnitude * 10.0f + 0.5f);

  // Знак берём уже округлённого значения: -0,04 даёт нулевой tenths и должна
  // печататься как "0.0", иначе у нуля появляется фантомный минус.
  char sign = ' ';
  if (tenths > 0) sign = value < 0.0f ? '-' : '+';
  if (sign == '+' && !force_sign) sign = ' ';

  snprintf(dst, size, "%c%ld.%ld", sign, tenths / 10, tenths % 10);
}

// Все строки печатаются ровно по 16 символов с дополнением пробелами, иначе
// от предыдущего экрана остаются «хвосты».
static void draw_line(LiquidCrystal& lcd, uint8_t row, const char* text) {
  char line[17];
  memset(line, ' ', 16);
  line[16] = '\0';
  strncpy(line, text, 16);
  lcd.setCursor(0, row);
  lcd.print(line);
}

// Короткая причина остановки вентилятора: экран 16 символов, поэтому
// формулировки сжаты до кода состояния.
static const char* block_text(FanStatus fan) {
  if (fan.mode == FAN_FORCE) return "FORCE ON";
  if (fan.mode == FAN_RUNNING) return "FAN ON";
  if (fan.mode == FAN_OPENING) return "OPENING";
  if (fan.mode == FAN_COOLDOWN) return "PAUSE";
  switch (fan.block) {
    case BLOCK_SENSOR: return "OFF:SENS";
    case BLOCK_TEMP_LOW: return "OFF:T-LO";
    case BLOCK_TEMP_HIGH: return "OFF:T-HI";
    case BLOCK_DEW: return "OFF:DEW";
    case BLOCK_DRY: return "OFF:DRY";
    case BLOCK_VALVE: return "OFF:VALVE";
    default: return "OFF";
  }
}

void display_init(LiquidCrystal& lcd) {
  // Подсветка LCD Keypad Shield сидит на D10: без явного HIGH экран рабочий,
  // но тёмный.
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Air-Vent");
  lcd.setCursor(0, 1);
  lcd.print("drying control");
  delay(1500);
  lcd.clear();
  s_force_redraw = true;
}

void display_next_screen() {
  s_screen = (Screen)((s_screen + 1) % SCREENS_COUNT);
  s_force_redraw = true;
}

void display_invalidate() { s_force_redraw = true; }

void display_show(LiquidCrystal& lcd, const SensorData& out,
                  const SensorData& in, const SurfaceData& floor,
                  const BaroData& baro, FanStatus fan, const Settings& st) {
  unsigned long now = millis();
  if (!s_force_redraw && (now - s_last_draw_ms) < DISPLAY_PERIOD_MS) return;
  s_last_draw_ms = now;
  s_force_redraw = false;

  // Принудительный режим и отказ датчика показываем вместе: вентилятор
  // продолжает работать, и экран обязан это отражать, а не прятать за
  // сообщением об ошибке. При исправных данных режим виден на экране статуса.
  if (!out.valid || !in.valid) {
    if (fan.mode == FAN_FORCE) {
      draw_line(lcd, 0, "FORCE ON");
      draw_line(lcd, 1, "sensor error too");
      return;
    }
    draw_line(lcd, 0, "SENSOR ERROR");
    draw_line(lcd, 1, out.valid ? "check indoor" : "check outdoor");
    return;
  }

  char buf[20];
  char v1[8];
  char v2[8];

  switch (s_screen) {
    case SCREEN_CLIMATE:
      fmt_1dp(v1, sizeof(v1), out.temperature, false);
      fmt_1dp(v2, sizeof(v2), in.temperature, false);
      snprintf(buf, sizeof(buf), "OUT%6sC %2ld%%", v1,
               (long)out.humidity);
      draw_line(lcd, 0, buf);
      fmt_1dp(v1, sizeof(v1), in.temperature, false);
      snprintf(buf, sizeof(buf), "IN %6sC %2ld%%", v1, (long)in.humidity);
      draw_line(lcd, 1, buf);
      break;

    case SCREEN_ABS:
      // Абсолютная влажность и точка росы притока — те величины, по которым
      // контроллер принимает решение.
      fmt_1dp(v1, sizeof(v1), out.absolute_humidity, false);
      fmt_1dp(v2, sizeof(v2), dew_point(out.temperature, out.humidity), false);
      snprintf(buf, sizeof(buf), "Out%5s Td%5s", v1, v2);
      draw_line(lcd, 0, buf);
      fmt_1dp(v1, sizeof(v1), in.absolute_humidity, false);
      fmt_1dp(v2, sizeof(v2), in.absolute_humidity - out.absolute_humidity,
              true);
      snprintf(buf, sizeof(buf), "In %5s d%5s", v1, v2);
      draw_line(lcd, 1, buf);
      break;

    case SCREEN_SURFACE: {
      // Критическая поверхность: её реальная температура вместо аппроксимации
      // "воздух минус запас" и есть то, на чём проверяется конденсат.
      if (!floor.valid) {
        draw_line(lcd, 0, "FLOOR SENSOR OFF");
        draw_line(lcd, 1, "fallback: air-3C");
        break;
      }
      fmt_1dp(v1, sizeof(v1), floor.temperature, false);
      snprintf(buf, sizeof(buf), "FLR %6sC %s", v1,
               fan.surface_wet ? "WET" : "DRY");
      draw_line(lcd, 0, buf);
      // Запас до росы на полу: положителен — сухо, отрицателен — идёт вода.
      fmt_1dp(v1, sizeof(v1), dew_point(in.temperature, in.humidity), false);
      fmt_1dp(v2, sizeof(v2), floor.temperature -
                                   dew_point(in.temperature, in.humidity),
              true);
      snprintf(buf, sizeof(buf), "Td%6s d%6s", v1, v2);
      draw_line(lcd, 1, buf);
      break;
    }

    case SCREEN_BARO: {
      // Влагосодержание (г/кг) сохраняется при нагреве и охлаждении, поэтому
      // оно честнее показывает, насколько приток действительно суше. Само
      // решение контроллер продолжает принимать по абсолютной влажности.
      fmt_1dp(v1, sizeof(v1), out.mixing_ratio, false);
      fmt_1dp(v2, sizeof(v2), in.mixing_ratio, false);
      snprintf(buf, sizeof(buf), "Out%5s In%5s", v1, v2);
      draw_line(lcd, 0, buf);

      char p[6];
      if (baro.valid) snprintf(p, sizeof(p), "%4ld", (long)(baro.pressure_hpa + 0.5f));
      else snprintf(p, sizeof(p), "%4s", "----");
      fmt_1dp(v2, sizeof(v2), in.mixing_ratio - out.mixing_ratio, true);
      snprintf(buf, sizeof(buf), "P%s hPa d%5s", p, v2);
      draw_line(lcd, 1, buf);
      break;
    }

    case SCREEN_STATUS:
    default: {
      uint32_t hours = fan.runtime_s / 3600UL;
      uint32_t minutes = (fan.runtime_s % 3600UL) / 60UL;
      snprintf(buf, sizeof(buf), "%-8s%3luh%02lu", block_text(fan),
               (unsigned long)hours, (unsigned long)minutes);
      draw_line(lcd, 0, buf);
      snprintf(buf, sizeof(buf), "Tmin%+3d Tmax%+3d", st.t_min, st.t_max);
      draw_line(lcd, 1, buf);
      break;
    }
  }
}

// Курсор переключается циклически по четырём позициям: t_min, t_max, привод,
// импульс привода.
void display_edit(LiquidCrystal& lcd, int16_t t_min, int16_t t_max,
                  bool valve_enabled, uint16_t valve_pulse_s, uint8_t cursor) {
  char buf[20];

  // Активная позиция помечается ">" в начале строки: на 16 символах
  // мигающее подчёркивание читается хуже.
  snprintf(buf, sizeof(buf), "%cTmin %+3d C", cursor == 0 ? '>' : ' ', t_min);
  draw_line(lcd, 0, buf);

  if (cursor == 1) {
    snprintf(buf, sizeof(buf), ">Tmax %+3d C", t_max);
  } else if (cursor == 2) {
    // Наличие привода заслонки — переключатель, а не число: шаг всегда один.
    snprintf(buf, sizeof(buf), ">VALVE %s", valve_enabled ? "ON " : "OFF");
  } else if (cursor == 3) {
    snprintf(buf, sizeof(buf), ">PULSE %2lu s", (unsigned long)valve_pulse_s);
  } else {
    snprintf(buf, sizeof(buf), " Tmax %+3d C", t_max);
  }
  draw_line(lcd, 1, buf);
}
