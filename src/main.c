#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "rtc.h"
#include <math.h>
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "driver/gpio.h" // Добавлено

// Замените на свои данные Wi-Fi
#define WIFI_SSID "MTSRouter_2.4GHz_001407"
#define WIFI_PASSWORD "n3cr3GaY"

static const char *TAG = "ESP32_DEMO"; // Тег для логов

// Обработчик событий Wi-Fi
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
      esp_wifi_connect();
      break;
    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
      esp_wifi_connect();
      break;
    default:
      break;
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP, IP:" IPSTR, IP2STR(&event->ip_info.ip));
  }
}

// Инициализация Wi-Fi
void wifi_init(void) {
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

void wifi_scan_task(void *pvParameters) {
    while (true) {
        uint16_t number = 20;
        wifi_ap_record_t ap_info[number];
        uint16_t ap_count = 0;
        memset(ap_info, 0, sizeof(ap_info));

        esp_wifi_scan_start(NULL, true);
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);

        for (int i = 0; (i < number) && (i < ap_count); i++) {
            ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
            ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
  // Инициализация NVS (Non-Volatile Storage)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Настраиваем вывод для светодиода (pin 2)
  esp_rom_gpio_pad_select_gpio(2);
  gpio_set_direction(2, GPIO_MODE_OUTPUT);

  ESP_LOGI(TAG, "Hello from ESP32!");

  // Информация о чипе
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  ESP_LOGI(TAG, "ESP32 Chip info:");
  ESP_LOGI(TAG, "- Model: %s", (chip_info.model == CHIP_ESP32) ? "ESP32" : "Unknown");
  ESP_LOGI(TAG, "- Cores: %d", chip_info.cores);
  ESP_LOGI(TAG, "- Revision: %d", chip_info.revision);
  ESP_LOGI(TAG, "- Features: %s%s%s",
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi-BGN " : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE " : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT " : "");

  uint32_t flash_size;
  esp_flash_get_size(NULL, &flash_size);
  ESP_LOGI(TAG, "- Flash size: %luMB", flash_size / (1024 * 1024)); // Исправлено %u на %lu

  // Инициализация Wi-Fi
  wifi_init();
    
  // Создаем задачу сканирования Wi-Fi
  xTaskCreate(wifi_scan_task, "wifi_scan_task", 4096, NULL, 5, NULL);

  while (true) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Получаем время в микросекундах
    uint64_t time_us = esp_rtc_get_time_us();

    // Преобразуем в секунды (с дробной частью)
    double time_s = (double)time_us / 1000000.0;

    // Выводим время с двумя знаками после запятой
    ESP_LOGI(TAG, "Tick written by Vlad, time: %.2f seconds", time_s);

    // Мигаем светодиодом
    gpio_set_level(2, 1);
    vTaskDelay(250 / portTICK_PERIOD_MS);
    gpio_set_level(2, 0);
  }
}
