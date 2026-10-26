#include <Arduino.h>
#include <OneWire.h>
#include <Wire.h>

// Диагностика стенда: подтяжка линии 1-Wire, поиск устройств на шине, чтение
// температуры и скан I2C. Живёт в отдельном env (uno_diag) и в основную
// прошивку не попадает.

static const uint8_t PIN_FLOOR = A3;

static void print_hex8(uint8_t v) {
  if (v < 16) Serial.print('0');
  Serial.print(v, HEX);
}

static void scan_i2c() {
  Serial.println(F("I2C scan:"));
  Wire.begin();
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  device 0x"));
      print_hex8(addr);
      Serial.println();
      found++;
    }
  }
  if (found == 0) Serial.println(F("  none"));
}

static void scan_onewire() {
  Serial.println(F("1-Wire on A3:"));

  // Подтяжка держит отпущенную линию у Vcc. Без резистора вход висит в
  // воздухе, и АЦП показывает произвольный уровень — это и отличает одну
  // неисправность от другой.
  pinMode(PIN_FLOOR, INPUT);
  delay(5);
  int level = analogRead(PIN_FLOOR);
  Serial.print(F("  idle level: "));
  Serial.print(level);
  if (level > 1000) Serial.println(F(" (pull-up OK)"));
  else if (level < 100) Serial.println(F(" (line held LOW - short?)"));
  else Serial.println(F(" (no pull-up?)"));

  OneWire bus(PIN_FLOOR);
  if (!bus.reset()) {
    Serial.println(F("  no presence pulse - nothing answers"));
    return;
  }
  Serial.println(F("  presence pulse OK"));

  uint8_t count = 0;
  uint8_t addr[8];
  bus.reset_search();
  while (bus.search(addr)) {
    count++;
    Serial.print(F("  device #"));
    Serial.print(count);
    Serial.print(F(": "));
    for (uint8_t i = 0; i < 8; i++) {
      print_hex8(addr[i]);
      if (i + 1 < 8) Serial.print(' ');
    }
    Serial.print(F("  crc="));
    Serial.println(OneWire::crc8(addr, 7) == addr[7] ? F("ok") : F("BAD"));
  }
  if (count == 0) {
    Serial.println(F("  no devices found"));
    return;
  }

  // Конверсию и чтение скретчпада делаем вручную: DallasTemperature здесь
  // не нужна, а лишняя библиотека только увеличила бы образ.
  bus.reset();
  bus.skip();
  bus.write(0x44, 1);
  delay(800);
  bus.reset();
  bus.skip();
  bus.write(0xBE);
  uint8_t sp[9];
  for (uint8_t i = 0; i < 9; i++) sp[i] = bus.read();

  int16_t raw = (int16_t)(((uint16_t)sp[1] << 8) | sp[0]);
  Serial.print(F("  scratchpad crc="));
  Serial.print(OneWire::crc8(sp, 8) == sp[8] ? F("ok") : F("BAD"));
  Serial.print(F(" raw="));
  Serial.print(raw);
  Serial.print(F(" t="));
  Serial.print(raw / 16.0f, 3);
  Serial.println(F(" C"));
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("=== Air-Vent diagnostics ==="));
  scan_i2c();
  scan_onewire();
  Serial.println(F("=== done ==="));
}

// Повторяем: диагностику удобно снимать, не подгадывая момент после сброса.
void loop() {
  delay(5000);
  scan_i2c();
  scan_onewire();
  Serial.println(F("=== done ==="));
}
