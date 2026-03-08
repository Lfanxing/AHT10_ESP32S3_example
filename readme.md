
# AHT10 ESP32-S3 传感器驱动示例

本项目是一个基于 **ESP-IDF (Espressif IoT Development Framework)** 的 AHT10 温湿度传感器驱动示例。它展示了如何在 ESP32-S3 平台上通过 I2C 接口读取 AHT10/AHT15 传感器的温度和湿度数据。

该驱动采用**面向对象**的 C 语言风格封装，具有良好的可移植性和错误恢复机制，适合直接集成到各类物联网项目中。

## 📋 项目特性

- ✅ **支持芯片**: 基于ESP32-S3N16R8芯片。
- ✅ **传感器支持**: AHT10 (其余同系型号未经验证，可尝试)。
- ✅ **标准接口**: 遵循 ESP-IDF 组件规范，易于移植。
- ✅ **实时日志**: 通过 `esp_log` 输出实时温湿度数据及状态。

## 🛠️ 硬件准备

### 1. 硬件清单

- **开发板**: ESP32-S3N16R8
- **传感器模块**: AHT10 模块
-

### 2. 引脚连接 (默认配置)

请在代码中确认 `aht10_sensor.h` 或 `aht10_sensor.c` 中的 I2C 引脚定义。通常默认配置如下（基于 ESP32-S3）：

| AHT10 引脚 | ESP32-S3 引脚 | 说明                        |
| :--------- | :------------ | :-------------------------- |
| **VCC**    | 3.3V          | 电源 (严禁接 5V)            |
| **GND**    | GND           | 接地                        |
| **SDA**    | GPIO 8        | I2C 数据线 (可根据需要修改) |
| **SCL**    | GPIO 9        | I2C 时钟线 (可根据需要修改) |

> ⚠️ **注意**: AHT10 是 3.3V 逻辑电平设备。如果你的开发板是 5V 系统，请确保使用电平转换器。

## 🚀 快速开始

### 1. 环境要求

- **ESP-IDF**: v4.4 或更高版本 (推荐 v5.0+)

### 2. 编译与烧录
直接解压，运行 **"编译、烧录和监视"**

### 3. 预期输出

成功运行后，串口终端应显示类似以下内容：

```text
I (345) MAIN: Starting AHT10 Sensor Demo on ESP32-S3
I (450) AHT10: Sensor initialized successfully
I (550) MAIN: Initialization successful. Starting loop...
I (2050) MAIN: Humidity: 45.20 %  Temperature: 26.50 C
I (3550) MAIN: Humidity: 45.15 %  Temperature: 26.48 C
...
```

## 📦 项目结构

```text
.
├── components/
│   └── AHT10/              # 核心驱动组件
│       ├── aht10_sensor.c  # 驱动实现
│       ├── aht10_sensor.h  # 头文件与 API 定义
│       └── CMakeLists.txt  # 组件构建配置
├── main/
│   ├── main.c              # 应用主程序 (演示用法)
│   └── CMakeLists.txt
├── sdkconfig               # 项目配置文件
└── readme.md               # 本说明文档
```

## 🔌 移植指南 (如何集成到你的项目)

如果你想在现有的 ESP-IDF 项目中使用此驱动，请按以下步骤操作：

### 第一步：复制组件

将 `components/AHT10` 整个文件夹复制到你的项目根目录下的 `components` 文件夹中。

> 如果你的项目没有 `components` 文件夹，请新建一个。

### 第二步：注册依赖

打开你项目中 **主程序所在组件** (通常是 `main`) 的 `CMakeLists.txt`，添加对 `AHT10` 组件的依赖。

**修改前：**

```cmake
idf_component_register(SRCS "main.c" INCLUDE_DIRS ".")
```

**修改后：**

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES AHT10  # 添加这一行,或者在原有的REQUIRES后面加上AHT10（用空格间隔）

)
```
_注：根据 ESP-IDF 规范，组件名通常由文件夹名决定。_

### 第三步：修改代码

在你的 `.c` 文件中引入头文件并调用 API：
#### 调用说明
先创建aht10_sensor_t实例（必要）
如果需要单独的数据容器，还可创建aht10_data_t实例（可选）,如aht10_data_t mydata;
读取数据时**esp_err_t aht10_read**`**(aht10_sensor_t *sensor, aht10_data_t *data)中的data参数,
可传入mydata作为数据容器,此时会同时更新`1sensor->last_data`(`aht10_data_t类型`)和`mydata`(`aht10_data_t类型`)的值
也可参数可传入NULL，此时只更新`1sensor->last_data`的值, 需要在完成读取后，访问`1sensor->last_data`,获取数据
aht10_data_t类型数据下,有
```c
    float humidity;   // 湿度 %RH
    float temperature;// 温度 ℃
```c
两个成员,访问这两个成员即可获取数据.

```c
#include "aht10_sensor.h" // 直接引用，无需路径前缀

void app_main(void) {
    aht10_sensor_t sensor;
    aht10_data_t data;

    // 1. 初始化
    if (aht10_init(&sensor) != ESP_OK) {
        // 处理初始化失败
        return;
    }

    // 2. 循环读取
    while(1) {
        if (aht10_read(&sensor, &data) == ESP_OK && data.is_valid) {
            printf("Temp: %.2f, Humi: %.2f\n", data.temperature, data.humidity);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 第四步：自定义 I2C 引脚 (可选)

如果默认的 I2C 引脚不符合你的硬件设计，请编辑 `components/AHT10/aht10_sensor.c` (或在头文件中查找宏定义)，修改以下配置：

```c
// 在 aht10_sensor.c 或 .h 中找到类似定义
#define AHT10_I2C_SCL_GPIO           9      // 修改为你的 SCL 引脚
#define AHT10_I2C_SDA_GPIO           8      // 修改为你的 SDA 引脚
#define AHT10_I2C_NUM              I2C_NUM_0 // 使用的 I2C 端口号
```

## ⚠️ 注意事项

1. **上电时间**: AHT10 上电后需要至少 **100ms** 的稳定时间才能进行通信。驱动内部已处理，但若外部复位传感器需注意。
2. **读取频率**: 官方建议两次读取间隔不小于 **750ms** (推荐 1s 以上)，过于频繁的读取可能导致数据不变或传感器过热。代码中已设置为 1.5s。
3. **供电**: 务必使用 **3.3V** 供电。虽然部分模块标称支持 5V，但为了通信稳定，强烈建议使用 3.3V。


---

### 💡 提示

（存疑）如果你的 `CMakeLists.txt` 中组件名大小写敏感导致找不到组件，请将 `REQUIRES AHT10` 改为 **`REQUIRES aht10`** (全小写)，这是 ESP-IDF 的标准命名习惯。
