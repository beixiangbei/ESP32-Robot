# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 desktop embodied agent robot using PlatformIO with Arduino framework. Controls camera, dual stepper motors (via 74HC595 shift register), dual WS2812 LED strips (3 LEDs + 2 LEDs), and I2S audio.

## Build Commands

```bash
pio run              # Compile firmware
pio run --target upload    # Compile and flash to ESP32
pio device monitor         # Open serial monitor (115200 baud)
```

## Hardware Configuration

- **MCU**: ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
- **Camera**: OV5640 via DVP interface
- **Motors**: 28BYJ-48 stepper motors + ULN2003 driven by 74HC595 shift register
- **LEDs**: WS2812 - LED0 (3 LEDs on GPIO 48), LED1 (2 LEDs on GPIO 7)
- **Audio**: I2S speaker (pins 9-11) and I2S mic (pins 12-14)

Key GPIO assignments (src/main.cpp):
- Camera: D0-D7 (42,45,46,41,16,18,17,40), XCLK(21), PCLK(15), VSYNC(38), HREF(39), PWDN(47)
- Motors: SHCP(3), STCP(4), DS(5)
- Battery: VBAT_ADC(8), USB_CHG(6)

## Architecture

### Tasks (FreeRTOS)
- **motorTask**: Async motor control via queue
- **ledTask**: LED animation via queue
- **monitorTask**: Health check, auto-recovery

### Key Structures
- `MotorState`: {pos, busy, target_pos, speed, accel}
- `LedGroup`: {effect, color, speed} per LED strip
- `MotorCommand`: Queue message for motor control
- `LedCommand`: Queue message for LED control

### HTTP API (prefix /api/v1/)

#### Motor Control
```
POST /api/v1/motor/relative   {"motor":0,"steps":100,"speed":8,"accel":true}
POST /api/v1/motor/absolute   {"motor":0,"target":500,"speed":8}
GET  /api/v1/motor/status     → {"pan":{"pos":0,"busy":false},"tilt":{"pos":0,"busy":false}}
POST /api/v1/motor/stop       {"motor":"pan"} or {"motor":"all"}
```
- motor: 0=pan (水平), 1=tilt (垂直)
- speed: 1-10, 越大越快
- accel: 是否使用加速

#### Camera
```
POST /api/v1/camera/config    {"enabled":true,"resolution":"vga","quality":12}
GET  /api/v1/camera/capture   → image/jpeg (640x480)
GET  /api/v1/camera/preview   → image/jpeg
```
- resolution: qqvga/qqvga2/hqvga/qvga/vga
- quality: 1-63, 越小越好

#### LED Control
```
POST /api/v1/led/effect       {"target":0,"effect":"rainbow","color":"FF0000","speed":50}
POST /api/v1/led/segments     {"segments":[{"id":0,"color":"#FF0000"},{"id":1,"color":"#00FF00"}]}
GET  /api/v1/led/status       → {"0":{"effect":"static","color":"FF0000"},"1":{"effect":"off"}}
```
- target: 0=LED0(3灯), 1=LED1(2灯), -1=全部
- effect: off/static/blink/breath/rainbow/pulse
- speed: 1-100

#### OLED Display (SSD1306 128x64, I2C on GPIO 1,2)
```
POST /api/v1/oled/text       {"text":"Hello","size":2,"x":0,"y":0,"line":0}
POST /api/v1/oled/clear
POST /api/v1/oled/rotation   {"rotation":1}
GET  /api/v1/oled/status
```
- size: 1-8 (字体倍数)
- line: 行号 0-7, 设置后 x,y 被忽略
- rotation: 0=0°, 1=90°, 2=180°, 3=270°
- **注意**: OLED与摄像头共享I2C总线，同时开启时会自动重置I2C解决冲突

#### Audio (I2S speaker + microphone)
```
POST /api/v1/audio/speaker/test  → 播放1kHz测试音
GET  /api/v1/audio/mic/test     → {"ok":true,"mic":true,"level":0-255}
GET  /api/v1/audio/status       → {"speaker":true,"mic":true}
```

#### System
```
GET  /api/v1/status            → Full system status JSON
POST /api/v1/control/selfcheck → Component health
POST /api/v1/control/reboot   → Soft reset
GET  /api/v1/ping             → {"ok":true}
GET  /api/v1/debug            → Internal state (for debugging)
```

### LED Effects
- off/static/blink/breath/rainbow/pulse
- target: 0 (LED0, 3 LEDs), 1 (LED1, 2 LEDs), -1 (both)

## WiFi Configuration

Edit `src/main.cpp` lines 14-15 to set `WIFI_SSID` and `WIFI_PASSWORD`.

## Notes

- JSON parsing uses `jsonGetInt()` and `jsonGetStr()` helper functions, NOT sscanf
- Motor commands are processed asynchronously - check `busy` field to wait for completion
- Low battery warning: LED flashes red when < 20% and not charging
- OLED and Camera share I2C bus (GPIO 1=SDA, GPIO 2=SCL). API handlers automatically call `Wire.end()/Wire.begin()` to reset I2C before OLED operations, allowing both to coexist.
- Audio uses I2S (GPIO 9-14) which does not conflict with I2C OLED/Camera.
