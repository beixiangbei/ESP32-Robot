# ESP32-S3 桌面具身 Agent 机器人

## 项目愿景

让 AI Agent 拥有"身体"——一个可被 AI 控制的物理交互平台。通过 HTTP REST API，AI Agent 可以感知环境（摄像头、麦克风）、表达意图（屏幕、扬声器、LED）、执行动作（云台电机），成为真正的具身智能助手。

### 典型应用场景

- **AI 伴侣**: AI 通过摄像头识别人脸、追踪人体，屏幕显示表情变化，与人自然对话
- **智能管家**: 与智能家居联动，识别主人回家、主动问候；听到异常声音时提醒
- **语音交互**: 内置麦克风和扬声器，支持语音对话、语音助手集成
- **生物识别**: 声纹识别确认身份、人脸识别追踪特定用户

## 硬件配置

### 当前支持的硬件

| 组件 | 型号 | 接口 | 数量 |
|------|------|------|------|
| 主控 | ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM) | - | 1 |
| 摄像头 | OV5640 | DVP | 1 |
| 电机 | 28BYJ-48 + ULN2003 | 74HC595 移位寄存器 | 2 (云台) |
| LED | WS2812 | GPIO | 5 (3灯+2灯) |
| OLED | SSD1306 128x64 | I2C | 1 |
| 麦克风 | I2S 数字麦克风 | I2S | 1 |
| 扬声器 | I2S 功放模块 | I2S | 1 |
| 电池 | 3.7V LiPo (通过 USB 充电) | - | 1 |

### GPIO 引脚分配

```
摄像头 (DVP 接口):
  D0-D7:  42, 45, 46, 41, 16, 18, 17, 40
  XCLK:   21
  PCLK:   15
  VSYNC:  38
  HREF:   39
  PWDN:   47
  SDA:    1 (与 OLED 共用)
  SCL:    2 (与 OLED 共用)

电机 (74HC595 移位寄存器):
  SHCP:   3
  STCP:   4
  DS:     5

LED:
  LED0:   48 (3 颗 WS2812)
  LED1:   7  (2 颗 WS2812)

音频 (I2S):
  扬声器: BCK=10, WS=9, DOUT=11
  麦克风: BCK=12, WS=14, DIN=13

电池:
  VBAT:   8 (ADC)
  USB_CHG: 6

OLED (I2C):
  SDA:    1
  SCL:    2
  地址:   0x3C
```

## 功能特性

### 已实现

- [x] **摄像头**: JPEG 图像采集、自动曝光
- [x] **云台控制**: 水平(pan) + 垂直(tilt) 双轴步进电机，支持相对/绝对位置控制
- [x] **OLED 屏幕**: 显示文字、清除、旋转(0°/90°/180°/270°)
- [x] **LED 效果**: 静态颜色、呼吸灯、彩虹、呼吸、闪烁、脉冲
- [x] **扬声器**: 1kHz 测试音播放（可扩展 TTS、语音播报）
- [x] **麦克风**: 环境音量检测 (0-255)
- [x] **电池监测**: 电压、百分比、充电状态
- [x] **WiFi 控制**: HTTP REST API，AP 模式可配置

### 规划中

- [ ] 人脸识别追踪 (固件或云端)
- [ ] 语音识别集成 (ASR)
- [ ] TTS 文字转语音
- [ ] 声纹识别
- [ ] 语音变声
- [ ] OTA 无线升级
- [ ] 蓝牙配网

## HTTP API

所有接口前缀 `/api/v1/`

### 系统

```bash
# 健康检查
GET  /ping              → {"ok":true}

# 系统状态
GET  /status            → {"version","uptime","memory","wifi","motor","camera","led","oled","battery"}

# 组件自检
POST /control/selfcheck → {"motor","camera","led","oled","memory"}

# 重启设备
POST /control/reboot

# 调试信息
GET  /debug            → {"motorQueue","ledQueue","m0_pos","m0_busy"...}
```

### 摄像头

```bash
# 配置摄像头
POST /camera/config     → {"enabled":true,"resolution":"vga","quality":12}
Body: {"enabled":bool,"resolution":"qqvga|qqvga2|hqvga|qvga|vga","quality":1-63}

# 拍照 (返回 JPEG)
GET  /camera/capture    → image/jpeg

# 实时预览 (返回 JPEG)
GET  /camera/preview    → image/jpeg
```

### 电机

```bash
# 查询状态
GET  /motor/status      → {"pan":{"pos":0,"busy":false},"tilt":{"pos":0,"busy":false}}

# 相对移动
POST /motor/relative    → {"motor":0,"steps":100,"speed":8,"accel":true}
Body: {"motor":0|1,"steps":±N,"speed":1-10,"accel":bool}
- motor: 0=水平(pan), 1=垂直(tilt)
- steps: 正数顺时针，负数逆时针

# 绝对移动
POST /motor/absolute    → {"motor":0,"target":500,"speed":8}
Body: {"motor":0|1,"target":N,"speed":1-10}

# 停止
POST /motor/stop        → {"motor":"pan"} or {"motor":"all"}
Body: {"motor":"pan"|"tilt"|"all"}
```

### OLED 屏幕

