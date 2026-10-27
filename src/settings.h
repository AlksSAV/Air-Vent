#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// Настраиваемые параметры и накопленная статистика, живущие в EEPROM.
struct Settings {
  int16_t t_min;  // нижний предел наружного воздуха, град
  int16_t t_max;  // верхний предел наружного воздуха, град
  bool valve_enabled;  // есть ли в сборке привод заслонки
  uint16_t valve_pulse_s;  // длительность импульса привода, сек
  uint32_t fan_runtime_s;  // накопленные секунды работы вентилятора
};

// Загружает настройки, при первой прошивке пишет значения по умолчанию.
void settings_init();

Settings settings_get();

// Меняет предел с учётом допустимых границ и порядка t_min <= t_max.
// Возвращает true, если значение изменилось.
bool settings_adjust_t_min(int8_t delta);
bool settings_adjust_t_max(int8_t delta);

// Переключает наличие привода заслонки.
bool settings_toggle_valve();

// Меняет длительность импульса привода (сек). Возвращает true, если изменилось.
bool settings_adjust_valve_pulse(int8_t delta);

// Сохраняет пределы в EEPROM (вызывать при выходе из меню).
void settings_commit();

// Прибавляет секунды работы вентилятора и сохраняет раз в час.
void settings_add_runtime(uint32_t seconds);

// Обнуляет наработку и пишет её в EEPROM немедленно: откладывать запись до
// очередного часового среза нельзя, иначе выключение питания вернёт старое
// значение.
void settings_reset_runtime();

#endif  // SETTINGS_H
