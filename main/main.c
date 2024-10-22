#include "wifi_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "wifi_manager.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "sensor_task.h"
#include "nvs_flash.h"

#define BUTTON_PIN GPIO_NUM_0  // Pino do botão
#define LED_PIN GPIO_NUM_2  // GPIO do LED na ESP32 WROOM-32
#define DNS_PORT 53
#define CAPTIVE_PORTAL_IP "192.168.4.1"

// Função para configurar o LED como saída
void configure_led() {
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
}

// Função para piscar o LED (modo AP)
void blink_led_task(void *pvParameter) {
    while (true) {
        gpio_set_level(LED_PIN, 1);  // Liga o LED
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Aguardar 500ms
        gpio_set_level(LED_PIN, 0);  // Desliga o LED
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Aguardar 500ms
    }
}

// Função para manter o LED aceso (modo STA)
void led_on() {
    gpio_set_level(LED_PIN, 1);  // Liga o LED permanentemente
}

// Função para apagar o LED (caso precise resetar)
void led_off() {
    gpio_set_level(LED_PIN, 0);  // Desliga o LED
}


extern void erase_wifi_credentials();

void check_reset_button() {
    while (true) {
        if (gpio_get_level(BUTTON_PIN) == 0) {  // Botão pressionado (nível baixo)
            ESP_LOGI("MAIN", "Botão pressionado. Resetando configurações WiFi.");
            
            // Apaga as credenciais WiFi da NVS
            erase_wifi_credentials();

            // Reinicia o dispositivo
            esp_restart();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Aguarda 100ms antes de verificar novamente
    }
}


void app_main() {
    // Desabilitar o Watchdog Timer do task idle
    esp_task_wdt_deinit();
    //esp_task_wdt_init(10, false);  // Inicializa o WDT com um timeout de 10 segundos

    // Inicializa NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Configura o GPIO do botão e do LED
    configure_led();  // Configura o LED

    // Configura o GPIO do botão
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(BUTTON_PIN);

    // Configura WiFi
    bool credentials_exist = wifi_credentials_exist();
    if (!credentials_exist) {
        ESP_LOGI("WIFI_MANAGER", "Nenhuma credencial WiFi encontrada. Iniciando modo AP...");
        start_ap_mode();  // Iniciar o modo AP para configuração
    } else {
        ESP_LOGI("WIFI_MANAGER", "Conectando-se ao WiFi salvo...");
    }

    // Inicia a rotina do sensor
    xTaskCreate(&sensor_task, "sensor_task", 2048, NULL, 5, NULL);

    // Envia dados para o thingspeak
    // xTaskCreate(&send_data_thingspeak, "send_data_thingspeak", 2048, NULL, 5, NULL);

    // Verifica o botão de reset
    xTaskCreate(check_reset_button, "check_reset_button", 2048, NULL, 5, NULL);   
}