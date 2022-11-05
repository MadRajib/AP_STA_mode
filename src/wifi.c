#include "wifi.h"
#include "string.h"
#include "app_config.h"

static int s_retry_num = 0;
bool is_sta_connected = false;
extern struct sta_data wifi_config_txt;

/* Wifi handles*/
// AP event handler
static void ap_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(AP_TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(AP_TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

// STA event handler
static void sta_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        is_sta_connected = false;
        if (s_retry_num < 20)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(STA_TAG, "retry to connect to the AP");
        }

        ESP_LOGI(STA_TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(STA_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        is_sta_connected = true;
    }
}

void wifi_init(const int mode) {

    if(mode == WIFI_AP){
        esp_netif_create_default_wifi_ap();
    }else {
        esp_netif_t *sta = esp_netif_create_default_wifi_sta();
        esp_netif_dhcpc_stop(sta);
        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, 192, 168, 1, 22);
        IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        esp_netif_set_ip_info(sta, &ip_info);

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    }



    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    
    switch(mode) {
        case WIFI_AP:
            
            ESP_ERROR_CHECK(esp_event_handler_instance_register(
                WIFI_EVENT,ESP_EVENT_ANY_ID,&ap_event_handler,NULL,NULL));

            strcpy(&wifi_config.ap.ssid, AP_SSID);
            wifi_config.ap.ssid_len = strlen(AP_SSID);
            wifi_config.ap.channel = AP_CHANNEL;
            // strcpy(&wifi_config.ap.password, AP_PASS);
            wifi_config.ap.max_connection = AP_MAX_STA_CONN;
            // wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
            
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));          
            break;
        case WIFI_STA:
            ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &sta_event_handler, NULL));
            ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &sta_event_handler, NULL));

            strcpy(&wifi_config.sta.ssid, wifi_config_txt.ssid);
            strcpy(&wifi_config.sta.password,wifi_config_txt.psswd);

            ESP_LOGI(TAG, "Sssid %s", wifi_config.sta.ssid);
            ESP_LOGI(TAG, "password %s", wifi_config.sta.password);

            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            wifi_config.sta.pmf_cfg.capable = true;
            wifi_config.sta.pmf_cfg.required = false;


            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            break;
    }
    
    ESP_ERROR_CHECK(esp_wifi_start());

}
