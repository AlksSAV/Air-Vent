#include "sensors.h"
#include "config.h"
#include "humidity.h"
#include <DHT.h>
#include <math.h>
#include <string.h>

// Библиотека Adafruit задаёт пин и тип датчика в конструкторе, поэтому
// объекты создаются на уровне файла.
static DHT dht_out(PIN_DHT_OUT, DHT22);
static DHT dht_in(PIN_DHT_IN, DHT22);

static SensorData s_out;
static SensorData s_in;
static uint8_t s_errors_out = 0;
static uint8_t s_errors_in = 0;

void sensors_init() {
  memset(&s_out, 0, sizeof(s_out));
  memset(&s_in, 0, sizeof(s_in));

  dht_out.begin();
  dht_in.begin();

  // Отсчитываем период от старта, чтобы первое чтение не случилось сразу
  // после подачи питания: DHT22 нужна секунда на прогрев.
  s_out.last_read_ms = millis();
  s_in.last_read_ms = millis();
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
    data.valid = true;
    return;
  }

  if (errors < SENSOR_MAX_ERRORS) errors++;
  // Последнее достоверное значение сохраняем: контроллер сам отбракует
  // данные по давности через DATA_STALE_MS.
  data.valid = false;
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
}

SensorData sensors_get_out() { return s_out; }

SensorData sensors_get_in() { return s_in; }
