#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define SSD1306_WIDTH  72
#define SSD1306_HEIGHT 40

esp_err_t ssd1306_init(int sda_pin, int scl_pin);
void ssd1306_clear();
void ssd1306_draw_pixel(int x, int y, bool on);
void ssd1306_draw_xbm(int x, int y, int w, int h, const uint8_t *bitmap);
void ssd1306_refresh();
void ssd1306_display_on();
void ssd1306_display_off();

#endif
