#include "fan_controller.h"
#include "settings.h"
#include "config.h"
#include "humidity.h"
#include "valve.h"

static FanStatus s_status;

// Начало текущего цикла работы вентилятора — отсчёт лимита непрерывной
// работы.
static unsigned long s_run_start_ms = 0;

// Начало фазы паузы (COOLDOWN) — отсчёт обязательного простоя.
static unsigned long s_phase_start_ms = 0;

// Момент, с которого пол подряд остаётся мокрым при работающем вентиляторе.
// Ноль — конденсата нет или вентилятор стоит.
static unsigned long s_wet_since_ms = 0;

// Момент, до которого наработка уже передана в settings. Хранится отдельно,
// потому что в EEPROM пишем целые секунды, а проверка лимита идёт по миллисекундам.
static unsigned long s_credited_ms = 0;

// Принудительный режим: флаг переключает только кнопка, таймера на него нет.
static bool s_force = false;

// Ring buffer of the humidity difference used for the decision. The difference
// is taken in mixing ratio (g/kg), the mass of vapor per mass of dry air: it is
// the quantity that survives heating the inlet air from -10 degC to +20 degC,
// while g/m3 changes with temperature, so a fixed mass-per-volume threshold
// means a different required advantage in every season. One raw DHT22 sample
// carries up to +-1.5 g/kg of noise on that difference, so the relay could flip
// on a lone bad reading. The median of the last DIFF_FILTER_SAMPLES values
// ignores it.
static float s_diff_samples[DIFF_FILTER_SAMPLES];
static uint8_t s_diff_count = 0;
static uint8_t s_diff_next = 0;

// last_good_ms stamps of the reading already pushed, so one poll contributes
// one sample even though fan_update() runs on every loop().
static unsigned long s_push_out_ms = 0;
static unsigned long s_push_in_ms = 0;

static void push_diff(float value, unsigned long out_stamp, unsigned long in_stamp) {
  if (out_stamp == s_push_out_ms && in_stamp == s_push_in_ms) return;
  s_push_out_ms = out_stamp;
  s_push_in_ms = in_stamp;

  s_diff_samples[s_diff_next] = value;
  s_diff_next = (uint8_t)((s_diff_next + 1) % DIFF_FILTER_SAMPLES);
  if (s_diff_count < DIFF_FILTER_SAMPLES) s_diff_count++;
}

static float median_diff() {
  if (s_diff_count == 0) return 0.0f;

  float sorted[DIFF_FILTER_SAMPLES];
  uint8_t n = s_diff_count;
  for (uint8_t i = 0; i < n; i++) sorted[i] = s_diff_samples[i];

  for (uint8_t i = 1; i < n; i++) {
    float key = sorted[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }
  return sorted[n / 2];
}

// Переключение реле: единственный place, где меняется состояние выхода.
// Моменты смены состояния запоминаются: минимальное время работы и простоя
// отсчитывается именно от них, а не от попыток вызова, которые повторяются
// каждый проход loop().
static bool s_relay_state = false;
static unsigned long s_engaged_ms = 0;
static unsigned long s_released_ms = 0;

static void relay(bool on) {
  if (on != s_relay_state) {
    if (on) {
      s_engaged_ms = millis();
    } else {
      s_released_ms = millis();
    }
    s_relay_state = on;
  }
  digitalWrite(PIN_FAN, on ? FAN_RELAY_ACTIVE_LEVEL : !FAN_RELAY_ACTIVE_LEVEL);
}

// В принудительном режиме реле включено так же, как при штатной работе,
// поэтому наработка и её фиксация перед остановом для них общие.
static bool relay_engaged(FanMode mode) {
  return mode == FAN_RUNNING || mode == FAN_FORCE;
}

// The shortest rest before a new start and the shortest run before a stop.
// Every engagement of the relay costs the drive two full strokes, so a value
// wandering around a threshold must not be able to cycle it: the limits below
// cover the hysteresis band, the sensor noise and the temperature corridor
// chatter at once.
static bool relay_allowed_on(unsigned long now) {
  if (s_relay_state) return true;
  return (now - s_released_ms) >= FAN_MIN_OFF_MS;
}

static bool relay_allowed_off(unsigned long now) {
  if (!s_relay_state) return true;
  return (now - s_engaged_ms) >= FAN_MIN_RUN_MS;
}

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

  valve_init();

  s_status.mode = FAN_IDLE;
  s_status.block = BLOCK_SENSOR;  // достоверных данных ещё нет
  s_status.runtime_s = settings_get().fan_runtime_s;
  s_status.surface_wet = false;
  s_status.inlet_cold = false;
  s_wet_since_ms = 0;
  s_run_start_ms = 0;
  s_credited_ms = millis();
  // The minimum rest is meant to stop cycling, not to delay commissioning,
  // so the relay counts as released long enough ago to start right away.
  s_relay_state = false;
  s_released_ms = millis() - FAN_MIN_OFF_MS;
  s_engaged_ms = 0;
  s_diff_count = 0;
  s_diff_next = 0;
  s_push_out_ms = 0;
  s_push_in_ms = 0;

  // У привода заслонки нет пружинного возврата, поэтому после подачи питания
  // положение захлопки неизвестно. Принудительно закрываем: концевик внутри
  // привода сам разомкнёт цепь в крайнем положении.
  valve_home();
}

