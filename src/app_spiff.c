#include "app_spiff.h"

#define SPIFF_TAG "APP_SPIFF"

int init_spiff(const char* base_path, esp_vfs_spiffs_conf_t **conf){

    ESP_LOGI(SPIFF_TAG, "Initializing SPIFFS");

    *conf = calloc(1, sizeof(esp_vfs_spiffs_conf_t));

    (*conf)->base_path = base_path;
    (*conf)->partition_label = NULL;
    (*conf)->max_files = 5;
    (*conf)->format_if_mount_failed = true;

    esp_err_t ret = esp_vfs_spiffs_register(*conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SPIFF_TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(SPIFF_TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(SPIFF_TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        goto EXIT;
    }
    
    return 0;

EXIT:
    dinit_spiff(*conf);
    free(*conf);
    conf = NULL;
    return -1;
}

void dinit_spiff(esp_vfs_spiffs_conf_t *conf){
    if(!conf) return;
    esp_vfs_spiffs_unregister(conf->partition_label);
    ESP_LOGI(SPIFF_TAG, "SPIFFS unmounted");
}

int read_from_spiff(const char* path, char *buff){

    ESP_LOGI(SPIFF_TAG, "Reading file");

    FILE *f = fopen(path, "rb");
    if (!f){
        ESP_LOGE(SPIFF_TAG, "Failed to open file for reading");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    fread(buff, 1, len, f);
    fclose(f);
    buff[len] = '\0';
    
    return 0;
}

int write_to_spiff(const char* path, char* buff, long size){
    
    FILE *wf = fopen(path, "wb");
    if (!wf){
        ESP_LOGE(SPIFF_TAG, "Failed to open file for writing");
        return -1;
    }

    fprintf(wf, "%s", buff);
    fclose(wf);

    return 0;
}