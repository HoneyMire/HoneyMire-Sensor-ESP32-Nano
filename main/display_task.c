#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "icons.h"
#include "display_task.h"

#define DISPLAY_ON_TIME_MS 30000
#define ATTACK_ICON_TIME_MS 30000

typedef enum {
    DISPLAY_CMD_BOOT,
    DISPLAY_CMD_ATTACK,
    DISPLAY_CMD_WAKE,
} display_cmd_t;

static QueueHandle_t display_queue;
static int64_t off_time_us = 0;
static bool is_on = false;
static const char *TAG = "display_task";

void display_show_boot_logo() {
    display_cmd_t cmd = DISPLAY_CMD_BOOT;
    xQueueSend(display_queue, &cmd, 0);
}

void display_show_attack() {
    display_cmd_t cmd = DISPLAY_CMD_ATTACK;
    xQueueSend(display_queue, &cmd, 0);
}

void display_wake() {
    display_cmd_t cmd = DISPLAY_CMD_WAKE;
    xQueueSend(display_queue, &cmd, 0);
}

static void display_task(void *pvParameters) {
    display_cmd_t cmd;
    ssd1306_init(5, 6); // SDA: 5, SCL: 6
    
    while (1) {
        int64_t now = esp_timer_get_time();
        int64_t wait_ms = 1000;
        
        if (is_on) {
            int64_t remaining_ms = (off_time_us - now) / 1000;
            if (remaining_ms <= 0) {
                ssd1306_display_off();
                is_on = false;
                wait_ms = portMAX_DELAY;
            } else {
                wait_ms = remaining_ms;
            }
        } else {
            wait_ms = portMAX_DELAY;
        }

        if (xQueueReceive(display_queue, &cmd, pdMS_TO_TICKS(wait_ms)) == pdTRUE) {
            switch (cmd) {
                case DISPLAY_CMD_BOOT:
                    ssd1306_display_on();
                    ssd1306_clear();
                    ssd1306_draw_xbm((SSD1306_WIDTH - BOOT_LOGO_W) / 2, 
                                     (SSD1306_HEIGHT - BOOT_LOGO_H) / 2, 
                                     BOOT_LOGO_W, BOOT_LOGO_H, BOOT_LOGO);
                    ssd1306_refresh();
                    off_time_us = esp_timer_get_time() + (int64_t)DISPLAY_ON_TIME_MS * 1000;
                    is_on = true;
                    break;
                case DISPLAY_CMD_ATTACK:
                    ssd1306_display_on();
                    ssd1306_clear();
                    ssd1306_draw_xbm((SSD1306_WIDTH - TELNET_ICON_W) / 2, 
                                     (SSD1306_HEIGHT - TELNET_ICON_H) / 2, 
                                     TELNET_ICON_W, TELNET_ICON_H, TELNET_ICON);
                    ssd1306_refresh();
                    off_time_us = esp_timer_get_time() + (int64_t)ATTACK_ICON_TIME_MS * 1000;
                    is_on = true;
                    break;
                case DISPLAY_CMD_WAKE:
                    if (!is_on) {
                        ssd1306_display_on();
                        is_on = true;
                    }
                    off_time_us = esp_timer_get_time() + (int64_t)DISPLAY_ON_TIME_MS * 1000;
                    break;
            }
        }
    }
}

void display_task_start() {
    display_queue = xQueueCreate(10, sizeof(display_cmd_t));
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
}