// Остановка с указанием причины. Наработка фиксируется до смены режима, а
// канал притока закрывается: продолжать подавать уличный воздух в помещение,
// которое мы перестали осушать, смысла нет.
static void stop(FanBlock block) {
  unsigned long now = millis();
  if (relay_engaged(s_status.mode)) {
    credit_runtime(now);
  }
  s_status.mode = (block == BLOCK_COOLDOWN) ? FAN_COOLDOWN : FAN_IDLE;
  s_status.block = block;
  s_phase_start_ms = now;

  // На время обязательной паузы канал остаётся открытым: закрытая и снова
  // открытая заслонка стоила бы 24 секунды хода при том, что через 15 минут
  // канал всё равно открыли бы. Закрываем только при настоящей остановке.
  // Повторная остановка в том же состоянии (например, BLOCK_SENSOR пересчитывается
  // каждый проход loop) не должна перезапускать импульс: заслонка уже закрыта —
  // оставляем её в покое, иначе реле закрытия пульсирует без конца.
  if (block != BLOCK_COOLDOWN && valve_position() != VALVE_POS_CLOSED) {
    valve_close();
  }
}

// Пуск: сначала открываем канал и только потом включаем вентилятор. Пока
// захлопка в пути, реле вентилятора остаётся выключенным — иначе он гнал бы
// воздух в закрытый канал.
static void start(unsigned long now) {
  valve_open();
  s_status.mode = FAN_OPENING;
  s_status.block = BLOCK_VALVE;
  s_run_start_ms = now;
  s_phase_start_ms = now;
}

