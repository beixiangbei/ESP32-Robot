#!/usr/bin/env python3
"""
ESP32 Robot Python Tool Layer
封装 ESP32 HTTP 接口，提供给 OpenClaw 调用
"""

import requests
import time
from typing import Optional

class ESP32Robot:
    def __init__(self, ip: str, timeout: float = 5.0):
        self.ip = ip
        self.base_url = f"http://{ip}"
        self.timeout = timeout
    
    def _get(self, path: str) -> dict:
        """GET 请求"""
        try:
            resp = requests.get(f"{self.base_url}{path}", timeout=self.timeout)
            resp.raise_for_status()
            return resp.json()
        except Exception as e:
            return {"error": str(e)}
    
    def _post(self, path: str, data: dict = None) -> dict:
        """POST 请求"""
        try:
            resp = requests.post(f"{self.base_url}{path}", json=data, timeout=self.timeout)
            resp.raise_for_status()
            return resp.json()
        except Exception as e:
            return {"error": str(e)}
    
    # ========== 摄像头 ==========
    def get_image(self) -> bytes:
        """获取摄像头图像 (JPEG)"""
        try:
            resp = requests.get(f"{self.base_url}/image", timeout=self.timeout)
            resp.raise_for_status()
            return resp.content
        except Exception as e:
            raise RuntimeError(f"获取图像失败: {e}")
    
    # ========== 电机控制 ==========
    def move_motor1(self, steps: int) -> dict:
        """电机1转动"""
        return self._get(f"/motor1?step={steps}")
    
    def move_motor2(self, steps: int) -> dict:
        """电机2转动"""
        return self._get(f"/motor2?step={steps}")
    
    def move_head(self, steps: int) -> dict:
        """转头 (电机1别名)"""
        return self.move_motor1(steps)
    
    # ========== 状态 ==========
    def get_status(self) -> dict:
        """获取系统状态"""
        return self._get("/status")
    
    # ========== TTS (占位) ==========
    def play_tts(self, text: str = "") -> dict:
        """播放 TTS (暂未实现)"""
        return self._post("/tts", {"text": text})


# ========== 便捷函数 ==========
def create_robot(ip: str) -> ESP32Robot:
    """创建机器人实例"""
    return ESP32Robot(ip)


if __name__ == "__main__":
    # 测试
    import sys
    if len(sys.argv) < 2:
        print("Usage: python esp32_robot.py <IP>")
        sys.exit(1)
    
    robot = ESP32Robot(sys.argv[1])
    
    print("=== ESP32 Robot Test ===")
    print(robot.get_status())