#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// Данные одного датчика.
struct SensorData {
  float temperature;
  float humidity;
  float absolute_humidity;  // г/м3, вычисляется внутри
  bool  valid;
  unsigned long last_read_ms;
};

void sensors_init();

// Вызывать в loop(): пытается прочитать датчики, обновляет глобальные состояния.
void sensors_update();

SensorData sensors_get_out();
SensorData sensors_get_in();

#endif  // SENSORS_H
