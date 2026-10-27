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

void valve_init() {
  pinMode(PIN_VALVE_OPEN, OUTPUT);
  pinMode(PIN_VALVE_CLOSE, OUTPUT);
  drive(false, false);
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
  return s_moving_open ? VALVE_POS_OPEN : VALVE_POS_CLOSED;
}

void valve_home() {
  if (!valve_present()) return;

  // Пружинного возврата у привода нет, поэтому после включения питания
  // положение неизвестно. Принудительно закрываем: концевик сам разомкнёт
  // цепь, когда захлопка дойдёт до места.
  valve_close();
}

void valve_open() {
  if (!valve_present()) return;
  // Повторный вызов не должен перезапускать отсчёт хода: контроллер обращается
  // к приводу каждый проход loop(), а импульс обязан отработать целиком.
  if (s_moving && s_moving_open) return;

  drive(true, false);
  s_moving = true;
  s_moving_open = true;
  s_move_started_ms = millis();
}

void valve_close() {
  if (!valve_present()) return;
  if (s_moving && !s_moving_open) return;

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
  drive(false, false);
}
