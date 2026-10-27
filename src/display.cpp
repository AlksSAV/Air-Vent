#include "display.h"
#include "humidity.h"
#include <stdio.h>
#include <string.h>

static Screen s_screen = SCREEN_CLIMATE;
static unsigned long s_last_draw_ms = 0;
static bool s_force_redraw = true;

// Момент последней смены экрана: от него отсчитывается автолистание, поэтому
// экран, выбранный кнопкой, держится не меньше SCREEN_AUTO_ADVANCE_MS.
static unsigned long s_screen_since_ms = 0;

// Подсветка гаснет по бездействию клавиш. Таймер отдельный от перерисовки:
// экран продолжает обновляться, гаснет только подсветка.
static unsigned long s_last_activity_ms = 0;
static bool s_asleep = false;

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
// от предыдущего экрана остаются «хвосты». Копируем через memcpy: strncpy
// добивает остаток нулями, и lcd.print() печатает короче строки экрана.
static void draw_line(LiquidCrystal& lcd, uint8_t row, const char* text) {
  char line[17];
  memset(line, ' ', 16);
  line[16] = '\0';

  size_t len = strlen(text);
  if (len > 16) len = 16;
  memcpy(line, text, len);

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

// Строка по центру 16-символьного экрана: draw_line всегда дополняет строку
// пробелами справа, поэтому сдвиг делается префиксом.
static void draw_centered(LiquidCrystal& lcd, uint8_t row, const char* text) {
  char buf[20];
  size_t len = strlen(text);
  size_t pad = len < 16 ? (16 - len) / 2 : 0;
  snprintf(buf, sizeof(buf), "%*s%s", (int)pad, "", text);
  draw_line(lcd, row, buf);
}

// δ (разность) и φ (относительная влажность) в штатном CGROM HD44780
// отсутствуют, поэтому рисуются вручную в CGRAM. ρ (0xE6) и ° (0xDF) там есть
// — их слоты не занимаем.
// Слот 0 не используем: его код — 0x00, что обрывает C-строку в snprintf.
static byte CHAR_DELTA[8] = {0x04, 0x04, 0x0A, 0x0A, 0x11, 0x11, 0x1F, 0x00};
static byte CHAR_PHI[8] = {0x04, 0x0E, 0x15, 0x15, 0x15, 0x0E, 0x04, 0x00};
#define CG_DELTA 1
#define CG_PHI 2

void display_init(LiquidCrystal& lcd) {
  // Подсветка LCD Keypad Shield сидит на D10: без явного HIGH экран рабочий,
  // но тёмный.
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);

  lcd.begin(16, 2);

  // HD44780 после `begin` ещё не готов принимать команды: контроллер
  // дисплея выполняется внутреннюю инициализацию. Команда `clear()`,
  // отправленная сразу, теряется — экран остаётся пустым.
  delay(100);

  lcd.createChar(CG_DELTA, CHAR_DELTA);
  lcd.createChar(CG_PHI, CHAR_PHI);

  lcd.clear();
  // HD44780 выполняет `clear()` асинхронно — команда занимает 1.64 мс.
  // `draw_centered`, начатый сразу, стирает текст до того, как экран
  // отчистится, поэтому задержка обязательна.
  delay(5);

  // Заставка из двух кадров, по 2 с: название и версия. Пока она идёт,
  // контроллер уже опрашивает датчики — первый опрос приходится на эти же
  // секунды, поэтому после заставки экран сразу показывает данные.
  draw_centered(lcd, 0, "Air-Vent");
  draw_centered(lcd, 1, "drying control");
  delay(2000);

  lcd.clear();
  delay(5);
  draw_centered(lcd, 0, "McKey");
  draw_centered(lcd, 1, "v1.0 2026");
  delay(2000);

  lcd.clear();
  delay(5);
  // Отсчёт бездействия начинается после заставки: она и так занимает 4 с,
  // и таймаут должен отсчитываться от момента, когда экран стал рабочим.
  s_last_activity_ms = millis();
  s_force_redraw = true;
}

void display_next_screen() {
  s_screen = (Screen)((s_screen + 1) % SCREENS_COUNT);
  s_screen_since_ms = millis();
  s_force_redraw = true;
}

