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


// ============================================================
// TFT
// ============================================================

#define TFT_SCLK    21
#define TFT_MOSI    47
#define TFT_RST     45
#define TFT_DC      40
#define TFT_CS      41
#define TFT_BL      42

#define TFT_WIDTH   240
#define TFT_HEIGHT  240

#define TFT_SPI_HOST SPI2_HOST

typedef struct
{
    char character;
    uint8_t rows[7];
} FontCharacter;

uint16_t *framebuffer;
esp_lcd_panel_handle_t panel_handle;


static const FontCharacter font_5x7[] =
{
    // Espaço e Pontuação Básica
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},
    {',', {0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {'=', {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}},
    {'/', {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00}},
    {'?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},

    // Números (0-9)
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}},

    // Letras Maiúsculas (A-Z)
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x0F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0F}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x13, 0x11, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
};

void draw_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= TFT_WIDTH)
        return;

    if (y < 0 || y >= TFT_HEIGHT)
        return;

    framebuffer[y * TFT_WIDTH + x] = color;
}

void clear_screen(uint16_t color)
{
    for (int i = 0;
         i < TFT_WIDTH * TFT_HEIGHT;
         i++)
    {
        framebuffer[i] = color;
    }
}

static void draw_char(int x, int y, char character, uint16_t color, int scale)
{
    if (character >= 'a' && character <= 'z')
    {
        character -= 'a' - 'A';
    }

    const uint8_t *rows = font_5x7[0].rows;
    const size_t font_size = sizeof(font_5x7) / sizeof(font_5x7[0]);

    for (size_t i = 0; i < font_size; i++)
    {
        if (font_5x7[i].character == character)
        {
            rows = font_5x7[i].rows;
            break;
        }
    }

    for (int row = 0; row < 7; row++)
    {
        for (int col = 0; col < 5; col++)
        {
            if (rows[row] & (1U << (4 - col)))
            {
                for (int dy = 0; dy < scale; dy++)
                {
                    for (int dx = 0; dx < scale; dx++)
                    {
                        draw_pixel(x + col * scale + dx,
                                   y + row * scale + dy,
                                   color);
                    }
                }
            }
        }
    }
}

void draw_text(int x, int y, const char *text, uint16_t color, int scale)
{
    while (*text)
    {
        draw_char(x, y, *text, color, scale);
        x += 6 * scale;
        text++;
    }
}

void config_spi(){
    gpio_config_t bl_config =
    {
        .pin_bit_mask = (1ULL << TFT_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&bl_config));
    gpio_set_level(TFT_BL, 1);

    spi_bus_config_t bus_config =
    {
        .sclk_io_num = TFT_SCLK,
        .mosi_io_num = TFT_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * 2
    };

    ESP_ERROR_CHECK(spi_bus_initialize(TFT_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_io_spi_config_t io_config =
    {
        .cs_gpio_num = TFT_CS,
        .dc_gpio_num = TFT_DC,
        .spi_mode = 0,
        .pclk_hz = 20 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8
    };

    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_spi(
            (esp_lcd_spi_bus_handle_t)TFT_SPI_HOST,
            &io_config,
            &io_handle
        )
    );

    
    esp_lcd_panel_dev_config_t panel_config =
    {
        .reset_gpio_num = TFT_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16
    };

    ESP_ERROR_CHECK(
        esp_lcd_new_panel_st7789(
            io_handle,
            &panel_config,
            &panel_handle
        )
    );

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    framebuffer = heap_caps_malloc(TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);

    if (!framebuffer)
    {
        ESP_LOGE("TEXT", "Falha ao alocar framebuffer");
        return;
    }
}

void desenhar_tela(){
        ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap(
            panel_handle,
            0,
            0,
            TFT_WIDTH,
            TFT_HEIGHT,
            framebuffer
        )
    );
}
