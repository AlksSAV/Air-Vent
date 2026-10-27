#include "sensors.h"
#include "config.h"
#include "humidity.h"
#include <DHT.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <math.h>
#include <string.h>

// Библиотека Adafruit задаёт пин и тип датчика в конструкторе, поэтому
// объекты создаются на уровне файла.
static DHT dht_out(PIN_DHT_OUT, DHT22);
static DHT dht_in(PIN_DHT_IN, DHT22);

// DallasTemperature построена поверх OneWire и требует указатель на шину.
static OneWire floor_bus(PIN_FLOOR);
static DallasTemperature floor_sensor(&floor_bus);

static Adafruit_BMP280 baro;

static SensorData s_out;
static SensorData s_in;
static SurfaceData s_floor;
static BaroData s_baro;
static uint8_t s_errors_out = 0;
static uint8_t s_errors_in = 0;
static uint8_t s_errors_floor = 0;
static uint8_t s_errors_baro = 0;

// Действующее давление для расчёта влагосодержания: реальное с барометра либо
// справочное AMBIENT_HPA, пока он молчит. На решение о вентиляции оно почти не
// влияет (ошибка сокращается в разности), поэтому отказа как отдельного
// состояния нет — виден он только как точность числа на экране.
static float s_pressure_hpa = AMBIENT_HPA;

// Успешно ли опознана микросхема: повторный begin() нужен только после
// пропадания контакта, а не на каждом опросе.
static bool s_baro_ready = false;

static void recompute_mixing() {
  s_out.mixing_ratio = mixing_ratio(s_out.temperature, s_out.humidity, s_pressure_hpa);
  s_in.mixing_ratio = mixing_ratio(s_in.temperature, s_in.humidity, s_pressure_hpa);
}

// Конверсия DS18B20 в 12 битах идёт 750 мс, а блокирующая пауза остановила бы
// loop() и роняла опрос клавиш. Поэтому цикл разведён на два прохода: в одном
// запускаем конверсию, во втором — через FLOOR_CONVERT_MS — снимаем результат.
static bool s_floor_converting = false;
static unsigned long s_floor_convert_started_ms = 0;

void sensors_init() {
  memset(&s_out, 0, sizeof(s_out));
  memset(&s_in, 0, sizeof(s_in));
  memset(&s_floor, 0, sizeof(s_floor));
  memset(&s_baro, 0, sizeof(s_baro));

  dht_out.begin();
  dht_in.begin();

  // baro.begin() поднимает и шину Wire. Отказ барометра не блокирует остальное:
  // влагосодержание тогда считается по AMBIENT_HPA, а попытка опознать
  // микросхему повторяется в update_baro().
  s_baro_ready = baro.begin(BARO_I2C_ADDRESS);

  floor_sensor.begin();
  // Без этого requestTemperatures() внутри себя дожидается конца конверсии и
  // блокирует loop() на 750 мс.
  floor_sensor.setWaitForConversion(false);
  floor_sensor.setResolution(12);

  // Первое чтение — через SENSOR_FIRST_READ_MS, а не через полный период:
  // DHT22 нужна секунда на прогрев после подачи питания, но держать экран в
  // SENSOR ERROR целую минуту незачем. Дальше период отсчитывается штатно.
  // Разность считается в беззнаковой арифметике, поэтому заведомо «старая»
  // отметка времени корректно даёт срабатывание через SENSOR_FIRST_READ_MS.
  unsigned long now = millis();
  s_out.last_read_ms = now - (SENSOR_PERIOD_MS - SENSOR_FIRST_READ_MS);
  s_in.last_read_ms = s_out.last_read_ms;
  // Полу тоже нечего ждать две минуты: конверсия стартует сразу после прогрева.
  s_floor.last_read_ms = now - (FLOOR_PERIOD_MS - SENSOR_FIRST_READ_MS);
  s_baro.last_read_ms = s_out.last_read_ms;
  s_baro.pressure_hpa = AMBIENT_HPA;
}

