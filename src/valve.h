#ifndef VALVE_H
#define VALVE_H

#include <Arduino.h>

// Привод заслонки приточного канала — два реле: открытие и закрытие. Внутри
// привода концевики, которые сами размыкают цепь в крайних положениях, поэтому
// удерживать реле включённым не нужно, а обратная связь контроллеру не
// требуется. Модуль необязательный — его наличие задаётся настройкой, и без
// неё контроллер работает ровно как раньше.
void valve_init();

// Есть ли привод в сборке по текущей настройке.
bool valve_present();

// Положение захлопки. Обратной связи у привода нет, поэтому состояние
// выводится из поданного импульса и его длительности: концевик внутри
// привода сам разомкнёт цепь в крайнем положении.
//
// Когда привода в сборке нет, положение всегда VALVE_OPEN: заслонка
// отсутствует, канал открыт, и вся логика деградирует ровно в прежнюю.
enum ValvePos : uint8_t {
  VALVE_POS_MOVING = 0,  // идёт импульс, захлопка в пути
  VALVE_POS_CLOSED,
  VALVE_POS_OPEN,
};

ValvePos valve_position();

// Привести заслонку к заведомо закрытому положению. Вызывается один раз при
// загрузке, если привод есть: так как у мотора нет пружины возврата,
// контроллер не знает, в каком положении он оказался после отключения питания.
void valve_home();

// Forgets the derived position: the next valve_open()/valve_close() runs a
// full stroke again and valve_position() reports MOVING until it finishes.
// Required whenever the assumption "the last pulse ended in a limit switch"
// no longer holds, i.e. when the valve module is switched in the menu.
void valve_invalidate_position();

// Открыть заслонку (импульс на реле, концевик сам разомкнёт цепь).
void valve_open();

// Закрыть заслонку (аналогично).
void valve_close();

// Снимает импульс по истечении хода. Вызывать в каждом проходе loop().
void valve_update();

#endif  // VALVE_H
