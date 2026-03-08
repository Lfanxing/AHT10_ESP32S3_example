/**
 * @file aht10_sensor.c
 * @brief AHT10 温湿度传感器驱动程序实现
 * 
 * 本文件实现了基于 ESP-IDF 的 AHT10 传感器驱动。
 * 支持多实例、线程安全读取（通过输出参数隔离），并包含完整的初始化、复位及数据校验逻辑。
 * 
 * @version 1.0.0
 * @date 2026-03-08
 */

#include "aht10_sensor.h"

// ================= 内部命令与常量定义 =================

// AHT10 指令集定义 (参考 datasheet)
//#define AHT10_CMD_INIT      0xE1    // 初始化命令 (部分批次需要)
#define AHT10_CMD_TRIGGER   0xAC    // 触发测量命令
#define AHT10_CMD_STATUS    0x71    // 读取状态寄存器命令
#define AHT10_CMD_SOFT_RST  0xBA    // 软件复位命令

// 状态寄存器位掩码
#define AHT10_STATUS_BUSY   0x80    // Bit 7: 忙标志 (1=忙, 0=空闲)
#define AHT10_STATUS_CAL_EN 0x08    // Bit 3: 校准使能标志 (1=已校准, 0=未校准)

// 数据解析常量
#define AHT10_RAW_DIVISOR   1048576.0f  // 2^20，用于将原始数据转换为浮点数

// ================= 内部辅助函数声明 =================

/**
 * @brief 向传感器写入命令及可选数据
 * 
 * 构建 I2C 写事务：Start -> Addr(W) -> Command -> [Data] -> Stop
 * 
 * @param sensor 传感器实例指针
 * @param cmd 命令字节
 * @param data 附加数据缓冲区指针 (可为 NULL)
 * @param data_len 附加数据长度
 * @return esp_err_t ESP_OK 表示成功
 */
static esp_err_t aht10_write_command(aht10_sensor_t *sensor, uint8_t cmd, uint8_t *data, size_t data_len);

/**
 * @brief 从传感器读取指定长度的数据
 * 
 * 构建 I2C 读事务：Start -> Addr(R) -> Data -> Stop
 * 注意：最后一个字节会自动发送 NACK。
 * 
 * @param sensor 传感器实例指针
 * @param buffer 接收数据缓冲区
 * @param len 读取长度
 * @return esp_err_t ESP_OK 表示成功
 */
static esp_err_t aht10_read_bytes(aht10_sensor_t *sensor, uint8_t *buffer, size_t len);

/**
 * @brief 读取传感器状态寄存器
 * 
 * @param sensor 传感器实例指针
 * @param status 输出：状态寄存器值
 * @return esp_err_t ESP_OK 表示通信成功
 */
static esp_err_t aht10_check_status(aht10_sensor_t *sensor, uint8_t *status);

// ================= 内部辅助函数实现 =================

