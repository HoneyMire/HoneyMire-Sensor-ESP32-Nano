#include <string.h>
#include "ssd1306.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define SSD1306_I2C_ADDR 0x3C

// SSD1306 commands
#define SSD1306_CMD_DISPLAY_OFF 0xAE
#define SSD1306_CMD_DISPLAY_ON  0xAF
#define SSD1306_CMD_SET_DISPLAY_CLOCK_DIV 0xD5
#define SSD1306_CMD_SET_MULTIPLEX 0xA8
#define SSD1306_CMD_SET_DISPLAY_OFFSET 0xD3
#define SSD1306_CMD_SET_START_LINE 0x40
#define SSD1306_CMD_CHARGE_PUMP 0x8D
#define SSD1306_CMD_MEMORY_MODE 0x20
#define SSD1306_CMD_SET_COL_ADDR 0x21
#define SSD1306_CMD_SET_PAGE_ADDR 0x22
#define SSD1306_CMD_COM_SCAN_INC 0xC0
#define SSD1306_CMD_COM_SCAN_DEC 0xC8
#define SSD1306_CMD_SET_SEG_REMAP 0xA0
#define SSD1306_CMD_SET_CONTRAST 0x81
#define SSD1306_CMD_SET_PRECHARGE 0xD9
#define SSD1306_CMD_SET_VCOM_DETECT 0xDB
#define SSD1306_CMD_DISPLAY_ALL_ON_RESUME 0xA4
#define SSD1306_CMD_NORMAL_DISPLAY 0xA6

static uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
static const char *TAG = "ssd1306";

static esp_err_t ssd1306_write_cmd(uint8_t cmd) {
    i2c_cmd_handle_t link = i2c_cmd_link_create();
    i2c_master_start(link);
    i2c_master_write_byte(link, (SSD1306_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(link, 0x00, true); // Control byte: Command
    i2c_master_write_byte(link, cmd, true);
    i2c_master_stop(link);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, link, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(link);
    return ret;
}

esp_err_t ssd1306_init(int sda_pin, int scl_pin) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_pin,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    ssd1306_write_cmd(SSD1306_CMD_DISPLAY_OFF);
    ssd1306_write_cmd(SSD1306_CMD_SET_DISPLAY_CLOCK_DIV);
    ssd1306_write_cmd(0x80);
    ssd1306_write_cmd(SSD1306_CMD_SET_MULTIPLEX);
    ssd1306_write_cmd(SSD1306_HEIGHT - 1);
    ssd1306_write_cmd(SSD1306_CMD_SET_DISPLAY_OFFSET);
    ssd1306_write_cmd(0x00);
    ssd1306_write_cmd(SSD1306_CMD_SET_START_LINE | 0x00);
    ssd1306_write_cmd(SSD1306_CMD_CHARGE_PUMP);
    ssd1306_write_cmd(0x14);
    ssd1306_write_cmd(SSD1306_CMD_MEMORY_MODE);
    ssd1306_write_cmd(0x00); // Horizontal addressing mode
    ssd1306_write_cmd(SSD1306_CMD_SET_SEG_REMAP | 0x01);
    ssd1306_write_cmd(SSD1306_CMD_COM_SCAN_DEC);
    
    // For 72x40, we might need specific COM pins config
    ssd1306_write_cmd(0xDA); // SETCOMPINS
    ssd1306_write_cmd(0x12);
    
    ssd1306_write_cmd(SSD1306_CMD_SET_CONTRAST);
    ssd1306_write_cmd(0xCF);
    ssd1306_write_cmd(SSD1306_CMD_SET_PRECHARGE);
    ssd1306_write_cmd(0xF1);
    ssd1306_write_cmd(SSD1306_CMD_SET_VCOM_DETECT);
    ssd1306_write_cmd(0x40);
    ssd1306_write_cmd(SSD1306_CMD_DISPLAY_ALL_ON_RESUME);
    ssd1306_write_cmd(SSD1306_CMD_NORMAL_DISPLAY);
    ssd1306_write_cmd(SSD1306_CMD_DISPLAY_ON);

    ssd1306_clear();
    ssd1306_refresh();
    
    return ESP_OK;
}

void ssd1306_clear() {
    memset(buffer, 0, sizeof(buffer));
}

void ssd1306_draw_pixel(int x, int y, bool on) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) return;
    if (on) buffer[x + (y / 8) * SSD1306_WIDTH] |= (1 << (y % 8));
    else    buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
}

void ssd1306_draw_xbm(int x, int y, int w, int h, const uint8_t *bitmap) {
    int byte_width = (w + 7) / 8;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (bitmap[j * byte_width + i / 8] & (1 << (i % 8))) {
                ssd1306_draw_pixel(x + i, y + j, true);
            }
        }
    }
}

void ssd1306_refresh() {
    ssd1306_write_cmd(SSD1306_CMD_SET_COL_ADDR);
    ssd1306_write_cmd(28); // Column start offset for 72x40 EastRising panel
    ssd1306_write_cmd(28 + SSD1306_WIDTH - 1); // Column end
    ssd1306_write_cmd(SSD1306_CMD_SET_PAGE_ADDR);
    ssd1306_write_cmd(0); // Page start
    ssd1306_write_cmd((SSD1306_HEIGHT / 8) - 1); // Page end

    i2c_cmd_handle_t link = i2c_cmd_link_create();
    i2c_master_start(link);
    i2c_master_write_byte(link, (SSD1306_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(link, 0x40, true); // Control byte: Data
    i2c_master_write(link, buffer, sizeof(buffer), true);
    i2c_master_stop(link);
    i2c_master_cmd_begin(I2C_MASTER_NUM, link, 50 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(link);
}

void ssd1306_display_on() {
    ssd1306_write_cmd(SSD1306_CMD_DISPLAY_ON);
}

void ssd1306_display_off() {
    ssd1306_write_cmd(SSD1306_CMD_DISPLAY_OFF);
}
