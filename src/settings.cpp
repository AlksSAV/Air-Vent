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

// Пульс привода — единственное беззнаковое поле, и переиспользовать
// знаковые чтение/запись ради него дешевле, чем держать вторую пару функций.
static uint16_t read_u16(uint16_t addr) {
  return (uint16_t)(((uint16_t)EEPROM.read(addr) << 8) | EEPROM.read(addr + 1));
}

static void write_u16(uint16_t addr, uint16_t value) {
  EEPROM.update(addr, (uint8_t)((uint16_t)value >> 8));
  EEPROM.update(addr + 1, (uint8_t)((uint16_t)value & 0xFF));
}

// CRC-8 (poly 0x07) over the runtime counter. write_runtime() rewrites four
// bytes one at a time, so losing power in the middle of a save leaves a value
// built from two generations of counter, and on the display that reads as up to
// four billion seconds. A checksum makes such a torn write detectable, and a
// detected damage is cheaper than a wrong number: the counter restarts from
// zero instead of reporting a runtime nobody ever accumulated.
//
// Computed bit by bit rather than from a table: a 256-byte lookup would be
// exactly the flash the debug build no longer has.
static uint8_t crc8(const uint8_t* data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

// Reads the counter together with its checksum. Returns false when the stored
// pair does not match, leaving the output untouched.
static bool read_runtime(uint32_t* out_value) {
  uint8_t bytes[4];
  for (uint8_t i = 0; i < 4; i++)
    bytes[i] = EEPROM.read(EEPROM_RUNTIME_ADDR + i);

  uint32_t value = 0;
  for (uint8_t i = 0; i < 4; i++) value = (value << 8) | bytes[i];

  // 0xFF is the state of a cell that has never been written, so this is where a
  // board upgraded from a firmware without a checksum arrives. Its counter is a
  // plain four-byte record written long ago, not a torn one, and it is adopted
  // as it stands: discarding years of accumulated runtime over a missing
  // checksum would be a worse defect than the one this guard exists to catch.
  uint8_t stored_crc = EEPROM.read(EEPROM_CRC_ADDR);
  if (stored_crc != 0xFF && stored_crc != crc8(bytes, 4)) return false;

  *out_value = value;
  return true;
}

// The checksum is written last: torn power in the middle of a save then breaks
// the pair instead of leaving a self-consistent old value, which is what
// read_runtime() is able to notice.
static void write_runtime(uint32_t value) {
  uint8_t bytes[4];
  for (uint8_t i = 0; i < 4; i++)
    bytes[i] = (uint8_t)((value >> (8 * (3 - i))) & 0xFF);

  for (uint8_t i = 0; i < 4; i++) EEPROM.update(EEPROM_RUNTIME_ADDR + i, bytes[i]);
  EEPROM.update(EEPROM_CRC_ADDR, crc8(bytes, 4));
}

void settings_init() {
  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
    // Первая прошивка или повреждённая область — заводим значения по умолчанию.
    EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    write_i16(EEPROM_TMIN_ADDR, T_OUT_DEFAULT_MIN);
    write_i16(EEPROM_TMAX_ADDR, T_OUT_DEFAULT_MAX);
    write_runtime(0);
    EEPROM.update(EEPROM_VALVE_ADDR, 0);
    write_u16(EEPROM_VPULSE_ADDR, VALVE_PULSE_DEFAULT_S);
    write_i16(EEPROM_TINMIN_ADDR, T_IN_MIN_DEFAULT);
  }

  s_settings.t_min = read_i16(EEPROM_TMIN_ADDR);
  s_settings.t_max = read_i16(EEPROM_TMAX_ADDR);

  // The runtime counter is the only value here that only ever grows, so a torn
  // write cannot be spotted by a range check alone: half of an old and half of
  // a new count still looks plausible. The checksum catches it, and a value
  // beyond RUNTIME_MAX_H catches what a checksum cannot - damage that happened
  // to leave a consistent pair. Either way the counter restarts: a wrong
  // maintenance figure is worse than no figure at all.
  uint32_t stored_runtime = 0;
  if (!read_runtime(&stored_runtime) ||
      stored_runtime > (uint32_t)RUNTIME_MAX_S) {
    stored_runtime = 0;
    write_runtime(0);
  }
  s_settings.fan_runtime_s = stored_runtime;

  // Привод заслонки — модуль необязательный: включается только явной настройкой,
  // поэтому любое значение, кроме единицы, трактуется как "привода нет".
  s_settings.valve_enabled = (EEPROM.read(EEPROM_VALVE_ADDR) == 1);

  // Длительность импульса привода — по умолчанию 12 сек.
  s_settings.valve_pulse_s = read_u16(EEPROM_VPULSE_ADDR);
  if (s_settings.valve_pulse_s < VALVE_PULSE_MIN_S ||
      s_settings.valve_pulse_s > VALVE_PULSE_MAX_S) {
    s_settings.valve_pulse_s = VALVE_PULSE_DEFAULT_S;
    write_u16(EEPROM_VPULSE_ADDR, s_settings.valve_pulse_s);
  }

  // The indoor limit appeared after the others, so boards flashed with an
  // older firmware hold garbage in these cells (usually 0xFF 0xFF). A value
  // outside the range means "the parameter has never been set" and is replaced
  // by the factory one. The magic number stays untouched on purpose: bumping
  // it would reset the accumulated fan runtime.
  s_settings.t_in_min = read_i16(EEPROM_TINMIN_ADDR);
  if (s_settings.t_in_min < T_IN_MIN_ALLOWED_MIN ||
      s_settings.t_in_min > T_IN_MIN_ALLOWED_MAX) {
    s_settings.t_in_min = T_IN_MIN_DEFAULT;
    write_i16(EEPROM_TINMIN_ADDR, s_settings.t_in_min);
  }

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

bool settings_toggle_valve() {
  s_settings.valve_enabled = !s_settings.valve_enabled;
  return true;
}

bool settings_adjust_valve_pulse(int8_t delta) {
  int16_t next = (int16_t)s_settings.valve_pulse_s + delta * VALVE_PULSE_STEP_S;
  next = constrain(next, (int16_t)VALVE_PULSE_MIN_S, (int16_t)VALVE_PULSE_MAX_S);
  if (next == (int16_t)s_settings.valve_pulse_s) return false;
  s_settings.valve_pulse_s = (uint16_t)next;
  return true;
}

bool settings_adjust_t_in_min(int8_t delta) {
  int16_t next = s_settings.t_in_min + delta * T_IN_MIN_STEP;
  next = constrain(next, (int16_t)T_IN_MIN_ALLOWED_MIN, (int16_t)T_IN_MIN_ALLOWED_MAX);
  if (next == s_settings.t_in_min) return false;
  s_settings.t_in_min = next;
  return true;
}

void settings_commit() {
  write_i16(EEPROM_TMIN_ADDR, s_settings.t_min);
  write_i16(EEPROM_TMAX_ADDR, s_settings.t_max);
  EEPROM.update(EEPROM_VALVE_ADDR, s_settings.valve_enabled ? 1 : 0);
  write_u16(EEPROM_VPULSE_ADDR, s_settings.valve_pulse_s);

  // The runtime goes out with the rest of the settings: leaving it to the
  // hourly save alone loses up to an hour of counted work every time the board
  // is powered off after a menu edit. EEPROM.update skips a byte whose value
  // already matches, so a commit that did not change the counter costs nothing.
  write_runtime(s_settings.fan_runtime_s);
  s_runtime_dirty_s = 0;
  s_last_runtime_save_ms = millis();
}

void settings_reset_runtime() {
  s_settings.fan_runtime_s = 0;
  // Счётчик несохранённых секунд обнуляется вместе с наработкой: иначе
  // ближайший часовой срез запишет в EEPROM уже сброшенное значение повторно,
  // а следующий цикл работы начнёт отсчёт от старой точки.
  s_runtime_dirty_s = 0;
  s_last_runtime_save_ms = millis();
  write_runtime(0);
}

void settings_add_runtime(uint32_t seconds) {
  if (seconds == 0) return;

  s_settings.fan_runtime_s += seconds;
  s_runtime_dirty_s += seconds;

  unsigned long now = millis();
  if (s_runtime_dirty_s >= 3600 || (now - s_last_runtime_save_ms) >= RUNTIME_SAVE_PERIOD_MS) {
    write_runtime(s_settings.fan_runtime_s);
    s_runtime_dirty_s = 0;
    s_last_runtime_save_ms = now;
  }
}
