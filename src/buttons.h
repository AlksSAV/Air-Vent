#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

enum Button : uint8_t {
  BTN_NONE = 0,
  BTN_RIGHT,
  BTN_UP,
  BTN_DOWN,
  BTN_LEFT,
  BTN_SELECT,
};

// Опрос аналоговой клавиатуры щита. Возвращает состояние с учётом дребезга.
void buttons_init();
void buttons_poll();

// Текущее удерживаемое значение.
Button buttons_current();

// Фронт нажатия: true ровно один цикл после перехода в нажатое состояние.
bool buttons_pressed(Button b);

// События одинарного и двойного нажатия. Возвращают true ровно один раз
// (сбрасываются при вызове). Срабатывают по факту подтверждения: одиночное —
// через BUTTON_DOUBLE_CLICK_MS после первого нажатия, двойное — при втором
// нажатии в пределах окна.
bool buttons_single_clicked(Button b);
bool buttons_double_clicked(Button b);

// Сбросить накопленные события кликов (если кнопка не использовалась).
void buttons_reset_clicks();

#endif  // BUTTONS_H
