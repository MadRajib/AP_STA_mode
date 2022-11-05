#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "mqtt_client.h"
#include "common.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// AP Configs macros
#define AP_SSID CONFIG_AP_SSID
#define AP_PASS CONFIG_AP_PASSWORD
#define AP_CHANNEL CONFIG_AP_CHANNEL
#define AP_MAX_STA_CONN CONFIG_AP_MAX_STA_CONN

// STA config macros
#define STA_SSID CONFIG_STA_SSID
#define STA_PASS CONFIG_STA_PASSWORD
#define STA_MAXIMUM_RETRY CONFIG_STA_MAXIMUM_RETRY

#define MODE_PIN 13
#define SCRATCH_BUFSIZE  8192

void init_spiff(int, char *, const char *, const char *);

static const char *AP_TAG = "AP";
static const char *STA_TAG = "STA";
static const char *TAG = "SR";

enum { SP_READ, SP_WRITE };
enum { WIFI_AP, WIFI_STA };

// static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
bool is_sta_connected = false;

struct sta_data
{
    char *ssid;
    char *psswd;
    char *url; // mqtt server address
} wifi_settings;

esp_mqtt_client_handle_t client = NULL;

void publish_message(char *topic, char * msg){
    int msg_id = esp_mqtt_client_publish(client, topic, msg, 0, 1, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
 
    case MQTT_EVENT_SUBSCRIBED:
        // ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = wifi_settings.url,
        .refresh_connection_after_ms=6000,
        .disable_auto_reconnect=true,
    };
    if(!mqtt_cfg.uri)
        ESP_LOGI(TAG, "uri -> %s", mqtt_cfg.uri);

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}


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
        mqtt_app_start();
    }
}


void wifi_init(const int mode) {

    if(mode == WIFI_AP){
        esp_netif_create_default_wifi_ap();
    }else {
        esp_netif_create_default_wifi_sta();
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

            strcpy(&wifi_config.sta.ssid,wifi_settings.ssid);
            strcpy(&wifi_config.sta.password,wifi_settings.psswd);

            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            wifi_config.sta.pmf_cfg.capable = true;
            wifi_config.sta.pmf_cfg.required = false;

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            break;
    }
    
    ESP_ERROR_CHECK(esp_wifi_start());

}

void init_spiff(int mode, char *ssid, const char *psswd, const char *url)
{
    char *TAG = "SPIFF";
    char *data = NULL;
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    

    // Open renamed file for reading

    ESP_LOGI(TAG, "Reading file");

    FILE *f = fopen("/spiffs/config.json", "rb");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        goto EXIT;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    data = (char *)malloc(len + 1);
    if (!data)
    {
        ESP_LOGE(TAG, "Failed to alloc mem: %d", errno);
        fclose(f);
        goto EXIT;
    }

    fread(data, 1, len, f);
    fclose(f);

    data[len] = '\0';

    // parse the data
    cJSON *json = cJSON_Parse(data);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed: %s", cJSON_GetErrorPtr());
        goto EXIT;
    }

    char *s = cJSON_GetObjectItem(json, "ssid")->valuestring;
    char *p = cJSON_GetObjectItem(json, "psswd")->valuestring;
    char *u = cJSON_GetObjectItem(json, "url")->valuestring;

    if (mode == SP_WRITE)
    {
        if (!ssid || !psswd || !url)
        {
            cJSON_Delete(json);
            goto EXIT;
        }

        s = realloc(s, strlen(ssid));
        memcpy(s, ssid, strlen(ssid) + 1);

        p = realloc(p, strlen(psswd));
        memcpy(p, psswd, strlen(psswd) + 1);

        u = realloc(u, strlen(url));
        memcpy(u, url, strlen(url) + 1);

        ESP_LOGI(TAG, "Writing to file");
        FILE *wf = fopen("/spiffs/config.json", "wb");
        if (wf == NULL)
        {
            ESP_LOGE(TAG, "Failed to open file for writing");
            goto EXIT;
        }

        if (!json)
        {
            ESP_LOGE(TAG, "Failed json\n");
            goto EXIT;
        }

        char *out = cJSON_Print(json);
        if (!out)
        {
            ESP_LOGE(TAG, "Failed tout\n");
            goto EXIT;
        }
        printf("data: %s\n", out);
        fprintf(wf, "%s", out);

        fclose(wf);
        free(out);
        out = NULL;
    }

    // CONFIGURE THE ESP32 WIFI CONFIGS
    wifi_settings.ssid = s;
    wifi_settings.psswd = p;
    wifi_settings.url = u;

    // cJSON_Delete(json);

EXIT:
    free(data);
    data = NULL;
    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");
}


void init()
{   
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

void app_main(void)
{

    init();

    init_spiff(SP_READ, NULL, NULL, NULL);
    ESP_LOGI(STA_TAG, "%s", wifi_settings.ssid);
    ESP_LOGI(STA_TAG, "%s", wifi_settings.psswd);
    ESP_LOGI(STA_TAG, "%s", wifi_settings.url);

    // //Check the status of mode pin
    gpio_set_direction(MODE_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(MODE_PIN, GPIO_PULLUP_ONLY);

    // ESP_LOGI(AP_TAG, "LEVEL %d",gpio_get_level(13));

    if (!gpio_get_level(13))
    {
        ESP_LOGI(AP_TAG, "ESP_WIFI_MODE_AP");
        wifi_init(WIFI_AP);
    }
    else{
        ESP_LOGI(STA_TAG, "ESP_WIFI_MODE_STA");
        wifi_init(WIFI_STA);
    }

    // mqtt_app_start();
}