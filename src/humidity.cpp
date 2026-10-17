#include "humidity.h"
#include <math.h>

// Константы Магнуса для воды над плоской поверхностью в рабочем диапазоне
// температур подвала (-20..+50 град), точность лучше 0,4 %.
static const float MAGNUS_A = 17.625f;
static const float MAGNUS_B = 243.04f;
static const float SATURATION_HPA = 6.1094f;  // давление при 0 град, гПа
static const float GAS_CONSTANT_WATER_VAPOR = 461.5f;  // Дж/(кг*К)

float absolute_humidity(float temp_c, float rel_hum_pct) {
  if (temp_c <= -273.15f) return 0.0f;

  float e_hpa = SATURATION_HPA * expf(MAGNUS_A * temp_c / (MAGNUS_B + temp_c));
  float e_pa = e_hpa * 100.0f * (rel_hum_pct / 100.0f);

  // p = rho * R * T  ->  rho = p / (R * T), перевод кг/м3 в г/м3
  return e_pa / (GAS_CONSTANT_WATER_VAPOR * (temp_c + 273.15f)) * 1000.0f;
}

float dew_point(float temp_c, float rel_hum_pct) {
  if (rel_hum_pct <= 0.0f || rel_hum_pct > 100.0f) return -273.15f;

  float gamma = logf(rel_hum_pct / 100.0f) + MAGNUS_A * temp_c / (MAGNUS_B + temp_c);
  return MAGNUS_B * gamma / (MAGNUS_A - gamma);
}
