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
  // Power pins first. Until now D3/D11/D12 are high-impedance inputs: the
  // GND-trigger relay modules on D11/D12 are usually saved by the optocoupler
  // pull-up, but the HIGH-active SSR on D3 must not rely on that — a pick-up
  // noise on a floating input can open it.
  pinMode(PIN_FAN, OUTPUT);
  digitalWrite(PIN_FAN, !FAN_RELAY_ACTIVE_LEVEL);

  pinMode(PIN_VALVE_OPEN, OUTPUT);
  digitalWrite(PIN_VALVE_OPEN, !VALVE_RELAY_ACTIVE_LEVEL);

  pinMode(PIN_VALVE_CLOSE, OUTPUT);
  digitalWrite(PIN_VALVE_CLOSE, !VALVE_RELAY_ACTIVE_LEVEL);

  // Дисплей инициализируется первым: пока поднимаются датчики, LCD уже
  // показывает заставку. Иначе экран остаётся тёмным и с мусором всё время,
  // что занимает sensors_init().
  display_init(lcd);

#ifdef TELEMETRY
  Serial.begin(115200);
#endif

  sensors_init();
  settings_init();
  fan_init();
  buttons_init();

  // Заставка держится не фиксированное время, а до первого годного опроса:
  // DHT22 отвечает только через SENSOR_FIRST_READ_MS после подачи питания, и
  // без ожидания первым, что видит человек, оказывается SENSOR ERROR.
  unsigned long boot_ms = millis();
  while ((millis() - boot_ms) < BOOT_SPLASH_MS) {
    sensors_update();
    SensorData out = sensors_get_out();
    SensorData in = sensors_get_in();
    if (out.valid && in.valid) break;
    delay(50);
  }

  display_boot_done();

  // Watchdog: enabled at the end because setup() blocks intentionally
  // (LCD init, DHT warm-up, splash up to 5 s). A 2 s limit would be violated
  // before loop() even starts. A hang here (stuck I2C, library bug) would
  // leave the fan relay permanently on.
  //
  // Note: the optimized Optiboot bootloader jumps straight to the app on
  // watchdog reset without delay, so a reset-loop is impossible; the window
  // where power pins are floating is limited to ~130 ms during init.
  wdt_enable(WATCHDOG_TIMEOUT_MS);
}

void loop() {
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

#ifdef TELEMETRY
  // Телеметрия для отладки: одно короткое сообщение раз в 5 с.
  // Telemetry is cut out entirely from the release build.
  //
  // Every number is sent as whole tenths (24.5 deg becomes "245"):
  // Print::print(float) links printFloat(), about 1.5 KB of flash, and the
  // release env sits at 93.9 % while the debug env at 99.0 %. After the indoor
  // limit was added there is no room left for a float formatter. Tenths are
  // precise enough for debugging and they shorten the line as a side effect.
  static unsigned long s_telemetry_ms = 0;
  if ((millis() - s_telemetry_ms) >= SERIAL_LOG_PERIOD_MS) {
    s_telemetry_ms = millis();
    Serial.print(F("O"));
    Serial.print((long)(out.temperature * 10.0f));
    Serial.print(F("/"));
    Serial.print((long)(out.humidity + 0.5f));
    Serial.print(F("/"));
    Serial.print((long)(out.absolute_humidity * 10.0f));
    Serial.print(F(" I"));
    Serial.print((long)(in.temperature * 10.0f));
    Serial.print(F("/"));
    Serial.print((long)(in.humidity + 0.5f));
    Serial.print(F("/"));
    Serial.print((long)(in.absolute_humidity * 10.0f));
    Serial.print(F(" F"));
    Serial.print(floor.valid ? (long)(floor.temperature * 10.0f) : 0);
    Serial.print(F(" M"));
    Serial.print((int)fan.mode);
    Serial.print(F(" B"));
    Serial.print((int)fan.block);
    Serial.print(F(" R"));
    Serial.print(fan_is_on());
    Serial.print(F(" V"));
    Serial.println((int)valve_position());
  }
#endif

  // Last statement of loop(): if any call above ever hangs, the watchdog
  // resets the board instead of leaving the 230 V load in its last state.
  wdt_reset();
}