// Любое нажатие продлевает подсветку. Подсветка — обычный цифровой выход
// щита, поэтому включается простым digitalWrite.
void display_wake() {
  s_last_activity_ms = millis();
  if (!s_asleep) return;
  s_asleep = false;
  digitalWrite(10, HIGH);
  s_force_redraw = true;
}

bool display_is_asleep() { return s_asleep; }

// Гасит подсветку по таймауту бездействия. Экран при этом не очищается:
// показания остаются читаемыми при внешнем свете, а вернуть подсветку можно
// любой клавишей.
static void update_backlight(unsigned long now) {
  if (s_asleep) return;
  if ((now - s_last_activity_ms) < DISPLAY_BACKLIGHT_TIMEOUT_MS) return;
  s_asleep = true;
  digitalWrite(10, LOW);
}

void display_invalidate() { s_force_redraw = true; }

void display_show(LiquidCrystal& lcd, const SensorData& out,
                  const SensorData& in, const SurfaceData& floor,
                  const BaroData& baro, FanStatus fan, const Settings& st) {
  unsigned long now = millis();
  update_backlight(now);

  // Автолистание: экран меняется сам, если человек его не трогал. Отсчёт
  // идёт от последней смены, поэтому только что выбранный экран не сбежит
  // из-под пальца.
  if ((now - s_screen_since_ms) >= SCREEN_AUTO_ADVANCE_MS) {
    s_screen = (Screen)((s_screen + 1) % SCREENS_COUNT);
    s_screen_since_ms = now;
    s_force_redraw = true;
  }

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
      // "\xC2\xB0" не годится: HD44780 берёт знак град из собственного CGROM
      // по байту 0xDF (223).
      fmt_1dp(v1, sizeof(v1), out.temperature, false);
      fmt_1dp(v2, sizeof(v2), in.temperature, false);
      snprintf(buf, sizeof(buf), "OUT%5s\xDF" "C %c %2ld%%", v1, CG_PHI,
               (long)out.humidity);
      draw_line(lcd, 0, buf);
      fmt_1dp(v1, sizeof(v1), in.temperature, false);
      snprintf(buf, sizeof(buf), "IN %5s\xDF" "C %c %2ld%%", v1, CG_PHI,
               (long)in.humidity);
      draw_line(lcd, 1, buf);
      break;

    case SCREEN_ABS:
      // Абсолютная влажность — экран решения.
      // "\xE6" (230) — ρ из штатного CGROM HD44780: обозначение абсолютной
      // влажности.
      fmt_1dp(v1, sizeof(v1), out.absolute_humidity, false);
      snprintf(buf, sizeof(buf), "Out \xE6=%5s g/m3", v1);
      draw_line(lcd, 0, buf);
      fmt_1dp(v1, sizeof(v1), in.absolute_humidity, false);
      snprintf(buf, sizeof(buf), "In  \xE6=%5s g/m3", v1);
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
      snprintf(buf, sizeof(buf), "FLR %6s\xDF" "C %s", v1,
               fan.surface_wet ? "WET" : "DRY");
      draw_line(lcd, 0, buf);
      // Точка росы внутреннего воздуха. "Tdew" — 4 символа, поэтому поле
      // значения начинается в той же колонке, что и температура пола.
      fmt_1dp(v1, sizeof(v1), dew_point(in.temperature, in.humidity), false);
      snprintf(buf, sizeof(buf), "Tdew%6s\xDF" "C", v1);
      draw_line(lcd, 1, buf);
      break;
    }

    case SCREEN_BARO: {
      // Влагосодержание (г/кг) сохраняется при нагреве и охлаждении, поэтому
      // оно честнее показывает, насколько приток действительно суше. Само
      // решение контроллер продолжает принимать по абсолютной влажности.
      fmt_1dp(v1, sizeof(v1), out.mixing_ratio, false);
      snprintf(buf, sizeof(buf), "Out x=%5s g/kg", v1);
      draw_line(lcd, 0, buf);

      fmt_1dp(v1, sizeof(v1), in.mixing_ratio, false);
      snprintf(buf, sizeof(buf), "In  x=%5s g/kg", v1);
      draw_line(lcd, 1, buf);
      break;
    }

    case SCREEN_DELTA: {
      // Справочные разности: δA — разница абсолютной влажности (внутри минус
      // снаружи), г/м³; δT — запас поверхности до точки росы, °C.
      // CGRAM-слот 7: пользовательский значок «треугольник с чертой».
      char p[6];
      if (baro.valid) snprintf(p, sizeof(p), "%4ld", (long)(baro.pressure_hpa + 0.5f));
      else snprintf(p, sizeof(p), "%4s", "----");
      snprintf(buf, sizeof(buf), "P=%5s hPa", p);
      draw_line(lcd, 0, buf);

      fmt_1dp(v1, sizeof(v1), in.absolute_humidity - out.absolute_humidity, true);
      // Без датчика пола запас до росы не измерить: рисуем прочерк вместо числа.
      if (floor.valid) {
        fmt_1dp(v2, sizeof(v2), floor.temperature -
                                     dew_point(in.temperature, in.humidity), true);
      } else {
        snprintf(v2, sizeof(v2), "----");
      }
      snprintf(buf, sizeof(buf), "%cA%5s %cT%5s", CG_DELTA, v1, CG_DELTA, v2);
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
      // Точка росы притока: именно к ней сравнивается температура пола, когда
      // решается, пускать ли вентилятор (OFF:DEW).
      fmt_1dp(v1, sizeof(v1), dew_point(out.temperature, out.humidity), false);
      snprintf(buf, sizeof(buf), "Td out%6s\xDF" "C", v1);
      draw_line(lcd, 1, buf);
      break;
    }
  }
}