void fan_update(const SensorData& out, const SensorData& in,
                const SurfaceData& floor) {
  unsigned long now = millis();

  // Датчик поверхности — единственный в схеме необязательный элемент: его
  // отказ не должен останавливать осушение, поэтому для него отдельная
  // проверка давности и собственный запас в проверке росы.
  bool floor_ok = floor.valid && (now - floor.last_good_ms) < DATA_STALE_MS;
  float t_crit = floor_ok ? floor.temperature : in.temperature - DEW_FALLBACK_C;

  // Внутренний воздух насыщен относительно самой холодной поверхности — значит,
  // вода на ней уже выпадает. Это диагноз текущей сырости, а не запрет: приток
  // более сухого воздуха как раз и убирает причину, поэтому на решение о работе
  // он не влияет.
  s_status.surface_wet = floor_ok && in.valid &&
                         dew_point(in.temperature, in.humidity) > floor.temperature;

  // Cold inlet air is not a reason to stop by itself: it is exactly the winter
  // drying case. It is reported so the operator sees why the floor may get wet
  // later, and the run limit above handles the case where it actually does.
  s_status.inlet_cold = in.valid && out.valid &&
                        dew_point(in.temperature, in.humidity) > out.temperature;

  // A wet floor is normally the symptom the fan is here to remove, so the only
  // thing that stops on it is persistence: once the surface has been wet for
  // FLOOR_WET_MAX_MS since the fan started drying, the incoming air has cooled
  // it below the dew point of the room air and the fan feeds condensation
  // instead of drying. The clock starts only while the fan runs - a cellar that
  // is wet before the first start must get that start - and it is not reset by
  // a stop, only by the floor actually drying: restarting into the same wet
  // surface would buy nothing but two more strokes of the drive.
  if (!s_status.surface_wet) {
    s_wet_since_ms = 0;
  } else if (relay_engaged(s_status.mode) && s_wet_since_ms == 0) {
    s_wet_since_ms = now;
  }
  bool wet_persisted = s_wet_since_ms != 0 &&
                       (now - s_wet_since_ms) >= FLOOR_WET_MAX_MS;

  // Принудительный режим имеет безусловный приоритет: решение принял человек,
  // поэтому алгоритм осушения и все его ограничители (включая достоверность
  // данных) откладываются, пока режим не снят кнопкой. Пока заслонка в пути,
  // реле не подаётся — вентилятор должен иметь что гнать.
  if (s_force) {
    if (valve_position() == VALVE_POS_MOVING) {
      s_status.mode = FAN_OPENING;
      s_status.block = BLOCK_VALVE;
      relay(false);
      return;
    }
    if (valve_position() != VALVE_POS_OPEN) {
      valve_open();
      s_status.mode = FAN_OPENING;
      s_status.block = BLOCK_VALVE;
      s_run_start_ms = now;
      s_credited_ms = now;
      relay(false);
      return;
    }
    s_status.mode = FAN_FORCE;
    s_status.block = BLOCK_NONE;
    credit_runtime(now);
    relay(true);
    return;
  }

  // 1. Свежесть и достоверность данных — безусловный приоритет: нельзя
  //    принимать решение об осушении по несуществующим показаниям.
  //    Давность считается по последнему успешному чтению: last_read_ms
  //    обновляется и при неудаче (он отсчитывает период опроса), поэтому
  //    по нему DATA_STALE_MS был бы недостижим.
  bool fresh = out.valid && in.valid &&
               (now - out.last_good_ms) < DATA_STALE_MS &&
               (now - in.last_good_ms) < DATA_STALE_MS;
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

  // 3. Открытие канала. Пока захлопка в пути, вентилятор не включаем, а все
  //    остальные проверки пропускаем: они будут пересчитаны в следующем
  //    проходе, когда канал откроется.
  if (s_status.mode == FAN_OPENING) {
    if (valve_position() == VALVE_POS_MOVING) {
      relay(false);
      return;
    }
    // Импульс отработал, положение — открыто. Отсчёт лимита непрерывной
    // работы ведём от реального пуска вентилятора, а не от подачи сигнала
    // приводу, поэтому обнуляем точку отсчёта здесь.
    s_status.mode = FAN_RUNNING;
    s_status.block = BLOCK_NONE;
    s_run_start_ms = now;
    s_credited_ms = now;
  }

  // Пока вентилятор включён, поддерживаем актуальные часы наработки.
  if (s_status.mode == FAN_RUNNING) {
    credit_runtime(now);
  }

  Settings st = settings_get();

  // The difference is pushed once per poll and the decision uses the median of
  // the last DIFF_FILTER_SAMPLES values. A raw DHT22 sample carries +-0.5 degC
  // and +-2 % RH, which is about +-1.5 g/kg on the mixing ratio difference -
  // wider than the X_HYST_ON minus X_HYST_OFF band, so one bad reading must
  // not flip the relay. The median of three ignores a lone outlier.
  push_diff(in.mixing_ratio - out.mixing_ratio, out.last_good_ms,
            in.last_good_ms);
  float diff = median_diff();
  bool running = (s_status.mode == FAN_RUNNING);

  // 4. Температурный коридор наружного воздуха. Уставки целочисленные, а
  //    показание - float с шумом +-0.5 град, поэтому выход из коридора
  //    останавливает вентилятор по порогу с запасом: иначе на границе реле
  //    дребещет и каждый переход стоит двух полных ходов привода.
  float hyst = running ? T_CORRIDOR_HYST_C : 0.0f;
  if (out.temperature < (float)st.t_min - hyst) {
    stop(BLOCK_TEMP_LOW);
    relay(false);
    return;
  }
  if (out.temperature > (float)st.t_max + hyst) {
    stop(BLOCK_TEMP_HIGH);
    relay(false);
    return;
  }

  // 5. Lower bound of the indoor temperature. The run limit alone is not a
  //    protection against overcooling: thirty minutes on and fifteen off still
  //    walks the room toward the outdoor temperature, because the limit counts
  //    the fan, not the air it moves. Here the indoor sensor takes part in the
  //    decision directly, so the fan stops as soon as the room itself gets
  //    cold. Checked before the humidity hysteresis: drying a room at the cost
  //    of its temperature is not drying anything.
  //    Same band as the corridor: the set point is an integer, the reading
  //    carries noise, and a flip at the threshold costs two strokes of the
  //    drive.
  if (in.temperature < (float)st.t_in_min - (running ? T_IN_MIN_HYST_C : 0.0f)) {
    stop(BLOCK_TEMP_IN_LOW);
    relay(false);
    return;
  }

  // 6. Защита от конденсата. Приточный воздух безопасен, пока его точка росы
  //    не поднялась к температуре самой холодной конструкции. Пока датчик
  //    поверхности исправен, берается его реальное показание; иначе
  //    конструкции аппроксимируются внутренним воздухом минус запас.
  float dew_out = dew_point(out.temperature, out.humidity);
  float dew_limit = t_crit - DEW_MARGIN_C + (running ? DEW_HYST_C : 0.0f);
  if (dew_out > dew_limit) {
    stop(BLOCK_DEW);
    relay(false);
    return;
  }

  // 7. Гистерезис по влагосодержанию.
  if (s_status.mode == FAN_RUNNING) {
    // Valve position can change without the controller's knowledge: the
    // operator switched VALVE in the settings menu and the flap went home.
    // Running against a closed channel is exactly what the opening phase
    // exists to prevent, so re-open it instead of pushing air into a
    // closed duct.
    if (valve_position() != VALVE_POS_OPEN) {
      valve_open();
      s_status.mode = FAN_OPENING;
      s_status.block = BLOCK_VALVE;
      relay(false);
      return;
    }
    // Лимит непрерывной работы: защита от переохлаждения помещения.
    if ((now - s_run_start_ms) >= FAN_RUN_MAX_MS) {
      stop(BLOCK_COOLDOWN);
      relay(false);
      return;
    }
    // Упорный конденсат на полу при работающем вентиляторе: приток остудил
    // поверхность ниже точки росы комнатного воздуха и теперь воду не уносит,
    // а добавляет. Дальше крутить бессмысленно — помещение нужно греть, а не
    // вентилировать, и решение об этом принимает человек.
    if (wet_persisted) {
      stop(BLOCK_WET);
      relay(false);
      return;
    }
    // Выключаем, только когда преимущество наружного воздуха почти иссякло:
    // нижний порог ниже верхнего и не даёт реле циклировать на месте.
    // Минимальное время работы держит привод в покое: каждый пуск стоит двух
    // полных ходов заслонки, а шуму достаточно перескочить через узкую полосу.
    if (diff < X_HYST_OFF && relay_allowed_off(now)) {
      stop(BLOCK_DRY);
      relay(false);
      return;
    }
    relay(true);
    return;
  }

  // IDLE: включаем при достаточной разнице. Значения между порогами
  // оставляют контроллер в покое — это и есть зона нечувствительности.
  // Упорный конденсат запрещает и пуск: цикл старт-стоп на мокром полу стоит
  // приводу двух ходов и ровно ничего не сушит.
  if (wet_persisted) {
    s_status.block = BLOCK_WET;
    relay(false);
    return;
  }
  if (diff > X_HYST_ON && relay_allowed_on(now)) {
    start(now);
    relay(false);
    return;
  }

  s_status.block = BLOCK_DRY;
  relay(false);
}

