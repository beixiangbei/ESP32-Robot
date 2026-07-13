#!/usr/bin/env python3
"""
ESP32-Robot Skill - AI Agent 具身交互控制库

用于控制 ESP32-S3 桌面机器人，通过 HTTP API 实现表情、LED、云台、摄像头等功能

用法:
    python3 esp32_robot_skill.py <command> [args...]

示例:
    python3 esp32_robot_skill.py emotion happy
    python3 esp32_robot_skill.py led rainbow --brightness 50
    python3 esp32_robot_skill.py look --pan 500 --tilt 300
"""

import requests
import time
import sys
import json
import argparse
from typing import Optional


class ESP32RobotSkill:
    """ESP32桌面机器人控制Skill - 用于AI Agent具身交互"""

    def __init__(self, host: str = "192.168.31.220"):
        self.host = f"http://{host}"
        self.api = f"{self.host}/api/v1"

        # 默认参数
        self.face_line = 2      # 表情显示行
        self.face_size = 3      # 表情字号
        self.led_brightness = 10  # LED默认亮度（不晃眼）
        self.default_speed = 8   # 默认速度

        # 表情预设配置 (所有表情3字符, size=3, line=2)
        self._emotions = {
            "happy":     {"text": "^-^",  "led_effect": "static", "led_color": "FFFF00"},
            "sad":       {"text": "T_T",  "led_effect": "breath",  "led_color": "0088FF"},
            "angry":     {"text": ">_<",  "led_effect": "blink",  "led_color": "FF0000"},
            "surprised": {"text": "!_!",  "led_effect": "pulse",  "led_color": "AA00FF"},
            "think":     {"text": "-_-",  "led_effect": "static", "led_color": "0088FF"},
            "sleepy":    {"text": "-.-",  "led_effect": "breath",  "led_color": "8800FF"},
            "shy":       {"text": "@_@",  "led_effect": "breath",  "led_color": "FF88CC"},
            "cool":      {"text": "B-D",  "led_effect": "rainbow", "led_color": "00FF00"},
            "love":      {"text": "*_*",  "led_effect": "pulse",  "led_color": "FF88CC"},
            "confused":  {"text": "?_?",  "led_effect": "blink",  "led_color": "FFFF00"},
            "silent":    {"text": "._.",  "led_effect": "breath",  "led_color": "CCCCCC"},
            "wave":      {"text": "^_^",  "led_effect": "pulse",  "led_color": "00FF88"},
            "loading":   {"text": "o_o",  "led_effect": "pulse",  "led_color": "00FFFF"},
            "neutral":   {"text": "o_o",  "led_effect": "static", "led_color": "FFFFFF"},
        }

    # ========== 表情系统 ==========

    def show_emotion(self, emotion: str, duration: int = 0) -> dict:
        """
        显示预设表情

        Args:
            emotion: 表情名 (happy, very_happy, sad, angry, surprised, think,
                     sleepy, shy, cool, neutral, love, confused, silent, wave, loading)
            duration: 显示时长(秒)，0=一直显示

        Returns:
            {"ok": True, "emotion": emotion} 或 {"error": "message"}
        """
        if emotion not in self._emotions:
            available = ", ".join(self._emotions.keys())
            return {"error": f"Unknown emotion: {emotion}. Available: {available}"}

        cfg = self._emotions[emotion]

        # 显示表情文字
        self.show_text(cfg["text"], line=self.face_line, size=self.face_size)

        # 设置LED效果
        self.set_led(
            effect=cfg["led_effect"],
            color=cfg["led_color"],
            brightness=self.led_brightness
        )

        if duration > 0:
            time.sleep(duration)
            self.clear_display()

        return {"ok": True, "emotion": emotion}

    def show_text(self, text: str, **kwargs) -> dict:
        """
        显示自定义文字

        Args:
            text: 要显示的字符串
            x, y: 坐标 (可选)
            line: 行号 0-7 (可选，设置后x,y被忽略)
            size: 字号 1-8 (可选)

        Returns:
            {"ok": True, "text": text, ...}
        """
        # 固件会在文本两端加引号，发送前剥掉避免出现双引号
        clean_text = text.strip('"')
        data = {"text": clean_text}

        if "x" in kwargs:
            data["x"] = kwargs["x"]
        if "y" in kwargs:
            data["y"] = kwargs["y"]
        if "line" in kwargs:
            data["line"] = kwargs["line"]
        if "size" in kwargs:
            data["size"] = kwargs["size"]
        elif kwargs.get("_use_default_size", False):
            data["size"] = self.face_size

        try:
            r = requests.post(
                f"{self.api}/oled/text",
                data=json.dumps(data, separators=(",", ":")),
                headers={
                    "Content-Type": "application/json",
                    "Accept-Encoding": "identity",
                },
                timeout=5,
            )
            if r.status_code == 200:
                # 固件返回的JSON中text字段带多余引号，手动提取
                try:
                    text_val = json.loads(r.text).get("text", clean_text)
                    return {"ok": True, "text": text_val}
                except Exception:
                    return {"ok": True, "text": clean_text}
            return {"error": f"HTTP {r.status_code}"}
        except Exception as e:
            return {"error": str(e)}

    def clear_display(self) -> dict:
        """清除屏幕"""
        r = requests.post(f"{self.api}/oled/clear", timeout=5)
        return r.json()

    def set_display_rotation(self, rotation: int) -> dict:
        """
        设置屏幕旋转

        Args:
            rotation: 0=0°, 1=90°, 2=180°, 3=270°
        """
        r = requests.post(
            f"{self.api}/oled/rotation",
            data=json.dumps({"rotation": rotation}, separators=(",", ":")),
            headers={"Content-Type": "application/json", "Accept-Encoding": "identity"},
            timeout=5,
        )
        return r.json()

    # ========== LED 系统 ==========

    def set_led(self, effect: str = "static", color: str = "FFFFFF",
                brightness: int = None, speed: int = None, target: int = -1) -> dict:
        """
        设置LED效果

        Args:
            effect: off/static/blink/breath/rainbow/pulse
            color: RRGGBB 格式颜色
            brightness: 0-100 (可选)
            speed: 1-100 (可选)
            target: 0=LED0(3灯), 1=LED1(2灯), -1=全部 (默认-1)

        Returns:
            LED设置结果
        """
        data = {
            "target": target,
            "effect": effect,
            "color": color.lstrip("#")
        }

        if brightness is not None:
            data["brightness"] = brightness
        if speed is not None:
            data["speed"] = speed

        try:
            r = requests.post(
                f"{self.api}/led/effect",
                data=json.dumps(data, separators=(",", ":")),
                headers={"Content-Type": "application/json", "Accept-Encoding": "identity"},
                timeout=5,
            )
            if r.status_code == 200:
                return {"ok": True}
            return {"error": f"HTTP {r.status_code}"}
        except Exception as e:
            return {"error": str(e)}

    def set_led_off(self, target: int = -1) -> dict:
        """关闭LED"""
        return self.set_led(effect="off", target=target)

    def set_led_color(self, color: str, brightness: int = None, target: int = -1) -> dict:
        """设置LED颜色（静态）"""
        return self.set_led(effect="static", color=color, brightness=brightness, target=target)

    def get_led_status(self) -> dict:
        """获取LED状态"""
        r = requests.get(f"{self.api}/led/status", timeout=5)
        return r.json()

    # ========== 云台系统 ==========

    def look_at(self, pan: int = None, tilt: int = None,
                speed: int = None, wait: bool = False) -> dict:
        """
        控制云台朝向

        Args:
            pan: 水平位置 0-1024 (可选，与tilt二选一或都选)
            tilt: 垂直位置 0-1024 (可选)
            speed: 速度 1-10 (可选)
            wait: 是否等待到达位置 (可选)

        Returns:
            电机状态
        """
        if pan is None and tilt is None:
            return {"error": "pan or tilt required"}

        results = []

        if pan is not None:
            data = {"motor": 0, "target": pan}
            if speed:
                data["speed"] = speed
            r = requests.post(
                f"{self.api}/motor/absolute",
                data=json.dumps(data, separators=(",", ":")),
                headers={"Content-Type": "application/json", "Accept-Encoding": "identity"},
                timeout=5,
            )
            results.append(r.json())

        if tilt is not None:
            data = {"motor": 1, "target": tilt}
            if speed:
                data["speed"] = speed
            r = requests.post(
                f"{self.api}/motor/absolute",
                data=json.dumps(data, separators=(",", ":")),
                headers={"Content-Type": "application/json", "Accept-Encoding": "identity"},
                timeout=5,
            )
            results.append(r.json())

        if wait:
            self.wait_for_idle()

        return results[0] if len(results) == 1 else results

    def move_relative(self, motor: int, steps: int,
                      speed: int = None, accel: bool = True) -> dict:
        """
        相对移动

        Args:
            motor: 0=pan(水平), 1=tilt(垂直)
            steps: 步数，正数顺时针，负数逆时针
            speed: 速度 1-10
            accel: 是否加速
        """
        data = {"motor": motor, "steps": steps, "accel": accel}
        if speed:
            data["speed"] = speed
        r = requests.post(
            f"{self.api}/motor/relative",
            data=json.dumps(data, separators=(",", ":")),
            headers={"Content-Type": "application/json", "Accept-Encoding": "identity"},
            timeout=5,
        )
        return r.json()

    def center(self) -> dict:
        """云台归中 (pan=512, tilt=512)"""
        return self.look_at(pan=512, tilt=512)

    def look_around(self, style: str = "slow", rounds: int = 1) -> list:
        """
        环顾四周

        Args:
            style: slow(慢速扫描)/shake(摇头)/nod(点头)
            rounds: 循环次数
        """
        sequences = {
            "slow": [(200, 300), (800, 300), (800, 600), (200, 600), (512, 450)],
            "shake": [(300, 450), (700, 450), (300, 450), (700, 450)],
            "nod": [(512, 300), (512, 600), (512, 300), (512, 600)],
        }

        if style not in sequences:
            return [{"error": f"Unknown style: {style}. Available: {list(sequences.keys())}"}]

        results = []
        for _ in range(rounds):
            for pan, tilt in sequences[style]:
                self.look_at(pan=pan, tilt=tilt, speed=self.default_speed)
                time.sleep(0.8)
                results.append({"pan": pan, "tilt": tilt})

        return results

    def stop_motor(self, motor: str = "all") -> dict:
        """
        停止电机

        Args:
            motor: "pan"/"tilt"/"all"
        """
        r = requests.post(
            f"{self.api}/motor/stop",
            data=json.dumps({"motor": motor}, separators=(",", ":")),
            headers={"Content-Type": "application/json", "Accept-Encoding": "identity"},
            timeout=5,
        )
        return r.json()

    def wait_for_idle(self, timeout_ms: int = 10000) -> bool:
        """
        等待电机空闲

        Args:
            timeout_ms: 超时毫秒

        Returns:
            True=空闲, False=超时
        """
        start = time.time() * 1000
        while (time.time() * 1000 - start) < timeout_ms:
            status = self.get_motor_status()
            if "pan" in status and "tilt" in status:
                if not status["pan"].get("busy", False) and not status["tilt"].get("busy", False):
                    return True
            time.sleep(0.1)
        return False

    def get_motor_status(self) -> dict:
        """获取电机状态"""
        r = requests.get(f"{self.api}/motor/status", timeout=5)
        return r.json()

    # ========== 摄像头 ==========

    def capture(self, save_path: str = None, warmup: float = 5.0) -> dict:
        """
        拍照

        Args:
            save_path: 可选保存路径
            warmup: 等待摄像头稳定的时间(秒)，默认5秒

        Returns:
            {"ok": True, "size": bytes大小} 或 {"error": "message"}
        """
        try:
            # 拍照前关闭LED1避免光污染，等待摄像头自动曝光稳定
            self.set_led(effect="off", target=1)
            if warmup > 0:
                time.sleep(warmup)
            r = requests.get(f"{self.api}/camera/capture", timeout=10)
            if r.status_code == 200:
                data = r.content
                if save_path:
                    with open(save_path, "wb") as f:
                        f.write(data)
                return {"ok": True, "size": len(data)}
            else:
                return {"error": f"Capture failed: {r.status_code}"}
        except Exception as e:
            return {"error": str(e)}

    def enable_camera(self, enabled: bool = True) -> dict:
        """开关摄像头"""
        r = requests.post(
            f"{self.api}/camera/config",
            data=json.dumps({"enabled": enabled}, separators=(",", ":")),
            headers={"Content-Type": "application/json", "Accept-Encoding": "identity"},
            timeout=5,
        )
        return r.json()

    # ========== 音频 ==========

    def beep(self, type: str = "ok") -> dict:
        """
        发出提示音

        Args:
            type: ok/error/click/beep/warning (目前都是1kHz测试音)
        """
        r = requests.post(f"{self.api}/audio/speaker/test", timeout=5)
        return r.json()

    def listen_level(self) -> dict:
        """
        获取环境音量

        Returns:
            {"ok": True, "level": 0-255}
        """
        r = requests.get(f"{self.api}/audio/mic/test", timeout=5)
        return r.json()

    def get_audio_status(self) -> dict:
        """获取音频状态"""
        r = requests.get(f"{self.api}/audio/status", timeout=5)
        return r.json()

    # ========== 系统 ==========

    def get_status(self) -> dict:
        """获取完整系统状态"""
        try:
            r = requests.get(f"{self.api}/status", timeout=5)
            return r.json()
        except:
            return {"error": "failed to get status"}

    def get_battery(self) -> dict:
        """获取电池状态"""
        status = self.get_status()
        return status.get("battery", {})

    def show_battery_display(self) -> dict:
        """
        在OLED上显示电池状态（百分比、电压、充电指示）

        Returns:
            {"ok": True, "battery": {"voltage": 4.12, "percent": 100, "charging": true}}
        """
        try:
            r = requests.post(
                f"{self.api}/oled/text",
                data=json.dumps({"text": "battery"}, separators=(",", ":")),
                headers={
                    "Content-Type": "application/json",
                    "Accept-Encoding": "identity",
                },
                timeout=5,
            )
            if r.status_code == 200:
                try:
                    return json.loads(r.text)
                except Exception:
                    return r.json()
            return {"error": f"HTTP {r.status_code}"}
        except Exception as e:
            return {"error": str(e)}

    def ping(self) -> bool:
        """检查设备在线"""
        try:
            r = requests.get(f"{self.api}/ping", timeout=3)
            return r.status_code == 200
        except:
            return False

    def reboot(self) -> dict:
        """重启设备"""
        r = requests.post(f"{self.api}/control/reboot", timeout=5)
        return r.json()

    def self_check(self) -> dict:
        """组件自检"""
        r = requests.post(f"{self.api}/control/selfcheck", timeout=5)
        return r.json()

    # ========== 便捷方法 ==========

    def wake_up(self):
        """唤醒机器人 - 显示开心表情"""
        self.center()
        self.show_emotion("happy")
        self.set_led_color("00FF00")

    def sleep(self):
        """进入睡眠 - 关闭显示和LED"""
        self.clear_display()
        self.set_led_off()

    def express(self, emotion: str):
        """
        快速表达表情（与show_emotion相同）
        兼容旧写法
        """
        return self.show_emotion(emotion)


