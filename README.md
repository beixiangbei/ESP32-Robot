# MOSS ESP32 桌面机器人

一个以 ESP32-S3 为硬件控制核心、外形参考《流浪地球》MOSS 的桌面具身机器人项目。

项目将摄像头、双轴云台、OLED、LED、麦克风、扬声器和电池管理封装为局域网 HTTP API。后续由一台 24 小时运行的 HPT630 微型主机承载 Agent 网关，并接入云端 ASR、TTS、视觉和大语言模型。

> 当前阶段重点是硬件安全、低功耗、可配置性和本地控制。云端 Agent 与完整语音对话尚未接入。

## 当前状态

### 已实现

- ESP32-S3 Arduino 固件与 PlatformIO 构建
- OV5640 摄像头启停、拍照和预览
- 28BYJ-48 双轴步进电机云台
- 可中断运动、队列取消和运动结束线圈释放
- pan/tilt 软限位、方向反转和中心校准
- 电机配置保存到版本化 NVS，重启后保留
- SSD1306 OLED 文字、旋转和绘图
- 两组 WS2812 LED 及静态、闪烁、呼吸、彩虹、脉冲效果
- I2S 扬声器测试音和麦克风音量读取
- 电池电压、电量和充电状态检测
- Wi-Fi、NTP、系统状态、自检、调试和重启接口
- 固件内嵌的云台校准页面
- 中文 Apple 风格完整控制台交互原型
- WSL Docker 固件构建和纯 C++ 主机测试

### 等待硬件验证

- 电机停止响应、丢步和线圈释放后的实际电流
- pan/tilt 真实安全行程与方向
- NVS 配置在实机重启后的恢复
- 10000mAh 电池的各状态功耗和实际续航
- 深度睡眠定时唤醒
- ESP32 模组是否确认为 N16R8
- 手机浏览器控制页面布局

### 规划中

- 四级电源状态：Active、Idle、Light Sleep、Deep Sleep
- AP 配网、管理员认证和完整设备配置
- OTA、回滚和恢复出厂
- 本地规则引擎
- HPT630 Agent 网关
- 云端流式 ASR、TTS、LLM 和视觉服务
- 本地唤醒词与对话打断

详细进度见 [docs/PROGRESS.md](docs/PROGRESS.md)，完整路线图见 [docs/PROJECT_PLAN.md](docs/PROJECT_PLAN.md)。

## 系统架构

```text
云端 ASR / TTS / LLM / Vision
              |
HPT630 Agent Gateway（计划 24 小时运行）
  对话、工具调用、供应商适配、审计和可观测性
              |
       局域网 HTTP / WebSocket
              |
ESP32-S3 硬件控制器
  电源、电机、摄像头、OLED、LED、音频和本地控制
```

职责边界：

- ESP32 负责确定性的硬件控制、安全限位、休眠和离线控制。
- HPT630 负责云端凭据、Agent、对话状态、ASR/TTS 流和模型工具。
- 云服务只负责推理，任何供应商都不直接绑定在固件中。

## 硬件

| 组件 | 型号 | 接口 |
|---|---|---|
| 主控 | ESP32-S3-WROOM-1-N16R8（待实机复核） | 16MB Flash / 8MB PSRAM |
| 摄像头 | OV5640 | DVP |
| 云台 | 2 x 28BYJ-48 + ULN2003 | 74HC595 |
| 灯光 | 3 + 2 颗 WS2812 | GPIO 48 / 7 |
| 屏幕 | SSD1306 128x64 OLED | I2C |
| 麦克风 | I2S 数字麦克风 | I2S |
| 扬声器 | I2S 功放模块 | I2S |
| 电池 | 10000mAh，具体电源链路待记录 | ADC / 充电检测 |

当前没有物理限位开关或位置编码器。云台坐标是固件累计的估算值，手动拨动、堵转、丢步或重启都可能使位置失真。软件限位不能替代物理原点传感器。

## 快速开始

### 1. 获取代码

```powershell
git clone https://github.com/beixiangbei/ESP32-Robot.git
cd ESP32-Robot
```

### 2. 配置本地 Wi-Fi

真实凭据不进入 Git：

```powershell
Copy-Item include\secrets.example.h include\secrets.h
```

编辑 `include/secrets.h`：

```cpp
#define MOSS_WIFI_SSID "your-wifi-name"
#define MOSS_WIFI_PASSWORD "your-wifi-password"
```

`include/secrets.h` 已被 Git 忽略。没有该文件时固件仍可编译，并在 Wi-Fi 未配置或连接超时后继续离线启动。

旧版本曾提交过真实 Wi-Fi 密码，该密码必须更换，不能继续使用。

### 3. 使用 WSL Docker 构建

公司开发环境使用 WSL2 内的原生 Docker Engine，不要求 Windows Docker Desktop 或 Docker Compose：

```powershell
.\scripts\firmware-build.ps1
```

构建产物：

```text
.pio/build/esp32-s3-devkitc-1/firmware.bin
```

当前基线：

```text
PlatformIO Core 6.1.18
espressif32 7.0.1
RAM 约 15.6%
Flash 约 28.4%
```

第一次构建会下载镜像、ESP32 工具链和依赖，后续使用 `moss-platformio-cache` Docker volume。

### 4. 运行主机测试

```powershell
.\scripts\host-test.ps1
```

当前覆盖云台边界、越界拒绝、相对位置溢出、非法轴、负绝对位置和方向反转。

### 5. 烧录和串口监控

硬件在本机连接时可使用 PlatformIO：