FanStatus fan_status() { return s_status; }

bool fan_is_on() { return relay_engaged(s_status.mode); }

void fan_force_start() {
  if (s_force) return;
  s_force = true;

  unsigned long now = millis();
  // Переход из штатной работы не должен потерять уже наработанные секунды.
  if (relay_engaged(s_status.mode)) credit_runtime(now);

  valve_open();
  s_status.mode = FAN_OPENING;
  s_status.block = BLOCK_VALVE;
  s_run_start_ms = now;
  s_credited_ms = now;
  s_phase_start_ms = now;
  relay(false);
}

void fan_force_stop() {
  if (!s_force) return;
  s_force = false;

  // Причина — NONE: дальнейшее решение снова за алгоритмом, он пересчитает
  // его в этом же цикле loop().
  //
  // stop() обязана закрыть канал: при остановленном вентиляторе открытый
  // приток — это оседание конденсата на решётке. Повторного импульса здесь не
  // возникает: вызов однократный, а drive() в valve.cpp блокирует подачу обеих
  // фаз, так что OPEN+CLOSE одновременно не замкнутся.
  stop(BLOCK_NONE);
  relay(false);
}

bool fan_force_active() { return s_force; }

void fan_runtime_reset() {
  // Секунды, накопленные с прошлого credit_runtime, сбрасывать не нужно:
  // settings_reset_runtime() обнуляет и их. Точку отсчёта переносим на сейчас,
  // иначе следующее начисление добавило бы к нулю уже зачтённое время.
  settings_reset_runtime();
  s_credited_ms = millis();
  s_status.runtime_s = 0;
}