// Очередное чтение. Данные признаются годными только если оба значения
// числовые и внутри физического диапазона DHT22: отдельные осколки появляются
// при наводках на линию данных и не должны попадать в расчёт.
static void read_sensor(DHT& dht, SensorData& data, uint8_t& errors) {
  bool ok = dht.read();
  float temp = ok ? dht.readTemperature() : NAN;
  float hum = ok ? dht.readHumidity() : NAN;

  bool plausible = isfinite(temp) && isfinite(hum) && hum >= 0.0f &&
                   hum <= 100.0f && temp >= -40.0f && temp <= 60.0f;

  if (plausible) {
    errors = 0;
    data.temperature = temp;
    data.humidity = hum;
    data.absolute_humidity = absolute_humidity(temp, hum);
    data.mixing_ratio = mixing_ratio(temp, hum, s_pressure_hpa);
    data.valid = true;
    return;
  }

  if (errors < SENSOR_MAX_ERRORS) errors++;
  // Последнее достоверное значение сохраняем: контроллер сам отбракует
  // данные по давности через DATA_STALE_MS.
  data.valid = false;
}

// Опрос DS18B20 в два прохода: запуск конверсии и, спустя FLOOR_CONVERT_MS,
// снятие результата. Отсутствие датчика на линии (подтяжка есть, микросхемы
// нет) проявляется как -125 град либо как 85 град — значения-маркеры 1-Wire,
// оба отсекаются диапазоном.
static void update_floor(unsigned long now) {
  if (s_floor_converting) {
    if ((now - s_floor_convert_started_ms) < FLOOR_CONVERT_MS) return;

    s_floor_converting = false;
    float temp = floor_sensor.getTempCByIndex(0);
    bool plausible = isfinite(temp) && temp >= -40.0f && temp <= 60.0f;

    if (plausible) {
      s_errors_floor = 0;
      s_floor.temperature = temp;
      s_floor.valid = true;
      s_floor.last_read_ms = now;
      return;
    }

    if (s_errors_floor < FLOOR_MAX_ERRORS) s_errors_floor++;
    s_floor.valid = false;
    return;
  }

  if ((now - s_floor.last_read_ms) < FLOOR_PERIOD_MS) return;

  floor_sensor.requestTemperatures();
  s_floor_converting = true;
  s_floor_convert_started_ms = now;
}

// Давление нужно только влагосодержанию, а оно входит в решение разностью.
// Поэтому отказ барометра не переводит систему в ошибку: остаётся последнее
// значение, а до него — константа AMBIENT_HPA.
static void update_baro(unsigned long now) {
  if ((now - s_baro.last_read_ms) < BARO_PERIOD_MS) return;
  s_baro.last_read_ms = now;

  if (!s_baro_ready) {
    s_baro_ready = baro.begin(BARO_I2C_ADDRESS);
    if (!s_baro_ready) {
      s_baro.valid = false;
      return;
    }
  }

  // readPressure() отдаёт паскали.
  float pressure_hpa = baro.readPressure() / 100.0f;
  bool plausible = isfinite(pressure_hpa) && pressure_hpa >= BARO_MIN_HPA &&
                   pressure_hpa <= BARO_MAX_HPA;

  if (!plausible) {
    if (s_errors_baro < SENSOR_MAX_ERRORS) s_errors_baro++;
    s_baro.valid = false;
    // Значение за пределами физики означает рассогласование с микросхемой:
    // следующий проход начнём с повторного опознавания.
    if (s_errors_baro >= SENSOR_MAX_ERRORS) s_baro_ready = false;
    return;
  }

  s_errors_baro = 0;
  s_baro.pressure_hpa = pressure_hpa;
  s_baro.valid = true;

  // Давление общее для обоих потоков, пересчитываем влагосодержание обоих.
  s_pressure_hpa = pressure_hpa;
  recompute_mixing();
}

void sensors_update() {
  unsigned long now = millis();

  // Период отсчитывается от попытки чтения, а не от успешного результата:
  // иначе при отказе датчика опрос пойдёт на каждом проходе loop, что для
  // DHT22 недопустимо.
  if (now - s_out.last_read_ms >= SENSOR_PERIOD_MS) {
    read_sensor(dht_out, s_out, s_errors_out);
    s_out.last_read_ms = now;
  }

  if (now - s_in.last_read_ms >= SENSOR_PERIOD_MS) {
    read_sensor(dht_in, s_in, s_errors_in);
    s_in.last_read_ms = now;
  }

  update_floor(now);
  update_baro(now);
}

SensorData sensors_get_out() { return s_out; }

SensorData sensors_get_in() { return s_in; }

SurfaceData sensors_get_floor() { return s_floor; }

BaroData sensors_get_baro() { return s_baro; }
