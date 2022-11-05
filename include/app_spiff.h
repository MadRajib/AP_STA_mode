#ifndef APPSPIFF_H
#define APPSPIFF_H

#include "esp_spiffs.h"
#include "esp_log.h"

int init_spiff(const char* base_path, esp_vfs_spiffs_conf_t **conf);
void dinit_spiff(esp_vfs_spiffs_conf_t *conf);
int read_from_spiff(const char* path, char *buff);
int write_to_spiff(const char* path, char* buff, long size);

#endif /*APPSPIFF_H*/