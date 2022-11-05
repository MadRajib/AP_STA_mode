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
#include <esp_http_server.h>
#include "esp_http_client.h"
#include "mqtt_client.h"
#include "app_config.h"

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

// #define DEBUG_BUILD 1

static const char *AP_TAG = "AP";
static const char *STA_TAG = "STA";
static const char *TAG = "SR";

enum { SP_READ, SP_WRITE };
enum { WIFI_AP, WIFI_STA };

struct sta_data_1 wifi_settings;
struct sta_data wifi_config_txt;
cJSON *wifi_config_json;

// static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
bool is_sta_connected = false;

struct file_server_data {
    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};
esp_mqtt_client_handle_t client = NULL;

void publish_message(char *topic, char * msg){
    int msg_id = esp_mqtt_client_publish(client, topic, msg, 0, 1, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
}

int update_config();

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
        .uri = wifi_config_txt.mqtt,
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

            strcpy(&wifi_config.sta.ssid,wifi_config_txt.ssid);
            strcpy(&wifi_config.sta.password,wifi_config_txt.psswd);

            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            wifi_config.sta.pmf_cfg.capable = true;
            wifi_config.sta.pmf_cfg.required = false;

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            break;
    }
    
    ESP_ERROR_CHECK(esp_wifi_start());

}

esp_err_t post_handler(httpd_req_t *req){

    char buf[128];
    size_t recv_size = MIN(req->content_len, sizeof(buf));

    int ret = httpd_req_recv(req, buf, recv_size);
    if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        httpd_resp_send_408(req);

    cJSON *json = cJSON_Parse(buf);
    if (!json){
        ESP_LOGE(TAG, "Failed: %s", cJSON_GetErrorPtr());
        return ESP_FAIL;
    }

    memset(&wifi_config_txt, 0, sizeof wifi_config_txt);
    size_t len = strlen(cJSON_GetObjectItem(json, "ssid")->valuestring);
    strncpy(
        wifi_config_txt.ssid,
        cJSON_GetObjectItem(json, "ssid")->valuestring,
        len
    );

    len = strlen(cJSON_GetObjectItem(json, "psswd")->valuestring);
    strncpy(
        wifi_config_txt.psswd,
        cJSON_GetObjectItem(json, "psswd")->valuestring,
        len
    );

    len = strlen(cJSON_GetObjectItem(json, "mqtt")->valuestring);
    strncpy(
        wifi_config_txt.mqtt,
        cJSON_GetObjectItem(json, "mqtt")->valuestring,
        len
    );
    
    const char resp[] = "URI POST Response";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    cJSON_Delete(json);

    update_config();

    return ESP_OK;
}

/* URI handler structure for POST /uri */
httpd_uri_t uri_post = {
    .uri = "/config",
    .method = HTTP_POST,
    .handler = post_handler,
    .user_ctx = NULL};

static httpd_handle_t start_webserver(void){
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK){
        httpd_register_uri_handler(server, &uri_post);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server){
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        // ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        // ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        // ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
        // printf("%.*s", evt->data_len, (char *)evt->data);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        printf("%.*s", evt->data_len, (char *)evt->data);
        break;
    case HTTP_EVENT_ON_FINISH:
        // ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        // ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }

    return ESP_OK;
}

int init_config(){
    return read_config(&wifi_config_txt);
}

int update_config(){
   return write_config(&wifi_config_txt);
}

void init(){

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

void app_main(void)
{
    int ret = 0;

    init();

    ret = init_config();
    if(ret < 0) 
        return;

    ESP_LOGI(TAG, ":%s\n", wifi_config_txt.ssid);
    ESP_LOGI(TAG, ":%s\n", wifi_config_txt.psswd);
    ESP_LOGI(TAG, ":%s\n", wifi_config_txt.mqtt);

    // //Check the status of mode pin
    gpio_set_direction(MODE_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(MODE_PIN, GPIO_PULLUP_ONLY);

    ESP_LOGI(AP_TAG, "LEVEL %d",gpio_get_level(13));

    if (!gpio_get_level(13))
    {
        ESP_LOGI(AP_TAG, "ESP_WIFI_MODE_AP");
        wifi_init(WIFI_AP);
        static httpd_handle_t server = NULL;
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
        server = start_webserver();

        // char *ptr = cJSON_GetObjectItem(wifi_config_json, "ssid")->valuestring;
        // char *buff = "Hello 1";
        // long len = strlen(buff) + 1;
        // ptr = realloc(ptr, len);
        // memcpy(ptr, buff, len);
       
        // ret = update_spiff();
        // if(ret < 0)
        //     return;
    }
    else
    {
        // ESP_LOGI(STA_TAG, "ESP_WIFI_MODE_STA");
        // wifi_init(WIFI_STA);
        // mqtt_app_start();
    }
}