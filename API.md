# ESP32-S3 机器人 HTTP 接口说明

## 设备信息
- **IP**: `192.168.31.220`
- **端口**: 80
- **协议**: HTTP/JSON

---

## 一、接口列表

### 1. GET 接口

| 接口 | 说明 | 返回示例 |
|------|------|----------|
| `/api/v1/ping` | 连通性测试 | `{"ok":true}` |
| `/api/v1/status` | 完整系统状态 | JSON 对象 |
| `/api/v1/motor/status` | 电机位置状态 | `{"pan":{"pos":0,"busy":false},"tilt":{"pos":0,"busy":false}}` |
| `/api/v1/led/status` | LED 状态 | `{"0":{"effect":"static","color":"FF0000"},"1":{"effect":"static","color":"00FF00"}}` |
| `/api/v1/debug` | 内部调试信息 | JSON 对象 |
| `/api/v1/camera/capture` | 抓拍图片 | JPEG 二进制 |

---

### 2. POST 接口

#### 电机控制

| 接口 | Body 示例 | 说明 |
|------|-----------|------|
| `/api/v1/motor/relative` | `{"motor":0,"steps":50,"speed":8}` | 相对移动 |
| `/api/v1/motor/absolute` | `{"motor":1,"target":100,"speed":8}` | 绝对移动 |
| `/api/v1/motor/stop` | `{"motor":"all"}` | 停止电机 |

#### LED 控制

| 接口 | Body 示例 | 说明 |
|------|-----------|------|
| `/api/v1/led/effect` | `{"target":0,"effect":"static","color":"FF0000"}` | 设置效果 |
| `/api/v1/led/segments` | `{"segments":[{"id":0,"color":"FF0000"},{"id":1,"color":"00FF00"}]}` | 分段控制 |

#### 摄像头

| 接口 | Body 示例 | 说明 |
|------|-----------|------|
| `/api/v1/camera/config` | `{"enabled":true}` | 开启/关闭摄像头 |

#### 系统

| 接口 | Body 示例 | 说明 |
|------|-----------|------|
| `/api/v1/control/selfcheck` | `{}` | 自检 |
| `/api/v1/control/reboot` | `{"reason":"test"}` | 重启 |

---

## 二、参数说明

### 电机参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `motor` | 0 / 1 | 0=pan(水平), 1=tilt(垂直) |
| `steps` | 整数 | 相对步数，正负方向 |
| `target` | 整数 | 绝对目标位置 |
| `speed` | 1-20 | 速度，越小越快 |
| `motor` (stop) | "pan" / "tilt" / "all" | 停止指定电机 |

### LED 参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `target` | 0 / 1 / -1 | 0=LED0(3灯), 1=LED1(2灯), -1=全部 |
| `effect` | off / static / blink / breath / rainbow / pulse | 效果模式 |
| `color` | 6位十六进制 | RGB颜色，如 FF0000=红 |
| `speed` | 1-100 | 动画速度 |
| `id` (segments) | 0 / 1 | LED 编号 |

### LED 效果

| 效果 | 说明 |
|------|------|
| `off` | 关闭 |
| `static` | 静态颜色 |
| `blink` | 闪烁 |
| `breath` | 呼吸灯 |
| `rainbow` | 彩虹渐变 |
| `pulse` | 脉冲 |

---

## 三、使用示例

### Windows curl 命令

```bash
# 测试连通
curl http://192.168.31.220/api/v1/ping

# 查询状态
curl http://192.168.31.220/api/v1/status
curl http://192.168.31.220/api/v1/motor/status
curl http://192.168.31.220/api/v1/led/status

# 电机控制
curl -X POST -H "Content-Type: application/json" -d "{\"motor\":0,\"steps\":100,\"speed\":8}" http://192.168.31.220/api/v1/motor/relative
curl -X POST -H "Content-Type: application/json" -d "{\"motor\":1,\"target\":100,\"speed\":8}" http://192.168.31.220/api/v1/motor/absolute
curl -X POST -H "Content-Type: application/json" -d "{\"motor\":\"all\"}" http://192.168.31.220/api/v1/motor/stop

# LED 控制
curl -X POST -H "Content-Type: application/json" -d "{\"target\":0,\"effect\":\"static\",\"color\":\"FF0000\"}" http://192.168.31.220/api/v1/led/effect
curl -X POST -H "Content-Type: application/json" -d "{\"target\":1,\"effect\":\"blink\",\"color\":\"0000FF\",\"speed\":5}" http://192.168.31.220/api/v1/led/effect
curl -X POST -H "Content-Type: application/json" -d "{\"target\":-1,\"effect\":\"rainbow\",\"speed\":30}" http://192.168.31.220/api/v1/led/effect
curl -X POST -H "Content-Type: application/json" -d "{\"target\":-1,\"effect\":\"off\"}" http://192.168.31.220/api/v1/led/effect
curl -X POST -H "Content-Type: application/json" -d "{\"segments\":[{\"id\":0,\"color\":\"FF0000\"},{\"id\":1,\"color\":\"00FF00\"}]}" http://192.168.31.220/api/v1/led/segments

# 摄像头
curl -X POST -H "Content-Type: application/json" -d "{\"enabled\":true}" http://192.168.31.220/api/v1/camera/config
curl -o photo.jpg http://192.168.31.220/api/v1/camera/capture

# 系统
curl -X POST -H "Content-Type: application/json" -d "{}" http://192.168.31.220/api/v1/control/selfcheck
curl -X POST -H "Content-Type: application/json" -d "{\"reason\":\"test\"}" http://192.168.31.220/api/v1/control/reboot
```

---

## 四、硬件配置

| 组件 | GPIO | 说明 |
|------|------|------|
| LED0 | GPIO 48 | 3 颗 WS2812 |
| LED1 | GPIO 7 | 2 颗 WS2812 |
| 电机 SHCP | GPIO 3 | 74HC595 移位寄存器 |
| 电机 STCP | GPIO 4 | 74HC595 锁存 |
| 电机 DS | GPIO 5 | 74HC595 数据 |
| 摄像头 | GPIO 42/45/46/41/16/18/17/40 | DVP 接口 |
| 电池 ADC | GPIO 8 | 电量检测 |

---

## 五、架构设计

```
┌─────────────────────────────────────────────────────────┐
│              HTTP Server (esp_http_server)                 │
│   /api/v1/motor/*  →  MotorQueue  →  motorTask         │
│   /api/v1/led/*    →  ledQueue    →  ledTask           │
│   /api/v1/camera/* →  直接调用 esp-camera 驱动            │
│   /api/v1/status   →  读取全局状态（无阻塞）              │
└─────────────────────────────────────────────────────────┘

FreeRTOS Tasks:
- motorTask   : 异步处理电机命令，带加速度曲线
- ledTask     : 持续更新 LED 动画（每个 LED 独立计时）
- monitorTask : 健康检查（电机卡死检测、内存监控）
```

---

## 六、版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.1.0 | 2026-03-26 | 异步架构重构，LED 独立控制，摄像头接口 |
| v1.0.0 | 2026-03-25 | 初始版本 |
