#ifndef APPCONFIG_H
#define APPCONFIG_H
#include "cJSON.h"

#define MAX_CONFIG_LEN 80

struct sta_data_1 {
    char *ssid;
    char *psswd;
    char *mqtt; // mqtt server address
    char *url;
};

struct sta_data {
    char ssid[MAX_CONFIG_LEN];
    char psswd[MAX_CONFIG_LEN];
    char mqtt[MAX_CONFIG_LEN]; // mqtt server address
};

int read_config(struct sta_data *);
int write_config(struct sta_data *);

#endif /*APPCONFIG_H*/