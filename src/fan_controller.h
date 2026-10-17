#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <Arduino.h>
#include "sensors.h"

// Причина, по которой вентилятор выключен. Порядок важен: от самого
// важного для диагностики к частным.
enum FanBlock : uint8_t {
  BLOCK_NONE = 0,
  BLOCK_SENSOR,    // данные невалидны или устарели
  BLOCK_TEMP_LOW,  // наружный холоднее нижнего предела
  BLOCK_TEMP_HIGH, // наружный теплее верхнего предела
  BLOCK_DEW,       // риск конденсата: точка росы притока слишком высока
  BLOCK_DRY,       // осушать нечего, наружный воздух не суше внутреннего
  BLOCK_COOLDOWN,  // обязательная пауза после лимита непрерывной работы
};

enum FanMode : uint8_t {
  FAN_IDLE = 0,
  FAN_RUNNING,
  FAN_COOLDOWN,
  FAN_FORCE,  // ручное включение в обход алгоритма осушения
};

struct FanStatus {
  FanMode    mode;
  FanBlock   block;
  uint32_t   runtime_s;  // накопленные секунды работы
};

void fan_init();

// Пересчитывает состояние по актуальным данным датчиков.
void fan_update(const SensorData& out, const SensorData& in);

// Принудительная вентиляция: состояние переключается только вручную, оно
// держит реле включённым в обход влажности, температурного коридора и лимита
// непрерывной работы. Ограничения по времени нет — режим снимается кнопкой.
void fan_force_start();
void fan_force_stop();
bool fan_force_active();

FanStatus fan_status();

// Текущее решение о подаче питания на реле.
bool fan_is_on();

#endif  // FAN_CONTROLLER_H
