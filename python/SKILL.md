---
name: esp32-robot
description: "ESP32-S3 桌面机器人控制 - 显示表情、控制LED、云台、摄像头、音频。机器人IP: 192.168.31.220"
compatibility: "Portable Agent Skills format. Requires Python 3.8+ with requests library."
metadata:
  {
    "version": "1.0.0",
    "category": "hardware",
    "author": "beixiangbei",
    "openclaw":
      {
        "emoji": "🤖",
        "requires": { "bins": ["python3"], "pip": ["requests"] },
      },
  }
---

# ESP32-Robot Skill

控制 ESP32-S3 桌面机器人，实现表情显示、LED控制、云台运动、摄像头拍照、音频提示等功能。

## 设备配置

- **IP地址**: 192.168.31.220
- **屏幕**: 128x64 OLED, rotation=3 (270°)
- **LED**: 5颗 WS2812 (3+2)
- **云台**: 双轴步进电机 (pan/tilt)
- **摄像头**: OV5640 JPEG
- **音频**: 扬声器 + 麦克风

## 表情系统

显示3字符表情 (size=3, line=2):

| 表情 | 文字 | LED效果 |
|------|------|---------|
| happy | `^-^` | 黄色静态 |
| sad | `T_T` | 蓝色呼吸 |
| angry | `>_<` | 红色闪烁 |
| surprised | `!_!` | 紫色脉冲 |
| think | `-_-` | 蓝色静态 |
| sleepy | `-.-` | 紫色呼吸 |
| shy | `@_@` | 粉色呼吸 |
| cool | `B-D` | 绿色彩虹 |
| love | `*_*` | 粉色脉冲 |
| confused | `?_?` | 黄色闪烁 |
| silent | `._.` | 灰色呼吸 |
| wave | `^_^` | 绿色脉冲 |
| loading | `o_o` | 青色脉冲 |
| neutral | `o_o` | 白色静态 |

## 使用方法

```bash
# 显示表情
python3 {baseDir}/esp32_robot_skill.py emotion happy
python3 {baseDir}/esp32_robot_skill.py emotion think

# 显示自定义文字
python3 {baseDir}/esp32_robot_skill.py text "Hello" --line 0 --size 2

# LED控制
python3 {baseDir}/esp32_robot_skill.py led rainbow --brightness 50
python3 {baseDir}/esp32_robot_skill.py led off

# 云台控制
python3 {baseDir}/esp32_robot_skill.py look --pan 500 --tilt 300
python3 {baseDir}/esp32_robot_skill.py center
python3 {baseDir}/esp32_robot_skill.py look-around --style slow

# 摄像头
python3 {baseDir}/esp32_robot_skill.py capture --save /tmp/photo.jpg

# 音频
python3 {baseDir}/esp32_robot_skill.py beep
python3 {baseDir}/esp32_robot_skill.py listen

# 系统
python3 {baseDir}/esp32_robot_skill.py status
python3 {baseDir}/esp32_robot_skill.py battery

# 便捷方法
python3 {baseDir}/esp32_robot_skill.py wake-up    # 显示开心表情
python3 {baseDir}/esp32_robot_skill.py sleep      # 关闭显示和LED
```

## 命令详情

### 表情命令
```bash
python3 esp32_robot_skill.py emotion <emotion_name>
# emotion_name: happy, sad, angry, surprised, think, sleepy, shy, cool, love, confused, silent, wave, loading, neutral
```

### 文字显示
```bash
python3 esp32_robot_skill.py text "文字内容" [--line 0-7] [--size 1-8] [--x X] [--y Y]
```

### LED控制
```bash
python3 esp32_robot_skill.py led <effect> [--color RRGGBB] [--brightness 0-100] [--speed 1-100] [--target 0|1|-1]
# effect: off, static, blink, breath, rainbow, pulse
# target: 0=LED0(3灯), 1=LED1(2灯), -1=全部(默认)
```

### 云台控制
```bash
python3 esp32_robot_skill.py look --pan 0-1024 --tilt 0-1024 [--speed 1-10]
python3 esp32_robot_skill.py center  # 归中 (512, 512)
python3 esp32_robot_skill.py look-around [--style slow|shake|nod] [--rounds N]
python3 esp32_robot_skill.py stop [pan|tilt|all]
```

### 摄像头
```bash
python3 esp32_robot_skill.py capture [--save /path/to/save.jpg]
python3 esp32_robot_skill.py camera on|off
```

### 音频
```bash
python3 esp32_robot_skill.py beep [--type ok|error|click|beep|warning]
python3 esp32_robot_skill.py listen  # 返回环境音量 0-255
```

### 系统
```bash
python3 esp32_robot_skill.py status   # 完整状态
python3 esp32_robot_skill.py battery  # 电池状态
python3 esp32_robot_skill.py ping      # 检查在线
python3 esp32_robot_skill.py reboot   # 重启设备
```

## Python API

```python
from esp32_robot_skill import ESP32RobotSkill

robot = ESP32RobotSkill("192.168.31.220")

# 显示表情
robot.show_emotion("happy")

# LED控制
robot.set_led("rainbow", brightness=50)
robot.set_led_off()

# 云台
robot.look_at(pan=500, tilt=300)
robot.center()

# 摄像头
robot.capture(save_path="photo.jpg")

# 音频
robot.beep()
level = robot.listen_level()

# 系统
status = robot.get_status()
battery = robot.get_battery()
```

## 安装依赖

```bash
pip install requests
```

## 注意事项

1. 确保机器人和电脑在同一网络
2. 电池电压 < 15% 时LED会低电量闪烁警告
3. 摄像头和OLED共享I2C总线，同时开启会自动处理冲突
4. 云台移动完成后 busy=false，可通过 wait_for_idle() 等待
