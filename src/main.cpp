#include <Arduino.h>
#include <LiquidCrystal.h>
#include "config.h"
#include "sensors.h"
#include "buttons.h"
#include "settings.h"
#include "fan_controller.h"
#include "display.h"
#include "valve.h"

// ---------------------------------------------------------------------------
// Интерфейс: обычный просмотр экранов и режим редактирования уставок.
//   Select — вход/выход из редактирования
//   Up     — увеличить выбранную уставку      Down — уменьшить
//   Left   — выбрать параметр (t_min / t_max) Right — следующий экран
//
//   Up (обычный режим): двойное нажатие — включить принудительную
//   вентиляцию, одинарное — выключить.
// ---------------------------------------------------------------------------

enum UIMode : uint8_t {
  UI_NORMAL,
  UI_EDIT,
};

static UIMode s_mode = UI_NORMAL;
static uint8_t s_cursor = 0;  // 0 = t_min, 1 = t_max, 2 = valve, 3 = valve_pulse
static unsigned long s_edit_since_ms = 0;

static void enter_edit() {
  s_mode = UI_EDIT;
  s_cursor = 0;
  s_edit_since_ms = millis();
  // Нажатия, накопленные до входа, не должны «выстрелить» после выхода.
  buttons_reset_clicks();
  display_invalidate();
}

static void exit_edit() {
  s_mode = UI_NORMAL;
  // Уставки сохраняем только при выходе: иначе EEPROM трётся на каждом
  // шаге редактирования.
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

  if (s_mode == UI_EDIT) {
    // Бездействие размыкает редактирование, чтобы контроллер не остался
    // «запертым» на экране уставок.
    if ((now - s_edit_since_ms) >= EDIT_TIMEOUT_MS) {
      exit_edit();
    }

    if (buttons_pressed(BTN_SELECT)) exit_edit();
    if (buttons_pressed(BTN_LEFT)) {
      s_cursor = (s_cursor + 1) % 4;
      display_invalidate();
    }
    if (buttons_pressed(BTN_UP)) {
      if (s_cursor == 0) settings_adjust_t_min(+1);
      else if (s_cursor == 1) settings_adjust_t_max(+1);
      else if (s_cursor == 2) settings_toggle_valve();
      else settings_adjust_valve_pulse(+1);
      display_invalidate();
    }
    if (buttons_pressed(BTN_DOWN)) {
      if (s_cursor == 0) settings_adjust_t_min(-1);
      else if (s_cursor == 1) settings_adjust_t_max(-1);
      else if (s_cursor == 2) settings_toggle_valve();
      else settings_adjust_valve_pulse(-1);
      display_invalidate();
    }
  } else {
    if (buttons_pressed(BTN_SELECT)) enter_edit();
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

  if (s_mode == UI_EDIT) {
    display_edit(lcd, st.t_min, st.t_max, st.valve_enabled,
                 st.valve_pulse_s, s_cursor);
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
    Serial.println(fan.runtime_s);
  }
}
