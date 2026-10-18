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
  unsigned long last_read_ms;
};

// Температура самой холодной поверхности в помещении (пол). Влажность ей не
// измерется, а требование "оба значения годны" к ней неприменимо, поэтому
// состояние вынесено в отдельную структуру.
struct SurfaceData {
  float temperature;
  bool  valid;
  unsigned long last_read_ms;
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