# ========== 独立测试 ==========

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ESP32-Robot Skill CLI")
    parser.add_argument("command", help="Command to execute")
    parser.add_argument("args", nargs="*", help="Command arguments")
    parser.add_argument("--host", default="192.168.31.220", help="Robot host IP")

    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    cmd_args = sys.argv[2:]
    args = parser.parse_args([cmd])
    robot = ESP32RobotSkill(args.host)

    try:
        if cmd == "emotion":
            emotion = cmd_args[0] if cmd_args else "happy"
            result = robot.show_emotion(emotion)
            print(result)

        elif cmd == "text":
            text = cmd_args[0] if cmd_args else ""
            parser2 = argparse.ArgumentParser()
            parser2.add_argument("--line", type=int)
            parser2.add_argument("--size", type=int)
            parser2.add_argument("--x", type=int)
            parser2.add_argument("--y", type=int)
            ns = parser2.parse_args(cmd_args[1:] if len(cmd_args) > 1 else [])
            kwargs = {}
            if ns.line is not None: kwargs["line"] = ns.line
            if ns.size is not None: kwargs["size"] = ns.size
            if ns.x is not None: kwargs["x"] = ns.x
            if ns.y is not None: kwargs["y"] = ns.y
            result = robot.show_text(text, **kwargs)
            print(result)

        elif cmd == "led":
            effect = cmd_args[0] if cmd_args else "static"
            parser2 = argparse.ArgumentParser()
            parser2.add_argument("--color", default="FFFFFF")
            parser2.add_argument("--brightness", type=int)
            parser2.add_argument("--speed", type=int)
            parser2.add_argument("--target", type=int, default=-1)
            ns = parser2.parse_args(cmd_args[1:] if len(cmd_args) > 1 else [])
            kwargs = {}
            if ns.brightness is not None: kwargs["brightness"] = ns.brightness
            if ns.speed is not None: kwargs["speed"] = ns.speed
            if ns.target is not None: kwargs["target"] = ns.target
            result = robot.set_led(effect, color=ns.color, **kwargs)
            print(result)

        elif cmd == "look":
            parser2 = argparse.ArgumentParser()
            parser2.add_argument("--pan", type=int)
            parser2.add_argument("--tilt", type=int)
            parser2.add_argument("--speed", type=int)
            ns = parser2.parse_args(cmd_args)
            kwargs = {}
            if ns.speed is not None: kwargs["speed"] = ns.speed
            if ns.pan is not None and ns.tilt is not None:
                result = robot.look_at(pan=ns.pan, tilt=ns.tilt, **kwargs)
            elif ns.pan is not None:
                result = robot.look_at(pan=ns.pan, **kwargs)
            elif ns.tilt is not None:
                result = robot.look_at(tilt=ns.tilt, **kwargs)
            else:
                print("Error: --pan or --tilt required")
                sys.exit(1)
            print(result)

        elif cmd == "center":
            print(robot.center())

        elif cmd == "look-around":
            parser2 = argparse.ArgumentParser()
            parser2.add_argument("--style", default="slow")
            parser2.add_argument("--rounds", type=int, default=1)
            ns = parser2.parse_args(cmd_args[1:] if len(cmd_args) > 1 else [])
            result = robot.look_around(style=ns.style, rounds=ns.rounds)
            print(result)

        elif cmd == "stop":
            motor = cmd_args[0] if cmd_args else "all"
            print(robot.stop_motor(motor))

        elif cmd == "capture":
            parser2 = argparse.ArgumentParser()
            parser2.add_argument("--save")
            ns = parser2.parse_args(cmd_args if cmd_args else [])
            result = robot.capture(save_path=ns.save)
            print(result)

        elif cmd == "camera":
            enabled = cmd_args[0] == "on" if cmd_args else True
            print(robot.enable_camera(enabled))

        elif cmd == "beep":
            parser2 = argparse.ArgumentParser()
            parser2.add_argument("--type", default="ok")
            ns = parser2.parse_args(cmd_args[1:] if len(cmd_args) > 1 else [])
            print(robot.beep(type=ns.type))

        elif cmd == "listen":
            print(robot.listen_level())

        elif cmd == "status":
            result = robot.get_status()
            print(result if isinstance(result, str) else str(result))

        elif cmd == "battery":
            result = robot.get_battery()
            print(result if isinstance(result, str) else str(result))

        elif cmd == "battery-display":
            result = robot.show_battery_display()
            print(result if isinstance(result, str) else str(result))

        elif cmd == "ping":
            print({"online": robot.ping()})

        elif cmd == "reboot":
            print(robot.reboot())

        elif cmd == "clear":
            print(robot.clear_display())

        elif cmd == "wake-up":
            robot.wake_up()
            print({"ok": True, "action": "wake-up"})

        elif cmd == "sleep":
            robot.sleep()
            print({"ok": True, "action": "sleep"})

        else:
            print(f"Unknown command: {cmd}")
            print("Available commands: emotion, text, led, look, center, look-around, stop, capture, camera, beep, listen, status, battery, ping, reboot, clear, wake-up, sleep")
            sys.exit(1)

    except Exception as e:
        print({"error": str(e)})
        sys.exit(1)
