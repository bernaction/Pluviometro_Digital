#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sensor_task.h"
#include "esp_http_client.h"

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
            ESP_LOGI(TAG, "Borda de descida detectada. Contagem: %d", contador);
        }

        ultimo_estado = estado_atual;
    }
}

void send_data_thingspeak(void *pvParameter){

    // Calculo da precipitação
    precipitacao = (contador * 0.15)*12; // Substituir "0.15" pela quantidade de mm que cada virada do pluviometro representa

    contador = 0; // Zera o contador para começar a próxima aquisição

    //Envio do dado para o thingspeak
    char url[256];
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

    vTaskDelay(300000 / portTICK_PERIOD_MS); // Delay de 300 segundos (5 minutos) entre cada envio
}
