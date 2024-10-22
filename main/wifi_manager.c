#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include <string.h>
#include "lwip/sockets.h"
#include "lwip/dns.h"

#include "wifi_manager.h"

#define DNS_PORT 53
#define CAPTIVE_PORTAL_IP "192.168.4.1"

static const char* TAG = "WIFI_MANAGER";
static bool connecting = false; 
EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

#define MIN(a,b) ((a) < (b) ? (a) : (b))  // Define a macro MIN
static bool wifi_initialized = false;  // Verifica se o WiFi foi inicializado

void dns_server_task(void *pvParameters) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DNS_PORT);

    bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    char buffer[512];
    while (1) {
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);
        if (len > 0) {
            // Redireciona todas as consultas DNS para o IP do captive portal
            // ESP_LOGI("DNS", "Requisição DNS recebida, redirecionando para IP: %s", CAPTIVE_PORTAL_IP);
            memset(buffer + 2, 0x84, 1);  // Configura a flag de resposta DNS
            sendto(sock, buffer, len, 0, (struct sockaddr *)&client_addr, client_len);
        }
    }
    close(sock);
}


// Funções locais
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void start_http_server(); 

void erase_wifi_credentials() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);  // Abre o NVS com permissão de escrita
    if (err == ESP_OK) {
        err = nvs_erase_all(nvs_handle);  // Apaga todas as chaves no namespace "wifi_config"
        if (err == ESP_OK) {
            ESP_LOGI("MAIN", "Configurações de WiFi apagadas com sucesso.");
            nvs_commit(nvs_handle);  // Aplica as mudanças no NVS
        } else {
            ESP_LOGE("MAIN", "Erro ao apagar configurações de WiFi: %s", esp_err_to_name(err));
        }
        nvs_close(nvs_handle);  // Fecha o NVS
    } else {
        ESP_LOGE("MAIN", "Erro ao abrir NVS: %s", esp_err_to_name(err));
    }
}


// Função para inicializar o WiFi (apenas uma vez)
void initialize_wifi() {
    if (!wifi_initialized) {
        ESP_LOGI(TAG, "Inicializando WiFi...");

        // Inicializa a interface de rede WiFi
        esp_netif_init();
        esp_event_loop_create_default();

        // Registrar o handler de eventos
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        wifi_initialized = true;
    }
}

// Função para conectar/reconectar ao WiFi
void connect_wifi() {
    if (connecting) {
        ESP_LOGW(TAG, "Já estamos tentando conectar, ignorando nova tentativa.");
        return;
    }

    esp_err_t err = esp_wifi_connect();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Tentando conectar ao WiFi... 1");
        connecting = true;  // Marcar que estamos conectando
    } else {
        ESP_LOGE(TAG, "Erro ao conectar ao WiFi: %s", esp_err_to_name(err));
    }
}

// Função para iniciar o modo STA (Station)
void start_sta_mode(const char* ssid, const char* password) {
    if (!wifi_initialized) {
        initialize_wifi();
    }

    esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_config = { 0 };
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    connect_wifi();  // Inicia a conexão WiFi
}


// Função para iniciar o modo AP (Access Point)
void start_ap_mode() {
    configure_led();  // Configura o LED
    xTaskCreate(blink_led_task, "blink_led_task", 1024, NULL, 5, NULL);  // Iniciar a task para piscar o LED

    initialize_wifi();  // Garante que o WiFi foi inicializado

    // Para o WiFi se já estiver em execução
    esp_wifi_stop();

    // Inicializa a interface de rede WiFi AP
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) {
        ESP_LOGE(TAG, "Falha ao criar a interface WiFi AP");
        return;
    }

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = "PLUV_DIGIT_AP",
            .ssid_len = strlen("PLUV_DIGIT_AP"),
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Modo AP iniciado com SSID: PLUV_DIGIT_AP");

    // Inicia o servidor DNS para o captive portal
    xTaskCreate(&dns_server_task, "dns_server", 4096, NULL, 5, NULL);

    // Inicia o servidor HTTP para a página de configuração
    start_http_server();
}

// Função de gerenciamento de eventos WiFi
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "Iniciando conexão WiFi...");
            connect_wifi();  // Tenta conectar
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI(TAG, "WiFi desconectado. Tentando reconectar... 2");
            connecting = false;  // Permite nova tentativa de conexão
            connect_wifi();  // Tenta reconectar
            led_off();  // Desliga o LED se perder a conexão
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Conectado ao WiFi. Endereço IP: " IPSTR, IP2STR(&event->ip_info.ip));
        led_on();  // Liga o LED após a conexão bem-sucedida
        connecting = false;  // Conexão bem-sucedida, resetar a flag

        // Verifique se o EventGroup foi criado antes de definir o bit
        if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        } else {
            ESP_LOGE(TAG, "EventGroup não foi criado!");
        }
    } else {
        ESP_LOGW(TAG, "Evento inesperado: base=%s, id=%ld", event_base, (long int)event_id);
    }
}



