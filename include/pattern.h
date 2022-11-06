#ifndef PATTERN_H
#define PATTERN_H

#include "esp_system.h"
#include "esp_log.h"
#include "led_strip.h"

struct config_pattern {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint16_t hue;
    uint16_t start_rgb;
} RainBow_Config;

struct color {
    uint32_t R;
    uint32_t G;
    uint32_t B;
} color_data;

void int_rainbow();
void show_rainbow_pattern(led_strip_t *);
void show_static_pattern(led_strip_t *);
void led_strip_hsv2rgb(uint32_t, uint32_t, uint32_t, uint32_t *, uint32_t *, uint32_t *);

#endif /*PATTERN_H*/