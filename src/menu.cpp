#include "menu.h"
#include "buttons.h"
#include "config.h"
#include "display.h"
#include "fan_controller.h"
#include "settings.h"

// Две страницы: главная с тремя разделами и разделы с параметрами. Глубину
// больше не делаем: на 16 символах без индикатора пути третий уровень
// запутал бы сильнее, чем помог.
enum MenuPage : uint8_t {
  PAGE_ROOT = 0,
  PAGE_TEMP,
  PAGE_VALVE,
  PAGE_MISC,
};

static MenuPage s_page = PAGE_ROOT;
static uint8_t s_cursor = 0;
static unsigned long s_idle_since_ms = 0;
static bool s_dirty = true;

// Подтверждение сброса: наработка обнуляется мгновенно, и без сообщения
// оператор не отличит сработавшее действие от промаха по клавише.
static unsigned long s_reset_flash_ms = 0;
static bool s_reset_flash = false;

static void redraw() { s_dirty = true; }

static uint8_t item_count(MenuPage page) {
  switch (page) {
    case PAGE_TEMP: return 2;   // Tmin, Tmax
    case PAGE_VALVE: return 2;  // VALVE, PULSE
    case PAGE_MISC: return 2;   // RESET RT, версия
    default: return 3;          // TEMP, VALVE, MISC
  }
}

static void go_back() {
  if (s_page == PAGE_ROOT) return;
  s_page = PAGE_ROOT;
  s_cursor = 0;
  redraw();
}

void menu_open() {
  s_page = PAGE_ROOT;
  s_cursor = 0;
  s_idle_since_ms = millis();
  // Нажатие, которым меню открыли, не должно сразу сработать внутри.
  buttons_reset_clicks();
  redraw();
}

bool menu_update() {
  unsigned long now = millis();

  if (buttons_pressed(BTN_UP) || buttons_pressed(BTN_DOWN)) {
    uint8_t count = item_count(s_page);
    int8_t step = buttons_pressed(BTN_DOWN) ? 1 : -1;
    s_cursor = (uint8_t)((s_cursor + count + step) % count);
    s_idle_since_ms = now;
    redraw();
  }

  if (buttons_pressed(BTN_LEFT) || buttons_pressed(BTN_RIGHT)) {
    int8_t step = buttons_pressed(BTN_RIGHT) ? 1 : -1;
    bool changed = false;
    if (s_page == PAGE_TEMP) {
      changed = (s_cursor == 0) ? settings_adjust_t_min(step)
                                : settings_adjust_t_max(step);
    } else if (s_page == PAGE_VALVE) {
      // Привод — переключатель, шаг у него всегда один.
      changed = (s_cursor == 0) ? settings_toggle_valve()
                                : settings_adjust_valve_pulse(step);
    }
    if (changed) {
      s_idle_since_ms = now;
      redraw();
    }
  }

  // Сброс наработки — удержанием Right на своём пункте. Короткое нажатие
  // ничего не делает: действие необратимое, случайно его не запустить.
  // Удержание Select занято выходом из меню, поэтому сброс вынесен на Right.
  if (s_page == PAGE_MISC && s_cursor == 0 &&
      buttons_held(BTN_RIGHT, BUTTON_HOLD_MS)) {
    fan_runtime_reset();
    s_idle_since_ms = now;
    s_reset_flash = true;
    s_reset_flash_ms = now;
    redraw();
  }

  // Удержание Select — назад: на главной это выход из меню, в разделе —
  // на уровень выше.
  if (buttons_held(BTN_SELECT, BUTTON_HOLD_MS)) {
    if (s_page == PAGE_ROOT) return false;
    go_back();
    return true;
  }

  if (buttons_pressed(BTN_SELECT)) {
    s_idle_since_ms = now;
    if (s_page == PAGE_ROOT) {
      s_page = (MenuPage)(PAGE_TEMP + s_cursor);
      s_cursor = 0;
      redraw();
    } else {
      // Внутри раздела Select выходит на уровень выше: отдельной кнопки
      // «назад» на щите нет, а удержание нужно беречь для сброса.
      go_back();
    }
  }

  if ((now - s_idle_since_ms) >= EDIT_TIMEOUT_MS) {
    settings_commit();
    return false;
  }

  return true;
}

void menu_draw(LiquidCrystal& lcd) {
  // Подсветка не должна гаснуть, пока открыто меню: вызывается каждый цикл,
  // а не только при перерисовке.
  display_keep_awake();

  if (!s_dirty) return;
  s_dirty = false;

  char buf[20];
  const char* mark = ">";

  switch (s_page) {
    case PAGE_TEMP:
      snprintf(buf, sizeof(buf), "%sTmin %+3d C",
               s_cursor == 0 ? mark : " ", settings_get().t_min);
      display_text(lcd, 0, buf);
      snprintf(buf, sizeof(buf), "%sTmax %+3d C",
               s_cursor == 1 ? mark : " ", settings_get().t_max);
      display_text(lcd, 1, buf);
      break;

    case PAGE_VALVE: {
      Settings st = settings_get();
      snprintf(buf, sizeof(buf), "%sVALVE %s", s_cursor == 0 ? mark : " ",
               st.valve_enabled ? "ON" : "OFF");
      display_text(lcd, 0, buf);
      snprintf(buf, sizeof(buf), "%sPULSE %2lu s", s_cursor == 1 ? mark : " ",
               (unsigned long)st.valve_pulse_s);
      display_text(lcd, 1, buf);
      break;
    }

    case PAGE_MISC:
      // Подтверждение держится две секунды: столько нужно, чтобы его
      // заметить, и меньше, чем пауза перед следующим нажатием.
      if (s_reset_flash) {
        if ((millis() - s_reset_flash_ms) >= 2000UL) {
          s_reset_flash = false;
          redraw();
        } else {
          display_text(lcd, 0, " RUNTIME CLEARED");
          snprintf(buf, sizeof(buf), "%sv%s", s_cursor == 1 ? mark : " ", VERSION);
          display_text(lcd, 1, buf);
          break;
        }
      }
      // Сброс запускается удержанием Right — подпись об этом стоит рядом,
      // иначе действие неоткуда угадать.
      snprintf(buf, sizeof(buf), "%sRESET RT hold>", s_cursor == 0 ? mark : " ");
      display_text(lcd, 0, buf);
      snprintf(buf, sizeof(buf), "%sv%s", s_cursor == 1 ? mark : " ", VERSION);
      display_text(lcd, 1, buf);
      break;

    default: {
      // Три раздела на двух строках, поэтому окно сдвигается за курсором и
      // список «прыгает». Счётчик в правом нижнем углу показывает, что
      // список прокрутился, а не заменился целиком.
      static const char* const kRoot[] = {"TEMPERATURE", "VALVE", "MISC"};
      uint8_t top = s_cursor < 1 ? 0 : 1;
      for (uint8_t row = 0; row < 2; ++row) {
        uint8_t idx = top + row;
        const char* mark_here = idx == s_cursor ? mark : " ";
        if (row == 0) {
          snprintf(buf, sizeof(buf), "%s%s", mark_here, kRoot[idx]);
        } else {
          snprintf(buf, sizeof(buf), "%s%-11s%u/3", mark_here, kRoot[idx],
                   (unsigned)s_cursor + 1);
        }
        display_text(lcd, row, buf);
      }
      break;
    }
  }
}
