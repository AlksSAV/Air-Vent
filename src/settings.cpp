#include "settings.h"
#include "config.h"
#include <EEPROM.h>

static Settings s_settings;
static unsigned long s_last_runtime_save_ms = 0;
static uint32_t s_runtime_dirty_s = 0;

static int16_t read_i16(uint16_t addr) {
  return (int16_t)(((uint16_t)EEPROM.read(addr) << 8) | EEPROM.read(addr + 1));
}

// Пишем только изменившиеся байты: EEPROM.update внутри сверяет значение,
// но явная проверка экономит время цикла записи.
static void write_i16(uint16_t addr, int16_t value) {
  EEPROM.update(addr, (uint8_t)((uint16_t)value >> 8));
  EEPROM.update(addr + 1, (uint8_t)((uint16_t)value & 0xFF));
}

static uint32_t read_u32(uint16_t addr) {
  uint32_t value = 0;
  for (uint8_t i = 0; i < 4; i++) {
    value = (value << 8) | EEPROM.read(addr + i);
  }
  return value;
}

static void write_u32(uint16_t addr, uint32_t value) {
  for (uint8_t i = 0; i < 4; i++) {
    EEPROM.update(addr + i, (uint8_t)((value >> (8 * (3 - i))) & 0xFF));
  }
}

void settings_init() {
  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
    // Первая прошивка или повреждённая область — заводим значения по умолчанию.
    EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    write_i16(EEPROM_TMIN_ADDR, T_OUT_DEFAULT_MIN);
    write_i16(EEPROM_TMAX_ADDR, T_OUT_DEFAULT_MAX);
    write_u32(EEPROM_RUNTIME_ADDR, 0);
  }

  s_settings.t_min = read_i16(EEPROM_TMIN_ADDR);
  s_settings.t_max = read_i16(EEPROM_TMAX_ADDR);
  s_settings.fan_runtime_s = read_u32(EEPROM_RUNTIME_ADDR);

  // Повреждённые при выключании питания пределы не должны ломать логику.
  if (s_settings.t_min < T_LIMIT_MIN_ALLOWED || s_settings.t_min > T_LIMIT_MAX_ALLOWED ||
      s_settings.t_max < T_LIMIT_MIN_ALLOWED || s_settings.t_max > T_LIMIT_MAX_ALLOWED ||
      s_settings.t_min > s_settings.t_max) {
    s_settings.t_min = T_OUT_DEFAULT_MIN;
    s_settings.t_max = T_OUT_DEFAULT_MAX;
    write_i16(EEPROM_TMIN_ADDR, s_settings.t_min);
    write_i16(EEPROM_TMAX_ADDR, s_settings.t_max);
  }

  s_last_runtime_save_ms = millis();
}

Settings settings_get() { return s_settings; }

bool settings_adjust_t_min(int8_t delta) {
  int16_t next = s_settings.t_min + delta * T_LIMIT_STEP;
  next = constrain(next, (int16_t)T_LIMIT_MIN_ALLOWED, s_settings.t_max);
  if (next == s_settings.t_min) return false;
  s_settings.t_min = next;
  return true;
}

bool settings_adjust_t_max(int8_t delta) {
  int16_t next = s_settings.t_max + delta * T_LIMIT_STEP;
  next = constrain(next, s_settings.t_min, (int16_t)T_LIMIT_MAX_ALLOWED);
  if (next == s_settings.t_max) return false;
  s_settings.t_max = next;
  return true;
}

void settings_commit() {
  write_i16(EEPROM_TMIN_ADDR, s_settings.t_min);
  write_i16(EEPROM_TMAX_ADDR, s_settings.t_max);
}

void settings_add_runtime(uint32_t seconds) {
  if (seconds == 0) return;

  s_settings.fan_runtime_s += seconds;
  s_runtime_dirty_s += seconds;

  unsigned long now = millis();
  if (s_runtime_dirty_s >= 3600 || (now - s_last_runtime_save_ms) >= RUNTIME_SAVE_PERIOD_MS) {
    write_u32(EEPROM_RUNTIME_ADDR, s_settings.fan_runtime_s);
    s_runtime_dirty_s = 0;
    s_last_runtime_save_ms = now;
  }
}
