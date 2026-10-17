#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

void draw_pixel(int x, int y, uint16_t color);

void clear_screen(uint16_t color);

void draw_text(int x, int y, const char *text, uint16_t color, int scale);

void config_spi();

void desenhar_tela();