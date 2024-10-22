#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "nvs_flash.h"

// libs dev
#include "wifi_manager.h"
#include "sensor_task.h"

#define BOOT_BUTTON_PIN GPIO_NUM_0  // O botão BOOT está ligado ao GPIO 0
#define BUTTON_PRESS_TIME 5000  // Tempo para resetar em milissegundos (5 segundos)
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
    int64_t press_start_time = 0;  // Variável para armazenar o tempo inicial da pressão

    while (true) {
        if (gpio_get_level(BOOT_BUTTON_PIN) == 0) {  // Botão pressionado
            if (press_start_time == 0) {
                press_start_time = esp_timer_get_time();  // Registra o tempo de início
            } else if ((esp_timer_get_time() - press_start_time) >= BUTTON_PRESS_TIME * 1000) {
                ESP_LOGI("MAIN", "Botão pressionado por 5 segundos. Resetando configurações WiFi.");
                erase_wifi_credentials();  // Função que apaga as credenciais WiFi
                esp_restart();  // Reinicia o dispositivo
            }
        } else {
            press_start_time = 0;  // Reseta o tempo quando o botão não está pressionado
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Aguarda 100ms antes de verificar novamente
    }
}

void task1(void *pvParameter) {
    while (1) {
        printf("Task 1 executando no núcleo: %d\n", xPortGetCoreID());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void task2(void *pvParameter) {
    while (1) {
        printf("Task 2 executando no núcleo: %d\n", xPortGetCoreID());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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

    // Configura o botão BOOT (GPIO 0)
    gpio_reset_pin(BOOT_BUTTON_PIN);
    gpio_set_direction(BOOT_BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(BOOT_BUTTON_PIN);  // debounce

    // Configura WiFi
    bool credentials_exist = wifi_credentials_exist();
    if (!credentials_exist) {
        ESP_LOGI("WIFI_MANAGER", "Nenhuma credencial WiFi encontrada. Iniciando modo AP...");
        start_ap_mode();  // Iniciar o modo AP para configuração
    } else {
        ESP_LOGI("WIFI_MANAGER", "Conectando-se ao WiFi salvo...");
    }

    // Inicia a rotina do sensor no núcleo 0 (PRO CPU)
    xTaskCreatePinnedToCore(sensor_task, "sensor_task", 2048, NULL, 5, NULL, 0);  // Núcleo 0

    // Envia dados para o thingspeak no núcleo 1 (APP CPU)
    xTaskCreatePinnedToCore(send_data_thingspeak, "send_data_thingspeak", 2048, NULL, 5, NULL, 1);  // Núcleo 1

    // Verifica o botão de reset no núcleo 1 (APP CPU)
    xTaskCreatePinnedToCore(check_reset_button, "check_reset_button", 2048, NULL, 5, NULL, 1);  // Núcleo 1
}