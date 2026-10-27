#include <Arduino.h>
#include <LiquidCrystal.h>
#include "config.h"
#include "sensors.h"
#include "buttons.h"
#include "settings.h"
#include "fan_controller.h"
#include "display.h"
#include "menu.h"
#include "valve.h"

// ---------------------------------------------------------------------------
// Интерфейс: обычный просмотр экранов и меню настроек.
//   Left / Right   — следующий экран
//   Select удержать— войти в меню настроек
//
// Внутри меню:
//   Up / Down      — перемещение по пунктам
//   Left / Right   — изменение значения
//   Select коротко — войти в раздел / выйти на уровень выше
//   Select удержать— назад, на пункте RESET RT — сброс наработки
//
//   Up (обычный режим): двойное нажатие — включить принудительную
//   вентиляцию, одинарное — выключить.
// ---------------------------------------------------------------------------

enum UIMode : uint8_t {
  UI_NORMAL,
  UI_MENU,
};

static UIMode s_mode = UI_NORMAL;

static void enter_menu() {
  s_mode = UI_MENU;
  menu_open();
  display_invalidate();
}

static void exit_menu() {
  s_mode = UI_NORMAL;
  settings_commit();
  buttons_reset_clicks();
  display_invalidate();
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

static LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5,
                         PIN_LCD_D6, PIN_LCD_D7);

void setup() {
  Serial.begin(115200);

  sensors_init();
  settings_init();
  fan_init();
  buttons_init();
  display_init(lcd);
}

void loop() {
  unsigned long now = millis();

  buttons_poll();

  // Любое нажатие зажигает подсветку и продлевает её таймаут: погасший экран
  // не должен требовать «особого» нажатия, первое же и есть пробуждение.
  if (buttons_current() != BTN_NONE) display_wake();

  if (s_mode == UI_MENU) {
    if (!menu_update()) exit_menu();
  } else {
    // Вход в настройки — только удержанием: короткое нажатие на обычных
    // экранах ничего не делает, чтобы случайно не открыть меню.
    if (buttons_held(BTN_SELECT, BUTTON_HOLD_MS)) enter_menu();
    if (buttons_pressed(BTN_RIGHT)) display_next_screen();
    if (buttons_pressed(BTN_LEFT)) display_next_screen();

    // Принудительная вентиляция: двойное UP — включить, одинарное UP — выключить.
    if (buttons_double_clicked(BTN_UP)) fan_force_start();
    if (buttons_single_clicked(BTN_UP)) fan_force_stop();
  }

  sensors_update();
  SensorData out = sensors_get_out();
  SensorData in = sensors_get_in();
  SurfaceData floor = sensors_get_floor();
  BaroData baro = sensors_get_baro();

  // Импульс привода снимается до обращения контроллера к положению заслонки:
  // иначе он на один цикл увидит «в пути» там, где ход уже закончился.
  valve_update();
  fan_update(out, in, floor);

  Settings st = settings_get();
  FanStatus fan = fan_status();

  if (s_mode == UI_MENU) {
    menu_draw(lcd);
  } else {
    display_show(lcd, out, in, floor, baro, fan, st);
  }

  // Телеметрия для отладки: одно короткое сообщение раз в 5 с.
  static unsigned long s_telemetry_ms = 0;
  if ((now - s_telemetry_ms) >= SERIAL_LOG_PERIOD_MS) {
    s_telemetry_ms = now;
    Serial.print(F("OUT "));
    Serial.print(out.temperature, 1);
    Serial.print(F("C "));
    Serial.print(out.humidity, 0);
    Serial.print(F("% abs="));
    Serial.print(out.absolute_humidity, 2);
    Serial.print(F(" | IN "));
    Serial.print(in.temperature, 1);
    Serial.print(F("C "));
    Serial.print(in.humidity, 0);
    Serial.print(F("% abs="));
    Serial.print(in.absolute_humidity, 2);
    Serial.print(F(" | FLOOR "));
    if (floor.valid) {
      Serial.print(floor.temperature, 1);
      Serial.print(F("C wet="));
      Serial.print(fan.surface_wet ? F("1") : F("0"));
    } else {
      Serial.print(F("--"));
    }
    Serial.print(F(" | BARO "));
    if (baro.valid) {
      Serial.print(baro.pressure_hpa, 1);
    } else {
      Serial.print(F("--"));
    }
    Serial.print(F(" | x out="));
    Serial.print(out.mixing_ratio, 2);
    Serial.print(F(" in="));
    Serial.print(in.mixing_ratio, 2);
    Serial.print(F(" | FAN "));
    Serial.print(fan.mode == FAN_RUNNING ? F("ON") :
                 fan.mode == FAN_FORCE ? F("FORCE") :
                 fan.mode == FAN_COOLDOWN ? F("PAUSE") : F("OFF"));
    Serial.print(F(" block="));
    Serial.print((int)fan.block);
    Serial.print(F(" rt="));
    Serial.print(fan.runtime_s);
    // Логические уровни выходов: F — вентилятор (1=вкл), V — состояние
    // привода (0=moving, 1=closed, 2=open), чтобы не путать с физическими
    // уровнями пина: SSR вентилятора HIGH, реле привода — LOW (GND-trigger).
    Serial.print(F(" F="));
    Serial.print(fan_is_on());
    Serial.print(F(" V="));
    Serial.println((int)valve_position());
  }
}
