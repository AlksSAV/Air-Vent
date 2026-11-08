#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <avr/wdt.h>  // WDTO_2S for WATCHDOG_TIMEOUT_MS

// ---------------------------------------------------------------------------
// Пины. Arduino Uno + LCD Keypad Shield.
// Shield занимает D4..D10 и A0 (клавиатура). Свободны: D0..D3, D11..D13, A1..A5.
// ---------------------------------------------------------------------------

// LCD 1602 щита подключён напрямую (без I2C). Вывод RW на щите сидит на GND.
#define PIN_LCD_RS  8
#define PIN_LCD_EN  9
#define PIN_LCD_D4  4
#define PIN_LCD_D5  5
#define PIN_LCD_D6  6
#define PIN_LCD_D7  7

// Пять клавиш щита — резистивный делитель на одной аналоговой линии.
#define PIN_BUTTONS A0

// Датчики DHT22.
#define PIN_DHT_OUT  A1  // наружный
#define PIN_DHT_IN   A2  // внутренний

// Реле вентилятора (через оптопару/транзистор), активен высокий уровень.
// D3 выбран потому, что D2 занят миганием TX при прошивке, а shield закрыл D4..D10.
#define PIN_FAN 3

// DS18B20 самой холодной поверхности (пол в северном углу). 1-Wire,
// подтяжка 4.7 кОм к 5В. A3 — единственный свободный аналог: A0 занят
// клавиатурой щита, A1/A2 — DHT22.
#define PIN_FLOOR A3

// Привод заслонки приточного канала — два реле: открытие и закрытие. Внутри
// привода концевики, которые сами размыкают цепь в крайних положениях, поэтому
// удерживать реле включённым не нужно, а обратная связь контроллеру не
// требуется. D11 и D12 свободны: D4..D10 заняты щитом, D3 — реле вентилятора.
#define PIN_VALVE_OPEN  11
#define PIN_VALVE_CLOSE 12

// Активный уровень выходов. SSR вентилятора открывается 5 В, то есть HIGH.
// Модуль реле привода заслонки — GND-trigger: замыкается при LOW, а при обоих
// пинах LOW держит замкнутыми обе фазы. Именно это выглядело как «два реле
// сразу» при работающем вентиляторе, хотя программа оба входа одновременно не
// включала. Раздельные определения нужны потому, что логика у выходов
// противоположная.
#define FAN_RELAY_ACTIVE_LEVEL   HIGH
#define VALVE_RELAY_ACTIVE_LEVEL LOW

// Продолжительность импульса на реле привода. Концевик внутри привода сам
// разомкнёт цепь, когда захлопка дойдёт до крайнего положения, но реле мы
// держим фиксированное время и затем снимаем управление, чтобы мотор не гудел
// в упоре. Задаётся в меню настроек (секунды), 12 хватает с большим запасом.
#define VALVE_PULSE_DEFAULT_S 12

// Пределы настройки длительности хода привода, сек.
#define VALVE_PULSE_MIN_S  1
#define VALVE_PULSE_MAX_S  60
#define VALVE_PULSE_STEP_S 1

// Break-before-make pause between switching the drive relays. Contacts of a
// relay need a few milliseconds to fall back; without the pause the new
// direction closes while the old one is still shut, putting both phases of
// the motor under voltage at the same time. With COM1 and COM2 taken from the
// same phase nothing shorts, but drives usually require a dead time anyway.
#define VALVE_DEADTIME_MS 60UL

// ---------------------------------------------------------------------------
// Клавиатура щита: эталонные значения АЦП (10 бит) при нажатой клавише.
// Границы между клавишами берутся как середины между этими уровнями.
// ---------------------------------------------------------------------------
#define ADC_KEY_RIGHT  0
#define ADC_KEY_UP     145
#define ADC_KEY_DOWN   329
#define ADC_KEY_LEFT   505
#define ADC_KEY_SELECT 741
#define ADC_KEY_NONE   1023

// Окно подавления дребезга, мс.
#define BUTTON_DEBOUNCE_MS 50

// Максимальный интервал между двумя нажатиями, чтобы они засчитались как
// двойное. Заведомо больше дребезга и заметно меньше человеческой паузы.
#define BUTTON_DOUBLE_CLICK_MS 400

