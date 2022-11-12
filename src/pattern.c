#include "pattern.h"

#include "driver/rmt.h"
#include "esp_log.h"

#define EXAMPLE_CHASE_SPEED_MS (10)

void int_rainbow() {
    RainBow_Config.red = 0;
    RainBow_Config.green = 0;
    RainBow_Config.blue = 0;
    RainBow_Config.hue = 0;
    RainBow_Config.start_rgb = 0;
}

void show_rainbow_pattern(led_strip_t *strip){
    for (int i = 0; i < 3; i++) {
            for (int j = i; j < CONFIG_EXAMPLE_STRIP_LED_NUMBER; j += 3) {
                // Build RGB values
                RainBow_Config.hue = j * 360 / CONFIG_EXAMPLE_STRIP_LED_NUMBER + RainBow_Config.start_rgb;
                led_strip_hsv2rgb(RainBow_Config.hue, 100, 100, &RainBow_Config.red, &RainBow_Config.green, &RainBow_Config.blue);
                // Write RGB values to strip driver
                ESP_ERROR_CHECK(strip->set_pixel(strip, j, RainBow_Config.red, RainBow_Config.green, RainBow_Config.blue));
            }
            // Flush RGB values to LEDs
            ESP_ERROR_CHECK(strip->refresh(strip, 100));
            vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
            strip->clear(strip, 50);
            vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
        }
    RainBow_Config.start_rgb += 60;
}


void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

void show_static_pattern(led_strip_t *strip){

    for (int j = 0; j < CONFIG_EXAMPLE_STRIP_LED_NUMBER; j++) {        
        // Write RGB values to strip driver
        ESP_ERROR_CHECK(strip->set_pixel(strip, j, color_data.R, color_data.G, color_data.B));
    }

    // Flush RGB values to LEDs
    ESP_ERROR_CHECK(strip->refresh(strip, 50));
    vTaskDelay(pdMS_TO_TICKS(200));
}
