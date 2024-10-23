#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_http_client.h"

#include "sensor_task.h"
#include "wifi_manager.h"


#ifndef portTICK_PERIOD_MS
#define portTICK_PERIOD_MS (1000 / CONFIG_FREERTOS_HZ)
#endif
#include <portmacro.h>

#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_VERBOSE
#endif

#define SENSOR_PIN GPIO_NUM_4
static const char* TAG = "SENSOR_TASK";

static const char *TAG2 = "thing_speak";
#define THINGSPEAK_API_KEY "IJDIGYQD9KKACLAH"
#define THINGSPEAK_URL "http://api.thingspeak.com/update?api_key="

int contador = 0;
int ultimo_estado = 1;
float precipitacao = 0;

void sensor_task(void *pvParameter){
    gpio_reset_pin(SENSOR_PIN);  
    gpio_set_direction(SENSOR_PIN, GPIO_MODE_INPUT);

    ESP_LOGI(TAG, "Sensor inicializado. Aguardando eventos...");
    
    while (1) {
        int estado_atual = gpio_get_level(SENSOR_PIN);

        if (estado_atual == 0 && ultimo_estado == 1) {
            contador++;
            ESP_LOGI(TAG, "Borda de subida detectada. Contagem: %d", contador);
        }

        if (estado_atual == 1 && ultimo_estado == 0) {
            contador++;
            ESP_LOGI(TAG, "Borda de descida detectada. Contagem: %d", contador);
        }

        ultimo_estado = estado_atual;
        // Adiciona um pequeno delay para permitir que o FreeRTOS gerencie outras tarefas
        vTaskDelay(100 / portTICK_PERIOD_MS);  // 100 ms de delay
    }
}

void send_data_thingspeak(void *pvParameter) {
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        if (bits & WIFI_CONNECTED_BIT) {
            //vTaskDelay(900000 / portTICK_PERIOD_MS);  // 15 minutos de delay
            vTaskDelay(60000 / portTICK_PERIOD_MS);  //60 segundss de delay
            ESP_LOGI(TAG, "Conectado ao WiFi. Preparando para enviar dados...");
            precipitacao = (contador * 1.63) * 4; 
            contador = 0;

            // Envio do dado para o ThingSpeak
            static char url[256]; //memória estática global ou heap
            snprintf(url, sizeof(url), THINGSPEAK_URL THINGSPEAK_API_KEY "&field1=%.2f", precipitacao);

            esp_http_client_config_t config = {
                .url = url,
            };
            esp_http_client_handle_t client = esp_http_client_init(&config);
            esp_err_t err = esp_http_client_perform(client);

            if (err == ESP_OK) {
                ESP_LOGI(TAG2, "Dados enviados com sucesso: %s", url);
            } else {
                ESP_LOGE(TAG2, "Falha ao enviar dados: %s", esp_err_to_name(err));
            }
            esp_http_client_cleanup(client);
        } else {
            ESP_LOGE(TAG, "Não conectado ao WiFi. Tentando novamente em breve...");
            vTaskDelay(10000 / portTICK_PERIOD_MS);  // Espera 10 segundos antes de tentar novamente
        }
    }
}

