#ifndef AHT10_SENSOR_H
#define AHT10_SENSOR_H

#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_timer.h"

// ================= 配置宏定义 =================
// 用户可在此处修改硬件接口
#define AHT10_I2C_NUM             I2C_NUM_0       // 使用的I2C端口号
#define AHT10_I2C_SCL_GPIO        GPIO_NUM_9      // SCL引脚 (根据实际电路修改)
#define AHT10_I2C_SDA_GPIO        GPIO_NUM_8      // SDA引脚 (根据实际电路修改)
#define AHT10_I2C_FREQ_HZ         400000          // I2C频率 400kHz
#define AHT10_I2C_TIMEOUT_MS      1000            // 操作超时时间 ms
#define AHT10_I2C_ADDR            0x38            // AHT10默认地址 (0x70 >> 1)

// 标签用于日志打印
#define AHT10_TAG "AHT10_Sensor"

// ================= 数据结构定义 =================

/**
 * @brief 传感器数据结果结构体
 */
typedef struct {
    float humidity;   // 湿度 %RH
    float temperature;// 温度 ℃
    bool is_valid;    // 数据是否有效
} aht10_data_t;

/**
 * @brief 传感器对象实例结构体 (面向对象思想)
 */
typedef struct {
    i2c_port_t i2c_num;
    uint8_t i2c_addr;
    bool is_initialized;
    aht10_data_t last_data;
} aht10_sensor_t;

// ================= 函数声明 =================

/**
 * @brief 初始化AHT10传感器驱动及硬件I2C
 * @param sensor 传感器实例指针
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t aht10_init(aht10_sensor_t *sensor);

/**
 * @brief 读取一次温湿度数据
 * @param sensor 传感器实例指针
 * @param data 输出数据指针
 * @return esp_err_t ESP_OK表示读取成功
 */
esp_err_t aht10_read(aht10_sensor_t *sensor, aht10_data_t *data);

/**
 * @brief 软件复位传感器
 * @param sensor 传感器实例指针
 * @return esp_err_t 
 */
esp_err_t aht10_reset(aht10_sensor_t *sensor);

/**
 * @brief 卸载驱动，释放I2C资源
 * @param sensor 传感器实例指针
 */
void aht10_deinit(aht10_sensor_t *sensor);

#endif // AHT10_SENSOR_H
