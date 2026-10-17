#include "buttons.h"
#include "config.h"

// Стабильное состояние, состояние до него и «сырое» значение АЦП.
static Button s_stable = BTN_NONE;
static Button s_stable_prev = BTN_NONE;
static Button s_raw = BTN_NONE;
static unsigned long s_change_ms = 0;

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
    return;  // s_stable_prev обновлён, фронт увидит этот цикл
  }

  s_stable_prev = s_stable;
}

Button buttons_current() { return s_stable; }

bool buttons_pressed(Button b) {
  return b != BTN_NONE && s_stable == b && s_stable_prev != b;
}
