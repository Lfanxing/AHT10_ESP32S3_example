这是一份关于 **ESP-IDF 日志系统 (`esp_log`)** 的完整总结，涵盖了从基础用法、核心机制、配置陷阱到最佳实践的所有关键点。

---

### 1. 输出层级机制：双重过滤, Component config => Log中设置

ESP-IDF 的日志输出受**两层限制**，必须同时满足才能打印：

| **硬上限** | `CONFIG_LOG_MAXIMUM_LEVEL`(Maximum log verbosity) | **天花板**。决定代码中是否保留 Debug/Verbose 级别的代码。如果设得太低，相关代码会被编译器直接剔除。 | `menuconfig`重新编译 | **编译时**(必须重编) |
| **软默认** | `CONFIG_LOG_DEFAULT_LEVEL`(Default log level) | **默认层级**。程序启动时的默认过滤级别。 | `menuconfig`或 `esp_log_level_set()` | **运行时**(可动态调整) |

> **公式**：`实际打印` = `(当前Tag级别 <= 硬上限)` **AND** `(当前Tag级别 <= 当前运行时设定级别)`

#### Level Settings
  **Enable dynamic log level changes at runtime**
  `开启`此项, 允许在运行时使用`esp_log_level_set()`调整层级高度

### 2. 基本用法

#### A. 定义 Tag

每个源文件通常定义一个静态常量字符串作为标签（Tag），用于区分日志来源。

```c
static const char *TAG = "MY_MODULE"; // 建议与文件名或功能模块对应
#define TAG "MY_MODULE"                 //也可以用宏定义直接传入字符串常量
```

#### B. 打印宏

根据严重程度选择宏，自动添加时间戳、Tag 和级别前缀。

```c
ESP_LOGE(TAG, "Error message");   // Error (红色)
ESP_LOGW(TAG, "Warning message"); // Warning (黄色)
ESP_LOGI(TAG, "Info message");    // Info (绿色，默认开启)
ESP_LOGD(TAG, "Debug message");   // Debug (需开启 Debug 级别)
ESP_LOGV(TAG, "Verbose message"); // Verbose (最详细，需开启 Verbose 级别)
```

#### C. 条件打印 (减少开销)

在循环中打印大量日志时，先检查级别，避免不必要的字符串格式化开销。

```c
if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
    ESP_LOGD(TAG, "Expensive debug data: %d", calculate_value());
}
```

---

### 3. 动态控制日志级别 (`esp_log_level_set`)

这是调试的神器，允许在运行时单独开启某个模块的调试日志，而无需全局嘈杂。
  **需要开启**`Component config => Log => Level Settings => Enable dynamic log level changes at runtime`

- **函数原型**：
  ```c
  void esp_log_level_set(const char *tag, esp_log_level_t level);
  ```
- **常用场景**：
  1.  **单独调试**：只开启 `"AHT10"` 的 Debug，其他保持 Info。
      ```c
      esp_log_level_set("AHT10", ESP_LOG_DEBUG);
      ```
  2.  **全局调试**：临时开启所有模块的 Debug。
      ```c
      esp_log_level_set(ESP_LOG_DEFAULT_TAG, ESP_LOG_DEBUG);
      ```
  3.  **静默模式**：关闭某个吵闹模块的日志。
      ```c
      esp_log_level_set("NOISY_MODULE", ESP_LOG_NONE);
      ```

- **⚠️ 关键前提**：
  调用此函数前，必须确保 `Log => Log Level` 中的 **`Maximum log verbosity`** 已经设置为至少 `Debug`，否则函数调用无效（因为底层代码已被编译剔除）。

---

### 4. 常见配置陷阱 (Troubleshooting)

如果你发现 `ESP_LOGD` 不打印，请按以下顺序检查：

1.  **检查硬上限 (最常见)**：
    - 运行 `idf.py menuconfig` -> `Component config` -> `Log`。
    - 确认 **`Maximum log verbosity`** 是否为 `Debug` 或 `Verbose`？
    - _如果是 `Info`，即使你调用了 `esp_log_level_set(..., DEBUG)` 也没用。_

2.  **检查 Tag 匹配**：
    - `esp_log_level_set("AHT10", ...)` 中的字符串必须与代码中 `static const char *TAG = "AHT10"` **完全一致**（区分大小写）。

3.  **检查调用时机**：
    - `esp_log_level_set` 必须在第一条目标日志打印**之前**执行。通常放在 `app_main` 开头或模块初始化函数的第一行。

4.  **检查编译状态**：
    - 修改了 `menuconfig` 后，必须执行 `idf.py build` 重新编译。仅烧录 (`flash`) 是不够的，因为日志级别表是在编译时生成的。

---

### 5. 性能与最佳实践

- **生产环境策略**：
  - **`Maximum log verbosity`**: 设为 `Warning` 或 `Error`。
    - _好处_：显著减小固件体积（移除所有 D/V 代码），提高运行速度，避免敏感信息泄露。
  - **`Default log level`**: 设为 `Warning`。
- **开发环境策略**：
  - **`Maximum log verbosity`**: 设为 `Verbose`。
  - **`Default log level`**: 设为 `Info`。
  - **动态调试**: 在代码中针对特定难解 Bug 的模块使用 `esp_log_level_set` 临时开启 `Debug`。
- **避免在 ISR (中断服务程序) 中使用**：
  - 虽然 `ESP_LOGx` 在中断中通常可用，但频繁的串口输出会严重干扰中断时序。在中断中尽量只记录关键错误标志，留待主循环处理打印。
- **Bootloader 日志**：
  - Bootloader 的日志级别是独立配置的 (`Bootloader log verbosity`)，不受 App 的 `esp_log_level_set` 影响。

### 6. 速查命令表

| 需求                       | 操作                                                 | 是否需重编      |
| :------------------------- | :--------------------------------------------------- | :-------------- |
| 临时看某模块 Debug 日志    | 代码中加入 `esp_log_level_set("TAG", ESP_LOG_DEBUG)` | 否 (只需重烧录) |
| 永久开启全局 Debug         | `menuconfig`: Default=Debug, Max=Debug               | **是**          |
| 减小固件体积               | `menuconfig`: Max=Warning                            | **是**          |
| 解决 "调用了 set 但没反应" | 检查 `menuconfig` 中的 **Maximum log verbosity**     | **是**          |

掌握这套逻辑，你就能游刃有余地控制 ESP32 的任何日志输出了！
