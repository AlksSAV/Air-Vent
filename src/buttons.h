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

#endif  // BUTTONS_H
