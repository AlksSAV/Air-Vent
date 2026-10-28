#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// Данные одного датчика.
struct SensorData {
  float temperature;
  float humidity;
  float absolute_humidity;  // г/м3, вычисляется внутри
  float mixing_ratio;       // г/кг, вычисляется внутри при текущем давлении
  bool  valid;
  unsigned long last_read_ms;  // момент последней ПОПЫТКИ чтения: по нему
                               // отсчитывается SENSOR_PERIOD_MS, чтобы отказ
                               // не превращал опрос в каждый проход loop()
  unsigned long last_good_ms;  // момент последнего УСПЕШНОГО чтения: по нему
                               // контроллер проверяет давность через
                               // DATA_STALE_MS
};

// Температура самой холодной поверхности в помещении (пол). Влажность ей не
// измерется, а требование "оба значения годны" к ней неприменимо, поэтому
// состояние вынесено в отдельную структуру.
struct SurfaceData {
  float temperature;
  bool  valid;
  unsigned long last_read_ms;  // период опроса: обновляется при запуске
                               // конверсии, иначе отказ датчика заставлял бы
                               // запрашивать её на каждом проходе loop()
  unsigned long last_good_ms;  // последнее успешное чтение, для DATA_STALE_MS
};

// Барометр. Влагосодержание входит в решение разностью, где ошибка по
// давлению почти сокращается, поэтому отказ барометра не блокирует осушение:
// подставляется AMBIENT_HPA.
struct BaroData {
  float pressure_hpa;
  bool  valid;
  unsigned long last_read_ms;
};

void sensors_init();

// Вызывать в loop(): пытается прочитать датчики, обновляет глобальные состояния.
void sensors_update();

SensorData sensors_get_out();
SensorData sensors_get_in();

// Поверхность может отсутствовать в сборке или выйти из строя, не обесточив
// остальную логику: потребитель обязан сам проверить valid и давность.
SurfaceData sensors_get_floor();

BaroData sensors_get_baro();

#endif  // SENSORS_H
