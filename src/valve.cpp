#include "valve.h"
#include "config.h"
#include "settings.h"

// Реле привода — GND-trigger (VALVE_RELAY_ACTIVE_LEVEL=LOW), реле вентилятора —
// HIGH; выводим логическое «включено» в физический уровень в одном месте.
static void drive(bool open_on, bool close_on) {
  // Взаимная блокировка: одновременно обе фазы поданы быть не могут, иначе
  // замкнём обмотку мотора между двумя концевиками.
  if (open_on && close_on) return;

  digitalWrite(PIN_VALVE_OPEN,
               open_on ? VALVE_RELAY_ACTIVE_LEVEL : !VALVE_RELAY_ACTIVE_LEVEL);
  digitalWrite(PIN_VALVE_CLOSE,
               close_on ? VALVE_RELAY_ACTIVE_LEVEL : !VALVE_RELAY_ACTIVE_LEVEL);
}

// Идёт ли сейчас импульс, в какую сторону и когда он начался. Концевик внутри
// привода сам разомкнёт цепь в крайнем положении, но реле мы держим
// фиксированное время и затем снимаем управление, чтобы мотор не гудел в упоре.
static bool s_moving = false;
static bool s_moving_open = false;
static unsigned long s_move_started_ms = 0;

// Known position flag. Without it valve_open() re-pulses an already open
// flap on every cycle: after COOLDOWN the controller starts again, the flap
// is still open, and the motor stalls against the end stop for a full pulse
// (32 times a day, 6.4 minutes of a blocked rotor). Cleared on power-up and
// whenever the valve module is switched in the menu, set when a pulse
// completes. valve_home() ignores it on purpose.
static bool s_pos_known = false;

void valve_init() {
  pinMode(PIN_VALVE_OPEN, OUTPUT);
  pinMode(PIN_VALVE_CLOSE, OUTPUT);
  drive(false, false);
  s_moving = false;
  s_pos_known = false;
}

// Наличие привода читается из настройки при каждом обращении, а не
// запоминается при инициализации: модуль включается и выключается в меню
// настроек, и прошивка обязана отреагировать сразу, без перезагрузки.
bool valve_present() { return settings_get().valve_enabled; }

ValvePos valve_position() {
  // Без привода канал открыт постоянно: вся логика тогда совпадает с прошивкой
  // до появления заслонки.
  if (!valve_present()) return VALVE_POS_OPEN;
  if (s_moving) return VALVE_POS_MOVING;
  // Пока положение не подтверждено отработавшим импульсом, считаем заслонку
  // закрытой: контроллер обязан выдать полный ход открытия, прежде чем
  // включать вентилятор, а не поверить последнему сохранённому значению.
  if (!s_pos_known) return VALVE_POS_CLOSED;
  return s_moving_open ? VALVE_POS_OPEN : VALVE_POS_CLOSED;
}

void valve_invalidate_position() { s_pos_known = false; }

void valve_home() {
  if (!valve_present()) return;

  // Пружинного возврата у привода нет, поэтому после включения питания
  // положение неизвестно. Принудительно закрываем: концевик сам разомкнёт
  // цепь, когда захлопка дойдёт до места.
  s_pos_known = false;
  valve_close();
}

void valve_open() {
  if (!valve_present()) return;
  // Повторный вызов не должен перезапускать отсчёт хода: контроллер обращается
  // к приводу каждый проход loop(), а импульс обязан отработать целиком.
  if (s_moving && s_moving_open) return;
  // The flap is already fully open: a fresh pulse would only stall the motor
  // against its end switch for the whole duration.
  if (!s_moving && s_pos_known && s_moving_open) return;

  drive(true, false);
  s_moving = true;
  s_moving_open = true;
  s_move_started_ms = millis();
}

void valve_close() {
  if (!valve_present()) return;
  if (s_moving && !s_moving_open) return;
  if (!s_moving && s_pos_known && !s_moving_open) return;

  drive(false, true);
  s_moving = true;
  s_moving_open = false;
  s_move_started_ms = millis();
}

void valve_update() {
  if (!s_moving) return;

  // Длительность импульса читается из настройки при каждом проходе: уставка
  // меняется в меню без перезагрузки.
  unsigned long pulse_ms =
      (unsigned long)settings_get().valve_pulse_s * 1000UL;
  if ((millis() - s_move_started_ms) < pulse_ms) return;

  s_moving = false;
  s_pos_known = true;
  drive(false, false);
}
