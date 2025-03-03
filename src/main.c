#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/netdb.h"
#include "esp_netif.h"
#include "lwip/ip_addr.h" // Добавили

// Замените на свои данные Wi-Fi
#define WIFI_SSID "MTSRouter_2.4GHz_001407" // Замените на ваш SSID
#define WIFI_PASSWORD "n3cr3GaY"            // Замените на ваш пароль

static const char *TAG = "HTTP_SERVER";
static bool dhcp_stopped = false; // Добавляем флаг

// Обработчик событий Wi-Fi
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            dhcp_stopped = false; // Сбрасываем флаг при отключении
            esp_wifi_connect();
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP, IP:" IPSTR, IP2STR(&event->ip_info.ip));

        if (!dhcp_stopped) { // Проверяем, был ли уже остановлен DHCP
            // Внутри этого обработчика настраиваем статический IP
            esp_netif_t *esp_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            esp_netif_ip_info_t ip_info;

            //ip_addr_t ip, gw, netmask; // Больше не нужно

            ipaddr_aton("192.168.1.20", &ip_info.ip); // Вместо inet_pton
            ipaddr_aton("192.168.1.1", &ip_info.gw);  // Вместо inet_pton
            ipaddr_aton("255.255.255.0", &ip_info.netmask); // Вместо inet_pton

            ESP_ERROR_CHECK(esp_netif_dhcpc_stop(esp_netif));
            ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif, &ip_info));

            dhcp_stopped = true; // Устанавливаем флаг, что DHCP остановлен
        }
    }
}

// Инициализация Wi-Fi
void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler, NULL, NULL);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Обработчик GET запроса
esp_err_t http_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET request received");

    // Открываем файл index.html
    FILE *fd = fopen("/spiffs/index.html", "r");
    if (fd == NULL)
    {
        ESP_LOGE(TAG, "Failed to open index.html");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Определяем размер файла
    fseek(fd, 0, SEEK_END);
    long file_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    // Выделяем память для содержимого файла
    char *file_content = (char *)malloc(file_size + 1);
    if (file_content == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for file content");
        fclose(fd);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Считываем содержимое файла
    size_t read_size = fread(file_content, 1, file_size, fd);
    if (read_size != file_size)
    {
        ESP_LOGE(TAG, "Failed to read file completely");
        fclose(fd);
        free(file_content);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    file_content[file_size] = '\0'; // Добавляем нулевой символ

    // Закрываем файл
    fclose(fd);

    // Отправляем содержимое файла клиенту
    httpd_resp_send(req, file_content, HTTPD_RESP_USE_STRLEN);

    // Освобождаем память
    free(file_content);

    return ESP_OK;
}

// Описание GET запроса
httpd_uri_t get_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = http_get_handler,
    .user_ctx = NULL};

// Запуск HTTP сервера
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &get_uri);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

// Остановка HTTP сервера
void stop_webserver(httpd_handle_t server)
{
    httpd_stop(server);
}

#include "esp_spiffs.h"

void init_spiffs(void)
{
    esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "SPIFFS mounted");
}

void app_main(void)
{
    // Инициализация NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Инициализация Wi-Fi
    wifi_init();
    init_spiffs();

    // Запускаем HTTP сервер
    httpd_handle_t server = start_webserver();
    if (server == NULL)
    {
        ESP_LOGE(TAG, "Failed to start webserver!");
        return;
    }

    ESP_LOGI(TAG, "Server started");
}