```bash
# 显示文字
POST /oled/text         → {"ok":true,"text":"Hello","size":2}
Body: {"text":"字符串","size":1-8,"x":0,"y":0,"line":0}
- size: 字体倍数 (1=最小, 8=最大)
- line: 行号 0-7，设置后 x,y 被忽略
- 屏幕 128x64，字号 1 时每行可显示 21 字符

# 清除屏幕
POST /oled/clear

# 旋转屏幕
POST /oled/rotation     → {"ok":true,"rotation":1}
Body: {"rotation":0-3}  # 0=0°, 1=90°, 2=180°, 3=270°

# 屏幕状态
GET  /oled/status       → {"width":128,"height":64,"rotation":0}
```

### LED

```bash
# 查询状态
GET  /led/status        → {"0":{"effect":"static","color":"FF0000"},"1":{"effect":"off"}}

# 设置效果
POST /led/effect        → {"led":0,"effect":"rainbow","speed":50}
Body: {"target":0|1|-1,"effect":"off|static|blink|breath|rainbow|pulse","color":"FF0000","speed":1-100}
- target: 0=LED0(3灯), 1=LED1(2灯), -1=全部

# 分段控制
POST /led/segments      → {"segments":[{"id":0,"color":"FF0000"},{"id":1,"color":"00FF00"}]}
Body: {"segments":[{"id":0|1,"color":"#RRGGBB或RRGGBB"}]}
```

### 音频

```bash
# 扬声器测试 (播放 1kHz 音调)
POST /audio/speaker/test → {"ok":true,"speaker":true}

# 麦克风测试 (读取音量)
GET  /audio/mic/test    → {"ok":true,"mic":true,"level":0-255}

# 音频状态
GET  /audio/status      → {"speaker":true,"mic":true}
```

## AI Agent 集成示例

### Python 调用示例

```python
import requests
import json

BASE_URL = "http://192.168.31.220"

class ESP32Robot:
    def __init__(self, host):
        self.host = host

    def status(self):
        return requests.get(f"{self.host}/api/v1/status").json()

    def speak(self, text=None):
        """播放测试音 (可扩展 TTS)"""
        requests.post(f"{self.host}/api/v1/audio/speaker/test")

    def listen(self):
        """读取麦克风音量"""
        r = requests.get(f"{self.host}/api/v1/audio/mic/test")
        return r.json()["level"]

    def show_face(self, expression):
        """显示表情"""
        faces = {
            "happy": "^-^",
            "sad":   "T_T",
            "think": "-_-",
            "surprise": ">_<"
        }
        text = faces.get(expression, expression)
        requests.post(f"{self.host}/api/v1/oled/text",
                      json={"text": text, "size": 3})

    def look_at(self, pan, tilt):
        """控制摄像头朝向"""
        requests.post(f"{self.host}/api/v1/motor/absolute",
                      json={"motor": 0, "target": pan, "speed": 8})
        requests.post(f"{self.host}/api/v1/motor/absolute",
                      json={"motor": 1, "target": tilt, "speed": 8})

    def capture(self):
        """拍照"""
        r = requests.get(f"{self.host}/api/v1/camera/capture")
        return r.content

# 使用
robot = ESP32Robot("http://192.168.31.220")
robot.show_face("happy")
robot.look_at(500, 200)
img = robot.capture()
```

## 配置

### WiFi 配置

编辑 `src/main.cpp`:

```cpp
const char* WIFI_SSID = "你的WiFi名称";
const char* WIFI_PASSWORD = "你的WiFi密码";
```

### 硬件连接注意事项

1. **I2C 总线共享**: OLED 和摄像头 SCCB 共用 GPIO 1(SDA) 和 GPIO 2(SCL)，固件自动处理冲突
2. **I2S 音频**: 独立于 I2C，不产生冲突
3. **电机驱动**: 使用 74HC595 移位寄存器，仅需 3 个 GPIO

## 编译与烧录

### 环境要求

- [PlatformIO Core](https://platformio.org/install/cli) 或 PlatformIO IDE (VS Code 插件)
- Python 3.8+
- ESP32 工具链 (PlatformIO 自动安装)

### 构建命令

```bash
# 安装依赖
pio pkg install

# 编译
pio run

# 烧录 (连接 ESP32)
pio run --target upload

# 串口监视器 (115200 baud)
pio device monitor
```

### 分区配置

固件较大，使用 `huge_app.csv` 分区表：

| 分区 | 大小 | 用途 |
|------|------|------|
| nvs | 32KB | WiFi 配置 |
| otadata | 8KB | OTA 数据 |
| app0 | 3MB | 主程序 |
| spiffs | 1MB | 文件系统 |

## 后续扩展指南

### 添加语音识别 (ASR)

1. 集成 WebSocket 流式音频到云端 ASR 服务
2. 或使用本地离线 ASR 模型 (如 Resistrant)

### 添加 TTS

1. 使用云端 TTS API 生成音频
2. 或集成嵌入式 TTS 芯片 (如 JQ8900)

### 添加声纹识别

1. 录制音频样本到服务器
2. 提取声纹特征 (MFCC)
3. 与数据库比对验证身份

### 添加本地人脸识别

1. 使用 ESP32-S3 的向量处理能力
2. 或将图像发送到本地 NAS/服务器处理
3. 使用 FaceNet 或 ArcFace 模型

## 许可证

MIT License

## 项目地址

https://github.com/beixiangbei/ESP32-Robot
