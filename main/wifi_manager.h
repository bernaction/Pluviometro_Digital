#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

void start_wifi_configuration(bool credentials_exist, const char* ssid, const char* password);
void start_sta_mode(const char* ssid, const char* password);
void start_ap_mode();
bool wifi_credentials_exist();
void save_wifi_credentials(const char* ssid, const char* password);
void erase_wifi_credentials();

#endif