// Função para iniciar a configuração WiFi
void start_wifi_configuration(bool credentials_exist, const char* ssid, const char* password) {
    // Certifique-se de que o grupo de eventos está criado
    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();  // Cria o grupo de eventos WiFi
        if (s_wifi_event_group == NULL) {
            ESP_LOGE(TAG, "Falha ao criar o grupo de eventos WiFi");
            return;
        } else {
            ESP_LOGE(TAG, "Grupo de eventos WiFi criado com sucesso");
        }
    }

    if (credentials_exist) {
        ESP_LOGI(TAG, "Conectando-se ao WiFi salvo...");
        start_sta_mode(ssid, password);
    } else {
        ESP_LOGI(TAG, "Iniciando em modo AP para configuracao WiFi...");
        start_ap_mode();
    }
}



// Funções para manipulação do NVS para salvar e carregar as credenciais
bool wifi_credentials_exist() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI("WIFI_MANAGER", "Nenhuma credencial WiFi encontrada na NVS");
        return false;
    }

    char ssid[32] = {0};
    char password[64] = {0};  // Buffer para a senha
    size_t ssid_len = sizeof(ssid);
    size_t password_len = sizeof(password);

    err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI("WIFI_MANAGER", "Nenhum SSID encontrado");
        nvs_close(nvs_handle);
        return false;
    } else if (err != ESP_OK) {
        ESP_LOGE("WIFI_MANAGER", "Erro ao carregar SSID da NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_get_str(nvs_handle, "password", password, &password_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI("WIFI_MANAGER", "Nenhuma senha encontrada");
        nvs_close(nvs_handle);
        return false;
    } else if (err != ESP_OK) {
        ESP_LOGE("WIFI_MANAGER", "Erro ao carregar senha da NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    // Log para verificar se as credenciais estão sendo carregadas corretamente
    ESP_LOGI("WIFI_MANAGER", "Credenciais carregadas - SSID: %s, Senha: %s", ssid, password);

    // Aplicar as credenciais carregadas
    wifi_config_t wifi_config = { 0 };
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);  // Colocar no modo STA
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);  // Configurar WiFi com as credenciais
    esp_wifi_start();  // Iniciar WiFi

    nvs_close(nvs_handle);
    return true;
}



void save_wifi_credentials(const char* ssid, const char* password) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("WIFI_MANAGER", "Erro ao abrir NVS para escrita: %s", esp_err_to_name(err));
        return;
    }

    // Salva o SSID
    err = nvs_set_str(nvs_handle, "ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE("WIFI_MANAGER", "Erro ao salvar o SSID: %s", esp_err_to_name(err));
    }

    // Salva a senha
    err = nvs_set_str(nvs_handle, "password", password);
    if (err != ESP_OK) {
        ESP_LOGE("WIFI_MANAGER", "Erro ao salvar a senha: %s", esp_err_to_name(err));
    }

    // Comitar as mudanças
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("WIFI_MANAGER", "Erro ao fazer commit das credenciais: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI("WIFI_MANAGER", "Credenciais WiFi salvas com sucesso");
    }

    nvs_close(nvs_handle);
}



// Função para redirecionar para a raiz
esp_err_t redirect_to_root_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Função para lidar com requisições GET (exibir a página de configuração)
esp_err_t get_handler(httpd_req_t *req) {
    const char *response = "<html><body><h1>Configuracao WiFi</h1><form action=\"/config\" method=\"POST\"><label>SSID: </label><input type=\"text\" name=\"ssid\"><br><label>Senha: </label><input type=\"password\" name=\"password\"><br><input type=\"submit\" value=\"Configurar\"></form></body></html>";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Função para lidar com o formulário POST de configuração
esp_err_t post_handler(httpd_req_t *req) {
    char buf[100] = {0};  // Buffer de entrada com valores zerados
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    // Processar o SSID e a senha garantindo que as strings sejam terminadas corretamente
    char ssid[32] = {0};
    char password[64] = {0};  // Buffer com 64 caracteres para a senha

    sscanf(buf, "ssid=%31[^&]&password=%63s", ssid, password);  // Limita os tamanhos máximos

    // Certifique-se de que as strings estão terminadas corretamente
    ssid[31] = '\0';  // Garante que o SSID seja terminado
    password[63] = '\0';  // Garante que a senha seja terminada

    // Logar o SSID e a senha para depuração
    ESP_LOGI("WIFI_MANAGER", "Recebido SSID: %s, Senha: %s", ssid, password);

    // Salvar as credenciais no NVS
    save_wifi_credentials(ssid, password);

    // Enviar a resposta e reiniciar o ESP
    httpd_resp_send(req, "Configuração salva. Reiniciando o dispositivo...", HTTPD_RESP_USE_STRLEN);
    esp_restart();

    return ESP_OK;
}



httpd_uri_t uri_get = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = get_handler,
    .user_ctx  = NULL
};

httpd_uri_t uri_post = {
    .uri       = "/config",
    .method    = HTTP_POST,
    .handler   = post_handler,
    .user_ctx  = NULL
};

// Função para iniciar o servidor HTTP
void start_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Página de configuração
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
        
        // Redirecionar todas as outras requisições para a página de configuração
        httpd_uri_t uri_redirect = {
            .uri       = "/*",
            .method    = HTTP_GET,
            .handler   = redirect_to_root_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &uri_redirect);
    }
}