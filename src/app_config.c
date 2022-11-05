#include "app_config.h"
#include "app_spiff.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_log.h"

#define TAG "APP"

int read_config(struct sta_data *wifi_settings){

    int ret = 0;
    char *buff = NULL;
    esp_vfs_spiffs_conf_t *conf = NULL;

    ret = init_spiff("/spiffs", &conf);
    if(ret < 0)
        return -1;

    ESP_LOGI(TAG, "Reading file");
    FILE *f = fopen("/spiffs/config.txt", "rb");
    if (!f){
        ESP_LOGE(TAG, "Failed to open file for reading");
        ret = -1;
        goto RET;
    }

    fgets(wifi_settings->ssid, MAX_CONFIG_LEN, f);
    wifi_settings->ssid[strlen(wifi_settings->ssid) - 1] = '\0';
    
    fgets(wifi_settings->psswd, MAX_CONFIG_LEN, f);
    wifi_settings->psswd[strlen(wifi_settings->psswd) - 1] = '\0';

    fgets(wifi_settings->mqtt, MAX_CONFIG_LEN, f);
    wifi_settings->mqtt[strlen(wifi_settings->mqtt) - 1] = '\0';

RET:
    fclose(f);
    if(buff) {
        free(buff);
        buff = NULL;
    }

    dinit_spiff(conf);
    return ret;
}

int write_config(struct sta_data *wifi_settings){

    int ret = 0;
    char *buff = NULL;
    esp_vfs_spiffs_conf_t *conf = NULL;

    ret = init_spiff("/spiffs", &conf);
    if(ret < 0)
        return -1;

    ESP_LOGI(TAG, "Writing to file");
    FILE *f = fopen("/spiffs/config.txt", "wb");
    if (!f){
        ESP_LOGE(TAG, "Failed to open file for writing");
        ret = -1;
        goto WRET;
    }

    fprintf(f,
        "%s\n%s\n%s\n",
        wifi_settings->ssid,
        wifi_settings->psswd,
        wifi_settings->mqtt
    );    

WRET:
    fclose(f);
    if(buff){
        free(buff);
        buff = NULL;
    }

    dinit_spiff(conf);
    return 0;
}

