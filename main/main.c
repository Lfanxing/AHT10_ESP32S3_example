#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "aht10_sensor.h"

void app_main(void)
{
    ESP_LOGI("MAIN", "Starting AHT10 Sensor Demo on ESP32-S3");

    // 创建传感器实例 (面向对象方式)
    aht10_sensor_t my_sensor;
    aht10_data_t sensor_data;

    // 初始化
    if (aht10_init(&my_sensor) != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to initialize AHT10 sensor!");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGI("MAIN", "Initialization successful. Starting loop...");

    while (1) {
        if (aht10_read(&my_sensor, &sensor_data) == ESP_OK) {
            if (sensor_data.is_valid) {
                ESP_LOGI("MAIN", "Humidity: %.2f %%  Temperature: %.2f C", 
                         sensor_data.humidity, sensor_data.temperature);
            } else {
                ESP_LOGW("MAIN", "Data invalid");
            }
        } else {
            ESP_LOGE("MAIN", "Read failed, attempting recovery...");
            // 简单的错误恢复：重新初始化
            aht10_deinit(&my_sensor);
            vTaskDelay(pdMS_TO_TICKS(500));
            if (aht10_init(&my_sensor) != ESP_OK) {
                ESP_LOGE("MAIN", "Re-init failed");
            }
        }

        // 建议读取间隔 >= 1s，让传感器充分恢复
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}