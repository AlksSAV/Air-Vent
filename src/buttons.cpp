#include "buttons.h"
#include "config.h"

// Стабильное состояние, состояние до него и «сырое» значение АЦП.
static Button s_stable = BTN_NONE;
static Button s_stable_prev = BTN_NONE;
static Button s_raw = BTN_NONE;
static unsigned long s_change_ms = 0;

// Детектор двойного нажатия считается по фронтам RELEASE->PRESS одной клавиши.
// Одинарное нажатие подтверждается только по истечении окна
// BUTTON_DOUBLE_CLICK_MS, иначе первое нажатие пары не отличить от одиночного.
static Button s_click_btn = BTN_NONE;
static uint8_t s_click_count = 0;
static unsigned long s_click_ms = 0;
static Button s_single_btn = BTN_NONE;
static Button s_double_btn = BTN_NONE;

// Удержание отсчитывается от подтверждённого фронта. Флаг нужен, чтобы
// событие пришло один раз: иначе каждый цикл loop() считал бы кнопку
// удержанной и повторял действие.
static unsigned long s_hold_start_ms = 0;
static bool s_hold_fired = false;

// Середина между эталонными уровнями двух соседних клавиш — естественная
// граница, она устойчива к разбросу резисторов щита лучше, чем фиксированный
// допуск вокруг одного значения. Незанятый вход подтянут к Vcc (АЦП ~1023),
// нажатие любой клавиши тянет напряжение вниз, поэтому большое значение —
// это «нет клавиши», а малое — RIGHT.
static Button adc_to_button(int adc) {
  if (adc >= (ADC_KEY_NONE + ADC_KEY_SELECT) / 2) return BTN_NONE;
  if (adc >= (ADC_KEY_SELECT + ADC_KEY_LEFT) / 2) return BTN_SELECT;
  if (adc >= (ADC_KEY_LEFT + ADC_KEY_DOWN) / 2) return BTN_LEFT;
  if (adc >= (ADC_KEY_DOWN + ADC_KEY_UP) / 2) return BTN_DOWN;
  if (adc >= (ADC_KEY_UP + ADC_KEY_RIGHT) / 2) return BTN_UP;
  return BTN_RIGHT;
}

// Фронт нажатия: второе в окне — двойное, после чего серия закрывается, чтобы
// тройное нажатие не породило ещё одно событие.
static void register_press(Button b, unsigned long now) {
  if (s_click_count == 1 && b == s_click_btn &&
      (now - s_click_ms) < BUTTON_DOUBLE_CLICK_MS) {
    s_click_count = 0;
    s_double_btn = b;
    return;
  }

  s_click_btn = b;
  s_click_count = 1;
  s_click_ms = now;
}

void buttons_init() {
  analogReference(DEFAULT);
  s_raw = adc_to_button(analogRead(PIN_BUTTONS));
  s_stable = s_raw;
  s_stable_prev = s_raw;
  s_change_ms = millis();
}

void buttons_poll() {
  unsigned long now = millis();
  Button raw = adc_to_button(analogRead(PIN_BUTTONS));

  if (raw != s_raw) {
    // Уровень дрогнул — перезапускаем окно подавления дребезга, не принимая
    // переход за решение.
    s_raw = raw;
    s_change_ms = now;
  } else if (raw != s_stable && (now - s_change_ms) >= BUTTON_DEBOUNCE_MS) {
    // Уровень выстоялся: это настоящий переход, а не дребезг.
    s_stable_prev = s_stable;
    s_stable = raw;

    if (raw != BTN_NONE) {
      register_press(raw, now);
      s_hold_start_ms = now;
      s_hold_fired = false;
    }
    return;  // s_stable_prev обновлён, фронт увидит этот цикл
  }

  s_stable_prev = s_stable;

  // Второе нажатие так и не пришло — фиксируем одиночное.
  if (s_click_count == 1 && (now - s_click_ms) >= BUTTON_DOUBLE_CLICK_MS) {
    s_click_count = 0;
    s_single_btn = s_click_btn;
  }
}

Button buttons_current() { return s_stable; }

bool buttons_pressed(Button b) {
  return b != BTN_NONE && s_stable == b && s_stable_prev != b;
}

// События читаются один раз: после чтения флаг снимается, иначе обработчик
// получил бы одно и то же нажатие в каждом цикле loop().
bool buttons_single_clicked(Button b) {
  if (s_single_btn != b) return false;
  s_single_btn = BTN_NONE;
  return true;
}

bool buttons_double_clicked(Button b) {
  if (s_double_btn != b) return false;
  s_double_btn = BTN_NONE;
  return true;
}

bool buttons_held(Button b, unsigned long ms) {
  if (b == BTN_NONE || s_stable != b) return false;
  if (s_hold_fired) return false;
  if ((millis() - s_hold_start_ms) < ms) return false;
  s_hold_fired = true;
  return true;
}

void buttons_reset_clicks() {
  s_click_btn = BTN_NONE;
  s_click_count = 0;
  s_single_btn = BTN_NONE;
  s_double_btn = BTN_NONE;
  s_hold_fired = true;  // нажатие, начатое до сброса, удержанием не считается
}