// Удержание клавиши, после которого приходит отдельное событие. В меню это
// «назад» и необратимые действия вроде сброса наработки, поэтому порог
// заметно больше случайного залипания пальца, но меньше, чем хочется
// держать кнопку.
#define BUTTON_HOLD_MS 700

// ---------------------------------------------------------------------------
// Опрос датчиков
// ---------------------------------------------------------------------------

// Период опроса DHT22, мс. datasheet ограничивает частоту 0,5 Гц (не чаще
// раза в 2 с); 15 с — компромисс между отзывчивостью экрана и запасом по
// нагреву корпуса датчика от собственного импульса измерения.
#define SENSOR_PERIOD_MS (15UL * 1000UL)

// Задержка перед самым первым опросом после подачи питания, мс. DHT22 нужна
// секунда на прогрев, поэтому сразу читать нельзя, но и ждать полный период
// незачем: экран висел бы в SENSOR ERROR целую минуту. Один дополнительный
// опрос ресурс датчика не расходует.
#define SENSOR_FIRST_READ_MS 2000UL

// Сколько подряд неудачных чтений до перевода датчика в состояние ошибки.
#define SENSOR_MAX_ERRORS 3

// Lower bound of a plausible humidity reading. 0 % is the signature of a
// broken DHT22 (open signal line, dried-out sensor, pickup from the SSR), not
// a weather condition: in a temperate climate outdoor air never dries that far.
// Such a reading must be rejected, otherwise absolute_humidity() reports 0 and
// the fan starts for a full 30-minute cycle on data that does not exist.
#define RH_MIN_PLAUSIBLE 5.0f

// Данные считаются недействительными, если датчик не обновлялся дольше этого
// времени. Вентилятор при этом принудительно выключается.
#define DATA_STALE_MS (10UL * 60UL * 1000UL)

// DS18B20 критической поверхности. Конверсия 12 бит занимает 750 мс, поэтому
// опрос разнесён на два прохода (запуск конверсии и снятие результата) и
// идёт в два раза реже DHT22: температура бетона и металла за минуту не
// успевает измениться, а линия 1-Wire при этом не мешает чтению DHT.
#define FLOOR_PERIOD_MS (15UL * 1000UL)
#define FLOOR_CONVERT_MS 800UL

// Отказ единственного датчика поверхности не должен останавливать осушение,
// но и молча включать вентилятор нельзя: запас берут с избытком.
#define FLOOR_MAX_ERRORS 3

// BMP280 на шине I2C (A4 — SDA, A5 — SCL). Давление нужно только для
// влагосодержания, а оно входит в решение разностью, где ошибка по P почти
// сокращается; поэтому датчик справочный и период у него редкий.
#define BARO_PERIOD_MS (15UL * 1000UL)

// Адрес 0x76 у модулей, где перемычка SDO замкнута на GND (таких большинство);
// 0x77 — значение по умолчанию в библиотеке.
#define BARO_I2C_ADDRESS 0x76

// Значение, подставляемое при отказе барометра: средняя высота уровня моря.
// Ошибка реальной погоды против этой константы (±5 %) входит в x и почти
// полностью сокращается в разности x_внутри - x_снаружи.
#define AMBIENT_HPA 1013.25f

// Физический диапазон барометра: вне его показания считаются наводкой на I2C.
// Нижняя граница поднята до 800 гПа (~2000 м над уровнем моря): при 300 гПа
// сбойное чтение проходит проверку и мусор попадает в расчёт влагосодержания.
#define BARO_MIN_HPA 800.0f
#define BARO_MAX_HPA 1100.0f

// Предельный скачок давления между двумя соседними чтениями, гПа. Реальная
// погода меняет давление на единицы гПа за часы; скачок в десятки — признак
// сбоя шины I2C, а не погоды.
#define BARO_MAX_JUMP_HPA 20.0f

// I2C bus timeout, microseconds. The Wire core ships with the timeout
// disabled (twi_timeout_us = 0), so a stuck SDA line sends twi_writeTo() into
// an endless loop: loop() never returns and the fan relay stays latched on.
// 25 ms is the SMBus limit for clock stretching; after a timeout the core
// resets the TWI hardware by itself.
#define I2C_TIMEOUT_US 25000

// Watchdog period. The controller switches a 230 V load unattended, so a hang
// is not acceptable. 2 s is far above one loop() pass and short enough to stop
// a runaway fan well before the run limit becomes relevant.
#define WATCHDOG_TIMEOUT_MS WDTO_2S

