#ifndef HUMIDITY_H
#define HUMIDITY_H

#include <Arduino.h>

// Абсолютная влажность, г/м3, по температуре и относительной влажности.
// Давление насыщенного пара — уравнение Магнуса-Аldучч-Эспенса, плотность
// пара через уравнение состояния идеального газа.
float absolute_humidity(float temp_c, float rel_hum_pct);

// Температура точки росы, град. Нужна для оценки риска конденсата на
// конструкциях подвала.
float dew_point(float temp_c, float rel_hum_pct);

#endif  // HUMIDITY_H