// Курсор переключается циклически по пяти позициям: t_min, t_max, привод,
// импульс привода и сброс наработки. Первые две страницы несут по две позиции
// (пределы температур и параметры привода), третья — одна: сброс наработки,
// единственное в меню действие вместо уставки, поэтому ему нужна отдельная
// страница и явное подтверждение.
void display_edit(LiquidCrystal& lcd, int16_t t_min, int16_t t_max,
                  bool valve_enabled, uint16_t valve_pulse_s,
                  bool reset_runtime, uint8_t cursor) {
  char buf[20];

  // В меню подсветка не гаснет: пользователь как раз смотрит на экран и
  // щёлкает клавишами, а таймаут бездействия в меню свой (EDIT_TIMEOUT_MS).
  s_last_activity_ms = millis();
  update_backlight(s_last_activity_ms);

  // Активная позиция помечается ">" в начале строки: на 16 символах
  // мигающее подчёркивание читается хуже. Индикатор страницы стоит в правом
  // нижнем углу, чтобы не сдвигать редактируемое число.
  if (cursor < 2) {
    snprintf(buf, sizeof(buf), "%cTmin %+3d C", cursor == 0 ? '>' : ' ', t_min);
    draw_line(lcd, 0, buf);
    snprintf(buf, sizeof(buf), "%cTmax %+3d C  1/3", cursor == 1 ? '>' : ' ',
             t_max);
    draw_line(lcd, 1, buf);
    return;
  }

  if (cursor < 4) {
    // Наличие привода заслонки — переключатель, а не число: шаг всегда один.
    snprintf(buf, sizeof(buf), "%cVALVE %s", cursor == 2 ? '>' : ' ',
             valve_enabled ? "ON" : "OFF");
    draw_line(lcd, 0, buf);
    snprintf(buf, sizeof(buf), "%cPULSE %2lu s  2/3", cursor == 3 ? '>' : ' ',
             (unsigned long)valve_pulse_s);
    draw_line(lcd, 1, buf);
    return;
  }

  // Сброс наработки. Up выставляет yes, Down — no; применяется только по
  // Select, чтобы случайный уход из меню по таймауту не обнулил счётчик.
  snprintf(buf, sizeof(buf), "%cRESET RT   %s", cursor == 4 ? '>' : ' ',
           reset_runtime ? "yes" : "no");
  draw_line(lcd, 0, buf);
  draw_line(lcd, 1, " Select=OK   3/3");
}
