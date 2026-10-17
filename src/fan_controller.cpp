#include "fan_controller.h"
#include "settings.h"
#include "config.h"
#include "humidity.h"

static FanStatus s_status;

// Начало текущего цикла работы вентилятора — отсчёт лимита непрерывной
// работы.
static unsigned long s_run_start_ms = 0;

// Начало фазы паузы (COOLDOWN) — отсчёт обязательного простоя.
static unsigned long s_phase_start_ms = 0;

// Момент, до которого наработка уже передана в settings. Хранится отдельно,
// потому что в EEPROM пишем целые секунды, а проверка лимита идёт по миллисекундам.
static unsigned long s_credited_ms = 0;

// Переключение реле: единственный place, где меняется состояние выхода.
static void relay(bool on) { digitalWrite(PIN_FAN, on ? HIGH : LOW); }

// Переносит накопленные полные секунды работы в settings. Вызывается и во
// время работы (для показаний дисплея), и перед остановом.
static void credit_runtime(unsigned long now) {
  unsigned long elapsed_ms = now - s_credited_ms;
  if (elapsed_ms < 1000UL) return;

  uint32_t whole_seconds = elapsed_ms / 1000UL;
  settings_add_runtime(whole_seconds);
  s_credited_ms += whole_seconds * 1000UL;
  s_status.runtime_s = settings_get().fan_runtime_s;
}

void fan_init() {
  pinMode(PIN_FAN, OUTPUT);
  // Реле включается активным уровнем, поэтому на старте держим LOW:
  // вентилятор не дёрнется при прошивке и перезагрузке.
  relay(false);

  s_status.mode = FAN_IDLE;
  s_status.block = BLOCK_SENSOR;  // достоверных данных ещё нет
  s_status.runtime_s = settings_get().fan_runtime_s;
  s_run_start_ms = 0;
  s_credited_ms = millis();
}

// Остановка с указанием причины. Наработка фиксируется до смены режима.
static void stop(FanBlock block) {
  unsigned long now = millis();
  if (s_status.mode == FAN_RUNNING) {
    credit_runtime(now);
  }
  s_status.mode = (block == BLOCK_COOLDOWN) ? FAN_COOLDOWN : FAN_IDLE;
  s_status.block = block;
  s_phase_start_ms = now;
}

static void start(unsigned long now) {
  s_status.mode = FAN_RUNNING;
  s_status.block = BLOCK_NONE;
  s_run_start_ms = now;
  s_credited_ms = now;
  s_phase_start_ms = now;
}

void fan_update(const SensorData& out, const SensorData& in) {
  unsigned long now = millis();

  // 1. Свежесть и достоверность данных — безусловный приоритет: нельзя
  //    принимать решение об осушении по несуществующим показаниям.
  bool fresh = out.valid && in.valid &&
               (now - out.last_read_ms) < DATA_STALE_MS &&
               (now - in.last_read_ms) < DATA_STALE_MS;
  if (!fresh) {
    stop(BLOCK_SENSOR);
    relay(false);
    return;
  }

  // 2. Отработка обязательной паузы после лимита непрерывной работы.
  if (s_status.mode == FAN_COOLDOWN) {
    if ((now - s_phase_start_ms) < FAN_PAUSE_MS) {
      relay(false);
      return;
    }
    s_status.mode = FAN_IDLE;
    s_status.block = BLOCK_NONE;
  }

  // Пока вентилятор включён, поддерживаем актуальные часы наработки.
  if (s_status.mode == FAN_RUNNING) {
    credit_runtime(now);
  }

  Settings st = settings_get();

  // 3. Температурный коридор наружного воздуха.
  if (out.temperature < (float)st.t_min) {
    stop(BLOCK_TEMP_LOW);
    relay(false);
    return;
  }
  if (out.temperature > (float)st.t_max) {
    stop(BLOCK_TEMP_HIGH);
    relay(false);
    return;
  }

  // 4. Защита от конденсата. Приточный воздух безопасен, пока его точка росы
  //    не поднялась к температуре конструкций. Конструкции принимаем равными
  //    внутреннему воздуху — это худший случай для стен и пола подвала.
  float dew_out = dew_point(out.temperature, out.humidity);
  if (dew_out > in.temperature - DEW_MARGIN_C) {
    stop(BLOCK_DEW);
    relay(false);
    return;
  }

  // 5. Гистерезис по абсолютной влажности.
  float diff = in.absolute_humidity - out.absolute_humidity;

  if (s_status.mode == FAN_RUNNING) {
    // Выключаем, только когда преимущество наружного воздуха почти иссякло:
    // нижний порог ниже верхнего и не даёт реле циклировать на месте.
    if (diff < ABS_HYST_OFF) {
      stop(BLOCK_DRY);
      relay(false);
      return;
    }
    // Лимит непрерывной работы: защита от переохлаждения подвала.
    if ((now - s_run_start_ms) >= FAN_RUN_MAX_MS) {
      stop(BLOCK_COOLDOWN);
      relay(false);
      return;
    }
    relay(true);
    return;
  }

  // IDLE: включаем при достаточной разнице. Значения между порогами
  // оставляют контроллер в покое — это и есть зона нечувствительности.
  if (diff > ABS_HYST_ON) {
    start(now);
    relay(true);
    return;
  }

  s_status.block = BLOCK_DRY;
  relay(false);
}

FanStatus fan_status() { return s_status; }

bool fan_is_on() { return s_status.mode == FAN_RUNNING; }