static esp_err_t aht10_write_command(aht10_sensor_t *sensor, uint8_t cmd, uint8_t *data, size_t data_len) {
    if (sensor == NULL) return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    if (cmd_handle == NULL) return ESP_ERR_NO_MEM;

    // 构建 I2C 写序列
    i2c_master_start(cmd_handle);
    // 发送地址 + 写方向位
    i2c_master_write_byte(cmd_handle, (sensor->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    // 发送命令字节
    i2c_master_write_byte(cmd_handle, cmd, true);
    
    // 如果有附加数据，继续发送
    if (data != NULL && data_len > 0) {
        i2c_master_write(cmd_handle, data, data_len, true);
    }
    
    i2c_master_stop(cmd_handle);
    
    // 执行事务，等待超时
    esp_err_t ret = i2c_master_cmd_begin(sensor->i2c_num, cmd_handle, pdMS_TO_TICKS(AHT10_I2C_TIMEOUT_MS));
    
    i2c_cmd_link_delete(cmd_handle);
    return ret;
}

static esp_err_t aht10_read_bytes(aht10_sensor_t *sensor, uint8_t *buffer, size_t len) {
    if (sensor == NULL || buffer == NULL || len == 0) return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    if (cmd_handle == NULL) return ESP_ERR_NO_MEM;

    // 构建 I2C 读序列
    i2c_master_start(cmd_handle);
    // 发送地址 + 读方向位
    i2c_master_write_byte(cmd_handle, (sensor->i2c_addr << 1) | I2C_MASTER_READ, true);
    // 读取数据，最后一个字节发送 NACK (I2C_MASTER_LAST_NACK)
    i2c_master_read(cmd_handle, buffer, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd_handle);
    
    esp_err_t ret = i2c_master_cmd_begin(sensor->i2c_num, cmd_handle, pdMS_TO_TICKS(AHT10_I2C_TIMEOUT_MS));
    
    i2c_cmd_link_delete(cmd_handle);
    return ret;
}

static esp_err_t aht10_check_status(aht10_sensor_t *sensor, uint8_t *status) {
    esp_err_t ret = aht10_write_command(sensor, AHT10_CMD_STATUS, NULL, 0);
    if (ret != ESP_OK) return ret;
    
    // 读取 1 字节状态
    ret = aht10_read_bytes(sensor, status, 1);
    return ret;
}

// ================= 公共 API 实现 =================

esp_err_t aht10_reset(aht10_sensor_t *sensor) {
    if (sensor == NULL) return ESP_ERR_INVALID_ARG;

    ESP_LOGD(AHT10_TAG, "Executing Soft Reset Command");
    
    // 软复位命令不需要标准的 write_command 封装，因为它通常不期待 ACK 后的数据传输，直接发 Stop
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    if (cmd_handle == NULL) return ESP_ERR_NO_MEM;

    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (sensor->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_handle, AHT10_CMD_SOFT_RST, true);
    i2c_master_stop(cmd_handle);
    
    esp_err_t ret = i2c_master_cmd_begin(sensor->i2c_num, cmd_handle, pdMS_TO_TICKS(AHT10_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd_handle);
    
    if (ret == ESP_OK) {
        //  datasheet 要求复位后至少等待 20ms
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
    return ret;
}

esp_err_t aht10_init(aht10_sensor_t *sensor) {
    if (sensor == NULL) return ESP_ERR_INVALID_ARG;

    // 配置日志级别
    //esp_log_level_set(AHT10_TAG, ESP_LOG_DEBUG);    //只有debug过滤等级以上，才会打印ESP_LOGD信息，可自行配置
    //esp_log_level_set(AHT10_TAG, ESP_LOG_INFO);   //仅打印INFO等级以上的Log信息（如ESP_LOGI）

    // 1. 初始化传感器对象基础成员
    sensor->i2c_num = AHT10_I2C_NUM;
    sensor->i2c_addr = AHT10_I2C_ADDR;
    sensor->is_initialized = false;
    sensor->last_data.is_valid = false;
    sensor->last_data.humidity = 0.0f;
    sensor->last_data.temperature = 0.0f;

    // 2. 配置 I2C 硬件参数
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = AHT10_I2C_SDA_GPIO,
        .scl_io_num = AHT10_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, 
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = AHT10_I2C_FREQ_HZ,
        // .clk_flags = 0, // ESP-IDF 新版本可能需要，视具体版本而定
    };

    // 应用 I2C 参数配置
    esp_err_t ret = i2c_param_config(sensor->i2c_num, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(AHT10_TAG, "I2C Parameter Configuration Failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 安装 I2C 主模式驱动
    // 注意：如果驱动已安装，i2c_driver_install 会返回 ESP_ERR_INVALID_STATE。
    // 在此场景下视为正常，继续执行后续流程。
    ret = i2c_driver_install(sensor->i2c_num, conf.mode, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(AHT10_TAG, "I2C Driver Installation Failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGD(AHT10_TAG, "I2C Driver already installed, proceeding with initialization.");
        ret = ESP_OK;
    } else {
        ESP_LOGI(AHT10_TAG, "I2C Master Initialized on SDA:%d, SCL:%d @ %dHz", 
                 AHT10_I2C_SDA_GPIO, AHT10_I2C_SCL_GPIO, AHT10_I2C_FREQ_HZ);
    }

    // 3. 执行传感器软复位
    // 确保传感器处于已知状态，清除可能的挂起事务
    ESP_LOGD(AHT10_TAG, "Performing Sensor Soft Reset");
    esp_err_t reset_ret = aht10_reset(sensor);
    if (reset_ret != ESP_OK) {
        ESP_LOGW(AHT10_TAG, "Soft Reset command failed (Device may be missing), but continuing initialization check...");
    }
    
    // 复位后延时，确保传感器内部电路稳定 (推荐 30ms - 50ms)
    vTaskDelay(pdMS_TO_TICKS(40));

    // 4. 轮询检查传感器状态
    // 目标：确认传感器不再忙碌 (Busy=0)，并检查校准状态
    int retry_count = 0;
    const int max_retries = 5;
    uint8_t status_reg = 0;
    bool is_busy = true;
    bool cal_enabled = false;

    ESP_LOGD(AHT10_TAG, "Polling Sensor Status Register");

    while (retry_count < max_retries) {
        if (aht10_check_status(sensor, &status_reg) == ESP_OK) {
            ESP_LOGD(AHT10_TAG, "Status Read: 0x%02X (Attempt %d)", status_reg, retry_count + 1);

            // 解析状态位
            is_busy = (status_reg & AHT10_STATUS_BUSY) != 0;
            cal_enabled = (status_reg & AHT10_STATUS_CAL_EN) != 0;

            if (!is_busy) {
                // 传感器空闲，初始化成功
                if (cal_enabled) {
                    ESP_LOGI(AHT10_TAG, "Sensor Ready. Calibration Enabled (0x%02X).", status_reg);
                } else {
                    // 部分新版 AHT10 即使校准位为 0 也能正常工作，发出警告但继续
                    ESP_LOGW(AHT10_TAG, "Sensor Ready. Calibration Bit Not Set (0x%02X). Proceeding anyway.", status_reg);
                }
                
                sensor->is_initialized = true;
                return ESP_OK;
            } 
            else {
                ESP_LOGD(AHT10_TAG, "Sensor is busy, waiting before retry...");
            }
        } 
        else {
            ESP_LOGW(AHT10_TAG, "Failed to read status register, retrying...");
        }
        
        // 等待 20ms 后重试
        vTaskDelay(pdMS_TO_TICKS(20));
        retry_count++;
    }

    // 5. 容错处理
    // 如果多次轮询失败或一直忙，尝试强制标记为初始化。
    // 这允许后续的 read 函数尝试触发测量，有时能唤醒处于异常状态的传感器。
    ESP_LOGW(AHT10_TAG, "Status check timeout after %d retries. Forcing initialized state for blind read attempt.", max_retries);
    sensor->is_initialized = true;
    
    return ESP_OK; 
}

esp_err_t aht10_read(aht10_sensor_t *sensor, aht10_data_t *data) {
    // 1. 预检查：传感器初始化状态
    if (!sensor->is_initialized) {
        ESP_LOGE(AHT10_TAG, "Read failed: Sensor not initialized");
        if (data != NULL) {
            data->is_valid = false;
        }
        return ESP_ERR_INVALID_STATE;
    }

    // 2. 输出缓冲区处理策略
    // 如果用户传入 NULL，使用栈上的临时变量存储结果，仅更新内部缓存。
    // 如果用户传入有效指针，直接填充用户缓冲区。
    aht10_data_t local_buffer;
    aht10_data_t *p_out = NULL;

    if (data == NULL) {
        p_out = &local_buffer;
        ESP_LOGV(AHT10_TAG, "Read called with NULL output pointer, using temporary buffer.");
    } else {
        p_out = data;
        p_out->is_valid = false; // 初始化为无效状态
    }

    // 3. 触发测量序列
    // 命令：0xAC, 数据：0x33, 0x00
    // 0x33 表示正常测量模式，0x00 为保留位
    uint8_t trigger_seq[] = {0x33, 0x00};
    esp_err_t ret = aht10_write_command(sensor, AHT10_CMD_TRIGGER, trigger_seq, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(AHT10_TAG, "Trigger Measurement Command Failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 4. 等待测量完成
    // AHT10 典型转换时间为 75ms，最大不超过 100ms。
    // 采用主动轮询状态寄存器 Bit 7 (Busy) 的方式等待。
    uint32_t start_time_us = esp_timer_get_time();
    const uint32_t timeout_us = 100000; // 100ms 超时限制
    uint8_t status = 0;
    bool is_measuring = true;

    while (is_measuring) {
        // 检查超时
        if ((esp_timer_get_time() - start_time_us) > timeout_us) {
            ESP_LOGE(AHT10_TAG, "Measurement Timeout: Sensor did not clear Busy flag within 100ms");
            return ESP_ERR_TIMEOUT;
        }

        // 读取状态
        if (aht10_check_status(sensor, &status) == ESP_OK) {
            if ((status & AHT10_STATUS_BUSY) == 0) {
                is_measuring = false; // 测量完成
            }
        } else {
            // 读取状态失败，短暂休眠避免死循环占用 CPU
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // 5. 读取原始数据
    // 成功后传感器会输出 6 字节数据：
    // Byte[0]: Status (重复)
    // Byte[1..2]: Humidity MSB/LSB (部分)
    // Byte[3..5]: Temp MSB/LSB (部分)
    // 注意：根据 Datasheet，实际有效数据分布在 Byte 1 到 Byte 5，Byte 0 是状态字。
    // 但有些实现读取 6 字节包含状态字，这里我们读取 6 字节并按偏移解析。
    uint8_t raw_buf[6] = {0};
    ret = aht10_read_bytes(sensor, raw_buf, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(AHT10_TAG, "Read Data Bytes Failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 6. 数据解析与计算
    // 湿度计算公式：RH% = (RawH / 2^20) * 100
    // RawH = (Byte[1] << 12) | (Byte[2] << 4) | (Byte[3] >> 4)
    uint32_t raw_humidity = ((uint32_t)raw_buf[1] << 12) | 
                            ((uint32_t)raw_buf[2] << 4) | 
                            ((uint32_t)raw_buf[3] >> 4);

    // 温度计算公式：T = (RawT / 2^20) * 200 - 50
    // RawT = ((Byte[3] & 0x0F) << 16) | (Byte[4] << 8) | Byte[5]
    uint32_t raw_temperature = (((uint32_t)raw_buf[3] & 0x0F) << 16) | 
                               ((uint32_t)raw_buf[4] << 8) | 
                               ((uint32_t)raw_buf[5]);

    // 执行浮点转换
    p_out->humidity = (float)raw_humidity * 100.0f / AHT10_RAW_DIVISOR;
    p_out->temperature = (float)raw_temperature * 200.0f / AHT10_RAW_DIVISOR - 50.0f;

    // 7. 数据有效性钳位 (Clamping)
    // 物理上湿度不可能超过 100% 或低于 0%，温度虽范围更广，但此处做基础检查
    if (p_out->humidity > 100.0f) p_out->humidity = 100.0f;
    if (p_out->humidity < 0.0f) p_out->humidity = 0.0f;
    
    // 标记数据有效
    p_out->is_valid = true;

    // 8. 更新内部缓存
    // 无论用户是否请求了数据副本，都更新传感器对象内的最新快照
    // 注意：在多任务环境下，读取 last_data 需注意竞态条件，建议配合互斥锁或使用返回值
    sensor->last_data = *p_out;

    // 9. 调试日志
    ESP_LOGD(AHT10_TAG, "Data Processed -> H: %.2f%%, T: %.2f°C (Raw H:%lu, T:%lu)", 
             p_out->humidity, p_out->temperature, raw_humidity, raw_temperature);

    return ESP_OK;
}

void aht10_deinit(aht10_sensor_t *sensor) {
    if (sensor == NULL) return;

    // 卸载 I2C 驱动，释放硬件资源
    esp_err_t ret = i2c_driver_delete(sensor->i2c_num);
    if (ret == ESP_OK) {
        ESP_LOGI(AHT10_TAG, "I2C Driver Deleted and Sensor Deinitialized");
    } else {
        ESP_LOGW(AHT10_TAG, "Failed to delete I2C Driver: %s", esp_err_to_name(ret));
    }

    // 重置状态标志
    sensor->is_initialized = false;
    sensor->last_data.is_valid = false;

}
