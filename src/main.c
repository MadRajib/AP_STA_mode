#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
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
#include "esp_sleep.h"
#include "app_config.h"
#include "wifi.h"
#include "driver/rmt.h"
#include "led_strip.h"
#include "pattern.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MODE_PIN 4
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define EXAMPLE_CHASE_SPEED_MS (50)

int update_config();

led_strip_t *strip = NULL;
struct sta_data wifi_config_txt;

enum PATTRERNS{RAINBOW, STATIC, CLEAR, NONE};
int show_pattern = NONE;

TaskHandle_t Task2;
void renderLedTask( void * pvParameters ){
    while(1) {
        switch(show_pattern){
            case RAINBOW:
                show_rainbow_pattern(strip);
                break;
            case CLEAR:
                ESP_ERROR_CHECK(strip->clear(strip, 100));
                break;
            case STATIC:
                show_static_pattern(strip);
                show_pattern = NONE;
                break;
            default:
                vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/* API server */

/* Endpoint handlers*/
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
    
    update_config();

    const char resp[] = "Restarting Device!";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    cJSON_Delete(json);
    esp_restart();
    return ESP_OK;
}


esp_err_t echo_handler(httpd_req_t *req){
    
    const char resp[] = "ECHO";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t led_post_handler(httpd_req_t *req){

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

    long red = atol(cJSON_GetObjectItem(json, "R")->valuestring);
    long green = atol(cJSON_GetObjectItem(json, "G")->valuestring);
    long blue = atol(cJSON_GetObjectItem(json, "B")->valuestring);
    char *pattern = cJSON_GetObjectItem(json, "pattern")->valuestring;

    color_data.R = red;
    color_data.G = green;
    color_data.B = blue;

    if(strcmp(pattern, "rainbow") == NULL)
        show_pattern = RAINBOW;
    else if(strcmp(pattern, "static") == NULL)
        show_pattern = STATIC;

    const char resp[] = "Updated";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    cJSON_Delete(json);

    return ESP_OK;
}

/* Endpoints */

httpd_uri_t uri_post = {
    .uri = "/config",
    .method = HTTP_POST,
    .handler = post_handler,
    .user_ctx = NULL};

httpd_uri_t uri_get = {
    .uri = "/echo",
    .method = HTTP_GET,
    .handler = echo_handler,
    .user_ctx = NULL};

httpd_uri_t led_post = {
    .uri = "/led",
    .method = HTTP_POST,
    .handler = led_post_handler,
    .user_ctx = NULL};

/* Register Endpoints*/

static httpd_handle_t start_webserver(void){
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK){
        httpd_register_uri_handler(server, &uri_post);
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &led_post);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server){
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


/* End Server code*/ 

int init_config(){
    return read_config(&wifi_config_txt);
}

int update_config(){
   return write_config(&wifi_config_txt);
}


void init_led(){
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(CONFIG_EXAMPLE_RMT_TX_GPIO, RMT_TX_CHANNEL);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(CONFIG_EXAMPLE_STRIP_LED_NUMBER, (led_strip_dev_t)config.channel);
    strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!strip) {
        ESP_LOGE(TAG, "install WS2812 driver failed");
    }

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

    init_led();

    // //Check the status of mode pin
    gpio_set_direction(MODE_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(MODE_PIN, GPIO_PULLUP_ONLY);

    if (!gpio_get_level(MODE_PIN)){
        ESP_LOGI(AP_TAG, "ESP_WIFI_MODE_AP");
        wifi_init(WIFI_AP);
    }
    else
    {
        ESP_LOGI(STA_TAG, "ESP_WIFI_MODE_STA");
        wifi_init(WIFI_STA);
    }
    
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
    server = start_webserver();

    xTaskCreatePinnedToCore(
            renderLedTask,            /* Task function. */
            "Task_2",                 /* name of task. */
            1000,                    /* Stack size of task */
            NULL,                     /* parameter of the task */
            1,                        /* priority of the task */
            &Task2,                   /* Task handle to keep track of created task */
            1); 
}