// ---------------------------------------------------------------------------
// Алгоритм осушения
// ---------------------------------------------------------------------------

// Hysteresis of the drying decision, in g of water per kg of dry air. The fan
// starts when the room air exceeds the inlet air by X_HYST_ON and stops when
// that advantage falls below X_HYST_OFF. The gap between them is what keeps the
// relay from cycling in place.
//
// Mixing ratio, not absolute humidity, because the fan moves mass of air: x is
// unchanged when the inlet air is heated from -10 degC to +20 degC, while g/m3
// is not. A threshold in g/m3 therefore demanded a different advantage in every
// season - the same 2.0 g/m3 meant anything from 1.04 to 3.12 g/kg depending on
// the pair of temperatures, a threefold spread, and cold dry winter air was used
// less eagerly than the physics allowed.
//
// The band is deliberately wider than a unit conversion of the old thresholds
// (1.6 - 0.4 = 1.2 g/kg): the worst-case noise of one DHT22 pair on this
// difference is about 1.5 g/kg, and a band narrower than the noise lets the
// relay flip on a drifting value. The upper threshold carries the widening, so
// the fan simply needs a slightly clearer advantage to start.
#define X_HYST_ON  1.9f
#define X_HYST_OFF 0.4f

// Number of readings the decision is averaged over. One raw DHT22 sample
// carries +-0.5 degC and +-2 % RH, which is up to 1.5 g/kg on the difference in
// mixing ratio - comparable to the X_HYST_ON minus X_HYST_OFF band, so a single
// noisy sample could otherwise flip the relay. The median of three ignores a
// lone outlier and costs only two extra polling periods before a state change.
#define DIFF_FILTER_SAMPLES 3

// Shortest time the fan stays engaged after it has been switched on, and the
// shortest rest after it has been switched off. Without them a value drifting
// around a threshold cycles the drive every SENSOR_PERIOD_MS, and every such
// cycle is two full 12 s strokes of the motor.
#define FAN_MIN_RUN_MS (10UL * 60UL * 1000UL)
#define FAN_MIN_OFF_MS (5UL * 60UL * 1000UL)

// Защита от конденсата. Приточный воздух безопасен, пока его точка росы
// лежит ниже температуры самой холодной конструкции не менее чем на
// DEW_MARGIN_C градусов. Пока DS18B20 поверхности исправен, берётся его
// реальное показание и запас может быть малым; при отказе датчика контроллер
// возвращается к аппроксимации "температура воздуха минус DEW_FALLBACK_C".
#define DEW_MARGIN_C 1.0f
#define DEW_FALLBACK_C 3.0f

// Extra margin the fan runs on once it is already engaged. A single threshold
// makes the relay chatter on sensor noise near it, and every flip costs two
// full strokes of the drive, so stopping is allowed only a little closer to
// the limit than starting.
#define DEW_HYST_C 1.0f

// How long the floor may stay wet while the fan is actually running. The check
// above only asks whether the inlet air deposits water on the surface it has
// now; it cannot see that cold inlet air cools the surface itself. Once the
// floor falls below the dew point of the room air, condensation comes from the
// room, and a fan that keeps running is feeding it. Only a persistent wet
// floor stops the cycle: the reading is noisy, and the condition clears itself
// as the room dries, so a brief WET must not abort a working run.
#define FLOOR_WET_MAX_MS (20UL * 60UL * 1000UL)

// Same for the outdoor temperature corridor: the set points are integers while
// the reading is a float with +-0.5 degC of noise.
#define T_CORRIDOR_HYST_C 1.0f

// Lowest allowed indoor temperature. The run limit alone cannot protect the
// room from overcooling: 30 minutes on and 15 minutes off still moves the whole
// room toward the outdoor temperature when the inlet air is much colder. This
// set point reads the indoor sensor directly, so it stops the fan as soon as
// the room itself gets cold, and it is checked before the humidity hysteresis -
// drying the room is pointless if it costs its temperature.
#define T_IN_MIN_DEFAULT 5

// Range of the indoor floor setting, deg.
#define T_IN_MIN_ALLOWED_MIN 0
#define T_IN_MIN_ALLOWED_MAX 20
#define T_IN_MIN_STEP 1

// The fan keeps running this much above the floor. Same reason as the corridor
// hysteresis: the indoor reading has noise, and without a band the relay would
// flip at the threshold and every flip costs two full strokes of the drive.
#define T_IN_MIN_HYST_C 1.0f

