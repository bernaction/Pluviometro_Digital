#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "wifi_manager.h"
#include "esp_timer.h"
#include "sensor_task.h"
#include "nvs_flash.h"

#define BUTTON_PIN GPIO_NUM_0  // Pino do botão
#define BUTTON_PRESS_TIME 5000 // Tempo de 5 segundos para resetar

extern void erase_wifi_credentials();

void check_reset_button() {
    int64_t press_start_time = 0;  // Use uma variável para armazenar o tempo inicial da pressão

    while (true) {
        if (gpio_get_level(BUTTON_PIN) == 0) {  // Botão pressionado
            if (press_start_time == 0) {
                press_start_time = esp_timer_get_time();  // Registra o tempo de início
            } else if ((esp_timer_get_time() - press_start_time) >= BUTTON_PRESS_TIME * 1000) {  // Verifica se passaram 5 segundos
                ESP_LOGI("MAIN", "Botão pressionado por 5 segundos. Resetando configurações WiFi.");
                erase_wifi_credentials();
                esp_restart();
            }
        } else {  // Botão não pressionado
            press_start_time = 0;  // Reseta o tempo
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Aguarda 100ms antes de verificar novamente
    }
}

void app_main() {
    // Inicializa NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Configura o GPIO do botão
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(BUTTON_PIN);

    // Configura WiFi
    bool credentials_exist = wifi_credentials_exist();
    start_wifi_configuration(credentials_exist, "default_ssid", "default_password");

    // Inicia a rotina do sensor
    xTaskCreate(&sensor_task, "sensor_task", 2048, NULL, 5, NULL);

    // Envia dados para o thingspeak
    xTaskCreate(&send_data_thingspeak, "send_data_thingspeak", 2048, NULL, 5, NULL);

    // Verifica o botão de reset
    //xTaskCreate(check_reset_button, "check_reset_button", 2048, NULL, 5, NULL);   
}