```powershell
pio run --target upload
pio device monitor
```

串口波特率为 `115200`。公司环境只负责构建和主机测试，烧录与功耗验证在硬件所在地完成。

## 控制界面

### 固件内页面

ESP32 联网后访问：

```text
http://<ESP32-IP>/
```

当前固件内页面主要用于云台校准，支持：

- `±10`、`±50` 步点动
- 停止并释放电机线圈
- 捕获当前位置为最小值、中心或最大值
- 数值修改 min/center/max
- 电机方向反转
- 保存到 NVS

### 完整控制台原型

公司环境可启动本地 Mock：

```powershell
python tools\control_panel_preview.py
```

浏览器访问：

```text
http://127.0.0.1:8766
```

原型包含：

- 系统总览
- 云台控制
- 摄像头
- OLED 与 LED
- 语音与 Agent
- 自动化规则
- 网络、电源、Agent 网关与安全设置

完整原型用于确认交互和信息架构。除电机模块外，其余页面需要随着对应固件 API 落地后逐步接入，不能视为已经完成的设备功能。

## 电机安全与校准

开发默认范围：

| 轴 | 最小值 | 中心 | 最大值 |
|---|---:|---:|---:|
| pan | -1024 | 0 | 1024 |
| tilt | -256 | 0 | 256 |

这些值只用于拦截异常大命令，不是经过测量的机械安全边界。

推荐实机校准流程：

1. 停止并释放电机。
2. 手动把 MOSS 头部摆到自然中心。
3. 点击“设为中心”。
4. 使用控制页点动到安全最小边界并点击“设为最小”。
5. 点动到安全最大边界并点击“设为最大”。
6. 保存后重启，确认 NVS 配置仍然有效。

手动拨动只能用于重新定义中心。寻找边界时应使用电机点动，否则 ESP32 无法知道手动移动了多少步。

更多说明见 [docs/MOTOR_CONTROL.md](docs/MOTOR_CONTROL.md)。

## HTTP API

主要接口：

```text
GET  /api/v1/ping
GET  /api/v1/status

GET  /api/v1/motor/status
POST /api/v1/motor/relative
POST /api/v1/motor/absolute
POST /api/v1/motor/stop
POST /api/v1/motor/calibrate
GET  /api/v1/motor/config
POST /api/v1/motor/config
POST /api/v1/motor/limit/capture

POST /api/v1/camera/config
GET  /api/v1/camera/capture
GET  /api/v1/camera/preview

GET  /api/v1/led/status
POST /api/v1/led/effect
POST /api/v1/led/segments

POST /api/v1/oled/text
POST /api/v1/oled/clear
POST /api/v1/oled/rotation
POST /api/v1/oled/draw

POST /api/v1/audio/speaker/test
GET  /api/v1/audio/mic/test
GET  /api/v1/audio/status
```

接口细节见 [API.md](API.md)。API 当前没有认证，只能用于可信局域网，不应将 ESP32 的 HTTP 端口暴露到公网。

## 功耗计划

续航问题是当前最高优先级之一。已确认的主要优化方向：

- 电机运动完成、停止和异常时释放线圈
- 摄像头按需上电并自动断电
- Wi-Fi modem sleep
- OLED 超时熄屏
- LED 电池模式亮度限制
- I2S 驱动按需初始化和卸载
- Active、Idle、Light Sleep、Deep Sleep 四级状态机
- 低电量分级限制和自动休眠
- 修复深度睡眠定时唤醒

所有优化必须通过实际电流测量验收。测量矩阵与测试步骤见 [docs/HARDWARE_TEST_LOG.md](docs/HARDWARE_TEST_LOG.md)。

## 两地开发流程

硬件位于出租屋，白天通常在公司开发：

### 公司

- 开发协议、配置、控制台、Agent 网关和纯逻辑测试
- 使用 WSL Docker 编译固件
- 将硬件相关任务标记为 `needs-hardware`
- 离开前更新 `docs/PROGRESS.md` 并推送 GitHub

### 出租屋

- 拉取相同提交并确认 commit hash
- 按 `docs/HARDWARE_TEST_LOG.md` 执行准备好的测试
- 记录电压、电流、持续时间、串口日志和异常
- 测试结果提交到 GitHub，供公司继续开发

## 项目文档

| 文档 | 内容 |
|---|---|
| [docs/PROJECT_PLAN.md](docs/PROJECT_PLAN.md) | 完整路线图和阶段验收 |
| [docs/PROGRESS.md](docs/PROGRESS.md) | 当前进度和跨地点交接 |
| [docs/HARDWARE_TEST_LOG.md](docs/HARDWARE_TEST_LOG.md) | 实机和功耗测试记录 |
| [docs/MOTOR_CONTROL.md](docs/MOTOR_CONTROL.md) | 云台限位与校准 |
| [docs/BUILD.md](docs/BUILD.md) | WSL Docker 构建 |
| [docs/AI_STACK_SELECTION.md](docs/AI_STACK_SELECTION.md) | Agent 与云服务选型 |
| [API.md](API.md) | HTTP API 参考 |

## 安全说明

- 不要提交 Wi-Fi 密码、云服务密钥、设备令牌或真实家庭网络信息。
- 当前 HTTP API 没有鉴权，只允许在可信局域网使用。
- 所有电机命令必须经过固件中央限位控制器。
- 软件限位不能替代物理限位开关、原点传感器或编码器。
- 云服务失败必须降级，不能使电机、音频或摄像头任务永久运行。

## License

MIT