// Ограничение непрерывной работы вентилятора и обязательная пауза после неё.
#define FAN_RUN_MAX_MS (30UL * 60UL * 1000UL)
#define FAN_PAUSE_MS   (15UL * 60UL * 1000UL)

// Пределы настройки температурного коридора наружного воздуха, град.
#define T_LIMIT_MIN_ALLOWED -10
#define T_LIMIT_MAX_ALLOWED 60
#define T_LIMIT_STEP        1

// Значения по умолчанию при первой прошивке.
#define T_OUT_DEFAULT_MIN 7
#define T_OUT_DEFAULT_MAX 40

// ---------------------------------------------------------------------------
// Периодичность служебных задач, мс
// ---------------------------------------------------------------------------
#define DISPLAY_PERIOD_MS 500

// Период автоматического листания экранов, мс. Отсчитывается от последней
// смены экрана: ручное нажатие Left/Right перезапускает отсчёт, поэтому
// экран, выбранный человеком, держится не меньше этого времени.
#define SCREEN_AUTO_ADVANCE_MS (20UL * 1000UL)

// Предельное время заставки при старте, мс. Заставка снимается раньше, как
// только DHT22 отдал первые годные данные; предел нужен на случай, когда
// датчик не отвечает вовсе — иначе экран завис бы на заставке.
#define BOOT_SPLASH_MS 5000UL

// Бездействие клавиш, после которого гаснет подсветка LCD, мс. Сам экран
// продолжает обновляться: контроллер работает как обычно, гаснет только
// подсветка — она потребляет больше, чем весь остальной щит.
#define DISPLAY_BACKLIGHT_TIMEOUT_MS (180UL * 1000UL)

// Часы работы вентилятора пишутся в EEPROM раз в час: у ATmega328P ресурс
// ячейки ~100 тыс. записей, частая запись их выработает.
#define RUNTIME_SAVE_PERIOD_MS (60UL * 60UL * 1000UL)

// Время бездействия клавиш в режиме редактирования, после которого
// настройки сохраняются, а интерфейс возвращается в обычный режим.
#define EDIT_TIMEOUT_MS (30UL * 1000UL)

// Upper bound of a plausible accumulated runtime, seconds. The fan is allowed to
// run at full duty for years: 100000 h is over 11 years of nonstop operation,
// far beyond anything the relay or the fan would survive. A larger value in
// EEPROM cannot be a real count - it is a torn write (the four bytes are written
// one after another, so losing power mid-write leaves a mix of old and new) or
// garbage from an unwritten cell - and must never reach the display.
// Kept in seconds rather than hours so the comparison needs no 32-bit multiply,
// which would cost the debug build the flash it does not have.
#define RUNTIME_MAX_S 360000000UL

// Период отладочной телеметрии в Serial, мс.
#define SERIAL_LOG_PERIOD_MS (5UL * 1000UL)

// ---------------------------------------------------------------------------
// Карта EEPROM. int16 хранится big-endian, как в исходной прошивке.
// ---------------------------------------------------------------------------
#define EEPROM_MAGIC_ADDR  0
// Число оставлено прежним сознательно: карта EEPROM расширена вниз по адресам,
// и сбрасывать настройки ради одного байта значило бы обнулить наработку
// вентилятора на уже работающих сборках.
#define EEPROM_MAGIC_VALUE 102
#define EEPROM_TMIN_ADDR   1  // int16
#define EEPROM_TMAX_ADDR   3  // int16
#define EEPROM_RUNTIME_ADDR 5  // uint32, накопленные секунды работы
#define EEPROM_VALVE_ADDR  9  // uint8, есть ли в сборке привод заслонки
#define EEPROM_VPULSE_ADDR 10 // uint16, длительность импульса привода, сек
#define EEPROM_TINMIN_ADDR 12 // int16, нижний предел температуры внутри, град
// CRC8 over bytes 5..8. The slot is placed after every other field: the tail of
// the map is byte 13, so on a board where a firmware with a checksum has never
// written anything this cell still holds 0xFF, the state of an erased byte.
// That is what lets an upgraded board keep its runtime - 0xFF means "no
// checksum stored yet", not "the record is damaged".
#define EEPROM_CRC_ADDR  14  // uint8

#endif  // CONFIG_H
