#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_event.h"

// AP Configs macros
#define AP_SSID CONFIG_AP_SSID
#define AP_PASS CONFIG_AP_PASSWORD
#define AP_CHANNEL CONFIG_AP_CHANNEL
#define AP_MAX_STA_CONN CONFIG_AP_MAX_STA_CONN

// STA config macros
#define STA_SSID CONFIG_STA_SSID
#define STA_PASS CONFIG_STA_PASSWORD
#define STA_MAXIMUM_RETRY CONFIG_STA_MAXIMUM_RETRY

static const char *AP_TAG = "AP";
static const char *STA_TAG = "STA";
static const char *TAG = "SR";

enum { WIFI_AP, WIFI_STA };

void wifi_init(const int);