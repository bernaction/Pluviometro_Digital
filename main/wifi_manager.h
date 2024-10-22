#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

void start_wifi_configuration(bool credentials_exist, const char* ssid, const char* password);
void start_sta_mode(const char* ssid, const char* password);
void start_ap_mode();
bool wifi_credentials_exist();
void save_wifi_credentials(const char* ssid, const char* password);
void erase_wifi_credentials();

// Declaração das funções relacionadas ao LED
void configure_led(void);  // Configuração do LED
void blink_led_task(void *pvParameter);  // Task para piscar o LED
void led_on(void);  // Liga o LED fixo
void led_off(void);  // Desliga o LED


#endif
