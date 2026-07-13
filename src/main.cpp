#include <Arduino.h>
#include <esp_camera.h>
#include <esp_http_server.h>
#include <esp_system.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "audio.h"

// ========== WiFi 配置 ==========
const char* WIFI_SSID = "Xiaomi_MC1304";
const char* WIFI_PASSWORD = "qwertyui";
const char* VERSION = "v1.1.0";

// ========== 引脚定义 ==========
#define CAM_PWDN   47
#define CAM_RST    -1
#define CAM_XCLK   21
#define CAM_PCLK   15
#define CAM_VSYNC  38
#define CAM_HREF   39

#define MOTOR_SHCP 3
#define MOTOR_STCP 4
#define MOTOR_DS   5

#define LED_PIN_0  48   // LED组0: 3灯
#define LED_PIN_1  7    // LED组1: 2灯
#define LED_COUNT_0 3
#define LED_COUNT_1 2

#define VBAT_ADC   8
#define USB_CHG     6

// ========== OLED 定义 ==========
#define OLED_SCREEN_WIDTH 128
#define OLED_SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET);
uint8_t oled_rotation = 0;  // 0, 1, 2, 3 = 0°, 90°, 180°, 270°

// ========== 电机状态 ==========
struct MotorState {
    int pos;            // 当前位置
    bool busy;          // 是否运动中
    int target_pos;     // 目标位置
    int speed;          // 当前速度
    bool accel;         // 是否启用加速度
};

MotorState motors[2] = {
    {0, false, 0, 10, true},
    {0, false, 0, 10, true}
};

// 74HC595 控制
volatile uint8_t motorState = 0;
const uint8_t phaseSequence[8] = {0x08, 0x0C, 0x04, 0x06, 0x02, 0x03, 0x01, 0x09};
volatile int motor1Seq = 0, motor2Seq = 4;

Adafruit_NeoPixel strip0(LED_COUNT_0, LED_PIN_0, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip1(LED_COUNT_1, LED_PIN_1, NEO_GRB + NEO_KHZ800);

// LED 动画状态（每个 LED 独立）
unsigned long led_last_update[2] = {0, 0};
int led_animation_step[2] = {0, 0};

// ========== 全局状态 ==========
bool camera_enabled = false;
unsigned long boot_time = 0;
httpd_handle_t httpServer = NULL;

// ========== 电机命令队列 ==========
struct MotorCommand {
    int motor_idx;      // 0=pan, 1=tilt
    int steps;          // 相对步数 (0表示用target)
    int target;         // 绝对目标位置 (-1表示用steps)
    int speed;
    bool accel;
};

xQueueHandle motorQueue = NULL;
StaticQueue_t motorQueueStruct;
uint8_t motorQueueBuffer[sizeof(MotorCommand) * 8];

// ========== LED 命令队列 ==========
struct LedCommand {
    int led_idx;        // 0/1 或 -1表示全部
    char effect[16];    // 用固定数组替代 String
    uint32_t color;
    int speed;
    int brightness;     // 0-100, 100为全亮
};

xQueueHandle ledQueue = NULL;
StaticQueue_t ledQueueStruct;
uint8_t ledQueueBuffer[sizeof(LedCommand) * 8];

// ========== LED 状态 ==========
struct LedGroup {
    char effect[16];
    uint32_t color;
    int speed;
    int brightness;  // 0-100, 默认100
};

LedGroup leds[2] = {
    {"static", 0xFF0000, 50, 100},
    {"static", 0x00FF00, 50, 100}
};

// ========== 函数声明 ==========
void shiftOut(uint8_t data);
void latch();
void updateMotorState();
void initLED();
void setLedColor(int idx, uint32_t color);
uint32_t hsvToRgb(float h, float s, float v);
void ledUpdate();
float readBatteryVoltage();
int voltageToPercent(float v);
bool isCharging();

// ========== 74HC595 控制 ==========
void shiftOut(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        digitalWrite(MOTOR_DS, (data & 0x80) ? HIGH : LOW);
        digitalWrite(MOTOR_SHCP, HIGH);
        delayMicroseconds(1);
        digitalWrite(MOTOR_SHCP, LOW);
        data <<= 1;
    }
}

void latch() {
    digitalWrite(MOTOR_STCP, HIGH);
    delayMicroseconds(1);
    digitalWrite(MOTOR_STCP, LOW);
}

void updateMotorState() {
    shiftOut(motorState);
    latch();
}

// ========== 单步电机 ==========
void stepMotor(int motor_idx, int dir) {
    if (motor_idx == 0) {
        motor1Seq = (motor1Seq + dir + 8) % 8;
        motorState = (motorState & 0xF0) | (phaseSequence[motor1Seq] & 0x0F);
    } else {
        motor2Seq = (motor2Seq + dir + 8) % 8;
        motorState = (motorState & 0x0F) | ((phaseSequence[motor2Seq] & 0x0F) << 4);
    }
    updateMotorState();
}

// ========== 速度计算 (加速度曲线) ==========
int calcSpeed(int step, int total, int base_speed, bool accel) {
    if (!accel || total <= 1) return base_speed;
    float progress = (float)step / total;
    float speed_factor;
    if (progress < 0.2) {
        speed_factor = 0.3 + 0.7 * (progress / 0.2);
    } else if (progress > 0.8) {
        speed_factor = 1.0 - 0.7 * ((progress - 0.8) / 0.2);
    } else {
        speed_factor = 1.0;
    }
    int speed = (int)(base_speed / speed_factor);
    return constrain(speed, base_speed / 3, base_speed * 2);
}

// ========== 电机 Task ==========
void motorTask(void* param) {
    MotorCommand cmd;
    while (1) {
        if (xQueueReceive(motorQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            int motor_idx = cmd.motor_idx;
            int dir = 0;
            int steps_to_move = 0;

            if (cmd.target >= 0) {
                int diff = cmd.target - motors[motor_idx].pos;
                if (diff == 0) {
                    motors[motor_idx].busy = false;
                    continue;
                }
                dir = diff > 0 ? 1 : -1;
                steps_to_move = abs(diff);
            } else {
                dir = cmd.steps > 0 ? 1 : -1;
                steps_to_move = abs(cmd.steps);
            }

            motors[motor_idx].busy = true;
            motors[motor_idx].speed = cmd.speed;

            for (int i = 0; i < steps_to_move; i++) {
                stepMotor(motor_idx, dir);
                int speed = calcSpeed(i, steps_to_move, cmd.speed, cmd.accel);
                vTaskDelay(speed / portTICK_PERIOD_MS);
            }

            if (cmd.target >= 0) {
                motors[motor_idx].pos = cmd.target;
            } else {
                motors[motor_idx].pos += cmd.steps;
            }

            motors[motor_idx].busy = false;
            motors[motor_idx].target_pos = motors[motor_idx].pos;
        }
    }
}

void stopMotor(int motor_idx) {
    if (motor_idx == 0 || motor_idx == 1) {
        motors[motor_idx].busy = false;
        motors[motor_idx].target_pos = motors[motor_idx].pos;
        Serial.printf("[MOTOR] %d stopped at pos=%d\n", motor_idx, motors[motor_idx].pos);
    } else {
        motors[0].busy = false;
        motors[1].busy = false;
        Serial.println("[MOTOR] all stopped");
    }
}

// ========== LED Task ==========
void ledTask(void* param) {
    LedCommand cmd;
    while (1) {
        // 先处理队列中的命令（不阻塞）
        if (xQueueReceive(ledQueue, &cmd, 0) == pdTRUE) {
            if (cmd.led_idx == -1) {
                for (int i = 0; i < 2; i++) {
                    strncpy(leds[i].effect, cmd.effect, 15);
                    leds[i].effect[15] = 0;
                    leds[i].color = cmd.color;
                    leds[i].speed = cmd.speed;
                    leds[i].brightness = cmd.brightness;
                }
            } else if (cmd.led_idx >= 0 && cmd.led_idx < 2) {
                strncpy(leds[cmd.led_idx].effect, cmd.effect, 15);
                leds[cmd.led_idx].effect[15] = 0;
                leds[cmd.led_idx].color = cmd.color;
                leds[cmd.led_idx].speed = cmd.speed;
                leds[cmd.led_idx].brightness = cmd.brightness;
            }
        }
        // 持续更新 LED 动画
        ledUpdate();
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void initLED() {
    strip0.begin();
    strip1.begin();
    strip0.show();
    strip1.show();
}

void setLedColor(int idx, uint32_t color) {
    Adafruit_NeoPixel* strip = (idx == 0) ? &strip0 : &strip1;
    int count = (idx == 0) ? LED_COUNT_0 : LED_COUNT_1;
    for (int i = 0; i < count; i++) {
        strip->setPixelColor(i, color);
    }
    strip->show();
}

uint32_t hsvToRgb(float h, float s, float v) {
    float c = v * s, x = c * (1 - abs(fmod(h / 60.0, 2) - 1)), m = v - c;
    float r, g, b;
    if (h < 60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    return strip0.Color((r + m) * 255, (g + m) * 255, (b + m) * 255);
}

// 根据亮度缩放颜色 (brightness: 0-100)
uint32_t applyBrightness(uint32_t color, int brightness) {
    if (brightness >= 100) return color;
    if (brightness <= 0) return 0;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    float scale = brightness / 100.0;
    return strip0.Color(r * scale, g * scale, b * scale);
}

void ledUpdate() {
    unsigned long now = millis();
    static bool lowBatteryWarn = false;  // 保持状态，避免抖动
    static int warnEnterPct = 0;  // 进入警告时的电量

    float battery_v = readBatteryVoltage();
    int battery_pct = voltageToPercent(battery_v);
    bool charging = isCharging();

    // 迟滞逻辑：进入警告需要 <15%，退出需要 >25%（或正在充电）
    if (!lowBatteryWarn && battery_pct < 15 && !charging) {
        lowBatteryWarn = true;
        warnEnterPct = battery_pct;
    } else if (lowBatteryWarn && (battery_pct > 25 || charging)) {
        lowBatteryWarn = false;
    }

    for (int idx = 0; idx < 2; idx++) {
        Adafruit_NeoPixel* strip = (idx == 0) ? &strip0 : &strip1;
        int count = (idx == 0) ? LED_COUNT_0 : LED_COUNT_1;
        char* effect = leds[idx].effect;
        uint32_t color = leds[idx].color;
        int speed = leds[idx].speed;

        if (lowBatteryWarn) {
            int period = 500;
            if (now - led_last_update[idx] > period) {
                led_last_update[idx] = now;
                led_animation_step[idx] = !led_animation_step[idx];
                // 低电量闪烁：亮度降低到用户设置的5%
                uint32_t dimRed = applyBrightness(0xFF0000, 5);
                for (int i = 0; i < count; i++) {
                    strip->setPixelColor(i, led_animation_step[idx] ? dimRed : 0);
                }
                strip->show();
            }
            continue;
        }

        if (strcmp(effect, "off") == 0) {
            led_last_update[idx] = now;
            for (int i = 0; i < count; i++) strip->setPixelColor(i, 0);
            strip->show();
        }
        else if (strcmp(effect, "static") == 0) {
            led_last_update[idx] = now;
            uint32_t dimColor = applyBrightness(color, leds[idx].brightness);
            for (int i = 0; i < count; i++) strip->setPixelColor(i, dimColor);
            strip->show();
        }
        else if (strcmp(effect, "blink") == 0) {
            int period = 1000 / constrain(speed, 1, 100);
            if (now - led_last_update[idx] > period / 2) {
                led_last_update[idx] = now;
                led_animation_step[idx] = !led_animation_step[idx];
                uint32_t dimColor = applyBrightness(color, leds[idx].brightness);
                for (int i = 0; i < count; i++) {
                    strip->setPixelColor(i, led_animation_step[idx] ? dimColor : 0);
                }
                strip->show();
            }
        }
        else if (strcmp(effect, "breath") == 0) {
            int period = 20000 / constrain(speed, 1, 100);
            float t = fmod((float)(now % period) / period, 1.0);
            float animBrightness = sin(t * 3.14159) * 0.5 + 0.5;
            float userBrightness = leds[idx].brightness / 100.0;
            uint8_t r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;
            uint32_t c = strip->Color(r * animBrightness * userBrightness,
                                       g * animBrightness * userBrightness,
                                       b * animBrightness * userBrightness);
            for (int i = 0; i < count; i++) strip->setPixelColor(i, c);
            strip->show();
        }
        else if (strcmp(effect, "rainbow") == 0) {
            int period = 36000 / constrain(speed, 1, 100);
            if (now - led_last_update[idx] > period / 360) {
                led_last_update[idx] = now;
                led_animation_step[idx] = (led_animation_step[idx] + 1) % 360;
                float brightScale = leds[idx].brightness / 100.0;
                uint32_t c = hsvToRgb(led_animation_step[idx], 1.0, brightScale);
                for (int i = 0; i < count; i++) strip->setPixelColor(i, c);
                strip->show();
            }
        }
        else if (strcmp(effect, "pulse") == 0) {
            int period = 10000 / constrain(speed, 1, 100);
            float t = fmod((float)(now % period) / period, 1.0);
            float animBrightness = pow(sin(t * 3.14159), 2);
            float userBrightness = leds[idx].brightness / 100.0;
            uint8_t r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;
            uint32_t c = strip->Color(r * animBrightness * userBrightness,
                                       g * animBrightness * userBrightness,
                                       b * animBrightness * userBrightness);
            for (int i = 0; i < count; i++) strip->setPixelColor(i, c);
            strip->show();
        }
    }
}

// ========== 摄像头 ==========
esp_err_t configCamera() {
    Wire.begin(1, 2);
    camera_config_t camera_config;
    camera_config.ledc_channel = LEDC_CHANNEL_0;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.pin_d0 = 42; camera_config.pin_d1 = 45;
    camera_config.pin_d2 = 46; camera_config.pin_d3 = 41;
    camera_config.pin_d4 = 16; camera_config.pin_d5 = 18;
    camera_config.pin_d6 = 17; camera_config.pin_d7 = 40;
    camera_config.pin_xclk = CAM_XCLK;
    camera_config.pin_pclk = CAM_PCLK;
    camera_config.pin_vsync = CAM_VSYNC;
    camera_config.pin_href = CAM_HREF;
    camera_config.pin_sccb_sda = 2;
    camera_config.pin_sccb_scl = 1;
    camera_config.pin_pwdn = CAM_PWDN;
    camera_config.pin_reset = CAM_RST;
    camera_config.xclk_freq_hz = 20000000;
    camera_config.frame_size = FRAMESIZE_VGA;
    camera_config.pixel_format = PIXFORMAT_JPEG;
    camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    camera_config.fb_location = CAMERA_FB_IN_PSRAM;
    camera_config.jpeg_quality = 12;
    camera_config.fb_count = 2;

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("[CAM] init fail: 0x%x\n", err);
        return err;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_pixformat(s, PIXFORMAT_JPEG);
        s->set_framesize(s, FRAMESIZE_VGA);
    }

    camera_enabled = true;
    Serial.println("[CAM] ok");
    return ESP_OK;
}

framesize_t getFrameSize(const char* size) {
    if (strcmp(size, "qqvga") == 0) return FRAMESIZE_QQVGA;
    if (strcmp(size, "qvga") == 0) return FRAMESIZE_QVGA;
    if (strcmp(size, "cif") == 0) return FRAMESIZE_CIF;
    if (strcmp(size, "vga") == 0) return FRAMESIZE_VGA;
    if (strcmp(size, "svga") == 0) return FRAMESIZE_SVGA;
    if (strcmp(size, "xga") == 0) return FRAMESIZE_XGA;
    if (strcmp(size, "sxga") == 0) return FRAMESIZE_SXGA;
    if (strcmp(size, "uxga") == 0) return FRAMESIZE_UXGA;
    return FRAMESIZE_VGA;
}

// ========== 电量 ==========
void initBattery() {
    pinMode(VBAT_ADC, INPUT);
    pinMode(USB_CHG, INPUT);
}

float readBatteryVoltage() {
    // 采样平均，减少噪声
    const int samples = 16;
    float sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(VBAT_ADC);
        delayMicroseconds(100);
    }
    float raw_avg = sum / samples;
    // ESP32-S3 ADC: 12位 (0-4095), 参考电压 3.3V
    // 分压电阻: VBAT --[R1]--+--[R2]-- GND
    // 实际使用中分压比可能不是严格的 2:1，需要校准
    float v_adc = raw_avg / 4095.0 * 3.3;
    return v_adc * 2.0;  // 假设分压比 2:1
}

// 3.7V LiPo 电池电压曲线（近似）：
// 4.2V = 100%, 3.7V = ~60%, 3.5V = ~40%, 3.0V = ~5%, 3.3V = 20%
int voltageToPercent(float v) {
    if (v >= 4.2) return 100;
    if (v <= 3.0) return 0;
    if (v <= 3.3) return (int)((v - 3.0) / 0.3 * 20);  // 3.0-3.3V: 0-20%
    if (v <= 3.7) return (int)(20 + (v - 3.3) / 0.4 * 40);  // 3.3-3.7V: 20-60%
    return (int)(60 + (v - 3.7) / 0.5 * 40);  // 3.7-4.2V: 60-100%
}

bool isCharging() {
    // 充电检测需要稳定的电平，添加去抖
    static unsigned long last_change = 0;
    static int last_state = -1;
    static int stable_count = 0;

    int current = digitalRead(USB_CHG);
    unsigned long now = millis();

    if (current != last_state) {
        last_change = now;
        last_state = current;
        stable_count = 0;
        return last_state == HIGH;  // 返回上一次稳定状态
    }

    // 消抖 100ms
    if (now - last_change > 100) {
        stable_count++;
        if (stable_count > 5) {  // 连续5次稳定才确认状态
            return current == HIGH;
        }
    }
    return last_state == HIGH;
}

// ========== Monitor Task ==========
unsigned long lastMotorCheck[2] = {0, 0};

void monitorTask(void* param) {
    while (1) {
        for (int i = 0; i < 2; i++) {
            if (motors[i].busy) {
                if (lastMotorCheck[i] == 0) lastMotorCheck[i] = millis();
                else if (millis() - lastMotorCheck[i] > 30000) {
                    Serial.printf("[MONITOR] Motor %d stuck > 30s, stopping\n", i);
                    stopMotor(i);
                    lastMotorCheck[i] = 0;
                }
            } else {
                lastMotorCheck[i] = 0;
            }
        }

        size_t freeHeap = esp_get_free_heap_size();
        if (freeHeap < 20000) {
            Serial.printf("[MONITOR] Low memory: %d bytes\n", freeHeap);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// ========== JSON 工具 ==========
int getQueryParam(httpd_req_t* req, const char* key, char* val, size_t len) {
    const char* q = strchr(req->uri, '?');
    if (!q) return -1;
    const char* p = strstr(q + 1, key);
    if (!p || !(p = strchr(p, '='))) return -1;
    const char* e = strchr(++p, '&');
    if (!e) e = p + strlen(p);
    size_t l = e - p;
    if (l >= len) l = len - 1;
    strncpy(val, p, l);
    val[l] = 0;
    return 0;
}

int getQueryParamInt(httpd_req_t* req, const char* key, int def) {
    char buf[32] = {0};
    return getQueryParam(req, key, buf, sizeof(buf)) == 0 ? atoi(buf) : def;
}

uint32_t parseHexColor(const char* str) {
    if (!str) return 0;
    return strtol(str, NULL, 16);
}

// JSON 解析辅助函数
int jsonGetInt(const char* buf, const char* key, int def) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char* p = strstr(buf, pattern);
    if (!p) return def;
    p += strlen(pattern);
    // 跳过空格
    while (*p == ' ') p++;
    // 检查 true/false
    if (strncmp(p, "true", 4) == 0) return 1;
    if (strncmp(p, "false", 5) == 0) return 0;
    // 解析数字（可能是负数）
    int val = atoi(p);
    return val;
}

const char* jsonGetStr(const char* buf, const char* key, char* out, size_t maxlen) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    char* p = strstr(buf, pattern);
    if (!p) {
        // 尝试没有引号的格式
        snprintf(pattern, sizeof(pattern), "\"%s\":", key);
        p = strstr(buf, pattern);
        if (!p) return NULL;
        p += strlen(pattern);
        while (*p == ' ') p++;
        // 读取直到 , 或 }
        size_t i = 0;
        while (*p && *p != ',' && *p != '}' && i < maxlen - 1) {
            out[i++] = *p++;
        }
        out[i] = 0;
        return out;
    }
    p += strlen(pattern);
    size_t i = 0;
    while (*p && *p != '"' && i < maxlen - 1) {
        out[i++] = *p++;
    }
    out[i] = 0;
    return out;
}

// ========== HTTP Handlers ==========

// POST /api/v1/motor/relative
static esp_err_t motorRelativeHandler(httpd_req_t* req) {
    MotorCommand cmd = {0, 0, -1, 10, true};

    char buf[256];
    if (req->method == HTTP_GET) {
        cmd.motor_idx = getQueryParamInt(req, "motor", 0);
        cmd.steps = getQueryParamInt(req, "steps", 0);
        cmd.speed = getQueryParamInt(req, "speed", 10);
        cmd.accel = getQueryParamInt(req, "accel", 1) == 1;
    } else {
        int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (len <= 0) return ESP_FAIL;
        buf[len] = 0;

        cmd.motor_idx = jsonGetInt(buf, "motor", 0);
        cmd.steps = jsonGetInt(buf, "steps", 0);
        cmd.speed = jsonGetInt(buf, "speed", 10);
        cmd.accel = jsonGetInt(buf, "accel", 1) == 1;

        if (cmd.speed <= 0) cmd.speed = 10;
    }

    if (cmd.motor_idx < 0 || cmd.motor_idx > 1) {
        httpd_resp_send(req, "{\"error\":\"invalid motor\"}", 26);
        return ESP_FAIL;
    }

    xQueueSend(motorQueue, &cmd, 0);

    char resp[128];
    snprintf(resp, sizeof(resp),
        "{\"motor\":%d,\"busy\":true,\"speed\":%d,\"accel\":%s}",
        cmd.motor_idx, cmd.speed, cmd.accel ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /api/v1/motor/absolute
static esp_err_t motorAbsoluteHandler(httpd_req_t* req) {
    MotorCommand cmd = {0, 0, -1, 10, true};

    char buf[256];
    if (req->method == HTTP_GET) {
        cmd.motor_idx = getQueryParamInt(req, "motor", 0);
        cmd.target = getQueryParamInt(req, "target", -1);
        cmd.speed = getQueryParamInt(req, "speed", 10);
    } else {
        int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (len <= 0) return ESP_FAIL;
        buf[len] = 0;

        cmd.motor_idx = jsonGetInt(buf, "motor", 0);
        cmd.target = jsonGetInt(buf, "target", -1);
        cmd.speed = jsonGetInt(buf, "speed", 10);

        if (cmd.speed <= 0) cmd.speed = 10;
    }

    if (cmd.motor_idx < 0 || cmd.motor_idx > 1) {
        httpd_resp_send(req, "{\"error\":\"invalid motor\"}", 26);
        return ESP_FAIL;
    }

    xQueueSend(motorQueue, &cmd, 0);

    char resp[128];
    snprintf(resp, sizeof(resp),
        "{\"motor\":%d,\"target\":%d,\"busy\":true,\"speed\":%d}",
        cmd.motor_idx, cmd.target, cmd.speed);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// GET /api/v1/motor/status
static esp_err_t motorStatusHandler(httpd_req_t* req) {
    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"pan\":{\"pos\":%d,\"busy\":%s},\"tilt\":{\"pos\":%d,\"busy\":%s}}",
        motors[0].pos, motors[0].busy ? "true" : "false",
        motors[1].pos, motors[1].busy ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /api/v1/motor/stop
static esp_err_t motorStopHandler(httpd_req_t* req) {
    char buf[64] = {0};
    char motor_str[16] = "all";

    if (req->method == HTTP_GET) {
        getQueryParam(req, "motor", motor_str, sizeof(motor_str));
    } else {
        int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = 0;
            jsonGetStr(buf, "motor", motor_str, sizeof(motor_str));
        }
    }

    int motor_idx = -1;
    if (strcmp(motor_str, "pan") == 0 || strcmp(motor_str, "0") == 0) motor_idx = 0;
    else if (strcmp(motor_str, "tilt") == 0 || strcmp(motor_str, "1") == 0) motor_idx = 1;

    stopMotor(motor_idx);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"stopped\":true}", 16);
    return ESP_OK;
}

// POST /api/v1/camera/config
static esp_err_t cameraConfigHandler(httpd_req_t* req) {
    char buf[256];

    if (req->method == HTTP_GET) {
        char on_str[8] = {0};
        getQueryParam(req, "enabled", on_str, sizeof(on_str));
        if (strlen(on_str) > 0) {
            bool enable = atoi(on_str) == 1;
            if (enable && !camera_enabled) {
                configCamera();
            } else if (!enable && camera_enabled) {
                esp_camera_deinit();
                camera_enabled = false;
            }
        }
    } else {
        int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (len <= 0) return ESP_FAIL;
        buf[len] = 0;

        int enabled_val = jsonGetInt(buf, "enabled", -1);
        char size_str[16] = {0};
        jsonGetStr(buf, "resolution", size_str, sizeof(size_str));
        int quality_val = jsonGetInt(buf, "quality", -1);

        bool enable = (enabled_val == 1);

        if (enable && !camera_enabled) {
            configCamera();
        } else if (!enable && camera_enabled) {
            esp_camera_deinit();
            camera_enabled = false;
        }

        if (camera_enabled && strlen(size_str) > 0) {
            sensor_t* s = esp_camera_sensor_get();
            if (s) {
                framesize_t fs = getFrameSize(size_str);
                s->set_framesize(s, fs);
            }
        }

        if (camera_enabled && quality_val > 0) {
            sensor_t* s = esp_camera_sensor_get();
            if (s) {
                s->set_quality(s, quality_val);
            }
        }
    }

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"enabled\":%s}", camera_enabled ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// GET /api/v1/camera/capture
static esp_err_t cameraCaptureHandler(httpd_req_t* req) {
    Serial.println("[CAM] capture request");

    bool was_enabled = camera_enabled;
    if (!camera_enabled) {
        if (configCamera() != ESP_OK) {
            httpd_resp_send(req, "{\"error\":\"camera init failed\"}", 32);
            return ESP_FAIL;
        }
        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        if (!was_enabled) {
            esp_camera_deinit();
            camera_enabled = false;
        }
        httpd_resp_send(req, "{\"error\":\"capture failed\"}", 27);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);

    if (!was_enabled) {
        esp_camera_deinit();
        camera_enabled = false;
        Serial.println("[CAM] capture done, camera off");
    }

    return ESP_OK;
}

// GET /api/v1/camera/preview
static esp_err_t cameraPreviewHandler(httpd_req_t* req) {
    if (!camera_enabled) {
        httpd_resp_send(req, "{\"error\":\"camera disabled\"}", 27);
        return ESP_FAIL;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send(req, "{\"error\":\"capture failed\"}", 27);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);

    return ESP_OK;
}

// POST /api/v1/led/effect
static esp_err_t ledEffectHandler(httpd_req_t* req) {
    LedCommand cmd;
    cmd.led_idx = -1;
    strncpy(cmd.effect, "static", 15);
    cmd.effect[15] = 0;
    cmd.color = 0xFF0000;
    cmd.speed = 50;
    cmd.brightness = 100;  // 默认全亮

    char buf[256];
    if (req->method == HTTP_GET) {
        char target_str[8] = {0};
        getQueryParam(req, "target", target_str, sizeof(target_str));
        cmd.led_idx = atoi(target_str);
        char effect_str[32] = {0};
        if (getQueryParam(req, "effect", effect_str, sizeof(effect_str)) == 0) {
            strncpy(cmd.effect, effect_str, 15);
            cmd.effect[15] = 0;
        }
        char color_str[16] = {0};
        if (getQueryParam(req, "color", color_str, sizeof(color_str)) == 0) {
            cmd.color = parseHexColor(color_str);
        }
        cmd.speed = getQueryParamInt(req, "speed", 50);
        cmd.brightness = getQueryParamInt(req, "brightness", 100);
    } else {
        int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (len <= 0) return ESP_FAIL;
        buf[len] = 0;

        cmd.led_idx = jsonGetInt(buf, "target", -1);
        cmd.speed = jsonGetInt(buf, "speed", 50);
        cmd.brightness = jsonGetInt(buf, "brightness", 100);

        char effect_str[32] = {0};
        if (jsonGetStr(buf, "effect", effect_str, sizeof(effect_str))) {
            strncpy(cmd.effect, effect_str, 15);
            cmd.effect[15] = 0;
        }

        char color_str[16] = {0};
        if (jsonGetStr(buf, "color", color_str, sizeof(color_str))) {
            cmd.color = parseHexColor(color_str);
        }
    }

    xQueueSend(ledQueue, &cmd, 0);

    char resp[128];
    snprintf(resp, sizeof(resp),
        "{\"led\":%d,\"effect\":\"%s\",\"color\":\"%06X\",\"speed\":%d,\"brightness\":%d}",
        cmd.led_idx, cmd.effect, cmd.color, cmd.speed, cmd.brightness);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /api/v1/led/segments
static esp_err_t ledSegmentsHandler(httpd_req_t* req) {
    char buf[512];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = 0;

    // 直接在 JSON 中查找 "color":" 后的 6 位十六进制
    char* p = buf;
    while ((p = strstr(p, "\"color\":\"")) != NULL) {
        p += 9; // 跳过 "\"color\":\""
        char color_str[8] = {0};
        for (int i = 0; i < 6 && *p && *p != '"'; i++) {
            color_str[i] = *p++;
        }
        uint32_t color = parseHexColor(color_str);

        // 找到对应的 id
        char* id_p = buf;
        int found_id = -1;
        while ((id_p = strstr(id_p, "\"id\":")) != NULL && id_p < p) {
            id_p += 5;
            int id = atoi(id_p);
            if (id >= 0 && id <= 1) {
                // 确认这个 id 在 color 之前
                char* next_id = strstr(id_p, "\"id\":");
                if (next_id == NULL || next_id > p) {
                    found_id = id;
                    break;
                }
                id_p++;
            }
        }

        if (found_id >= 0) {
            strncpy(leds[found_id].effect, "static", 15);
            leds[found_id].effect[15] = 0;
            leds[found_id].color = color;
            setLedColor(found_id, color);
        }
        p++;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", 10);
    return ESP_OK;
}

// GET /api/v1/led/status
static esp_err_t ledStatusHandler(httpd_req_t* req) {
    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"0\":{\"effect\":\"%s\",\"color\":\"%06X\",\"speed\":%d,\"brightness\":%d},"
        "\"1\":{\"effect\":\"%s\",\"color\":\"%06X\",\"speed\":%d,\"brightness\":%d}}",
        leds[0].effect, leds[0].color, leds[0].speed, leds[0].brightness,
        leds[1].effect, leds[1].color, leds[1].speed, leds[1].brightness);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// ========== OLED Handlers ==========

// POST /api/v1/oled/text - Display text
// Body: {"text":"Hello","x":0,"y":0,"size":1} or {"text":"Line1","line":0,"size":2}
static esp_err_t oledTextHandler(httpd_req_t* req) {
    char buf[512];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = 0;

    char text[256] = {0};
    int x = -1, y = -1, size = 1;
    int line = -1;

    jsonGetStr(buf, "text", text, sizeof(text));
    x = jsonGetInt(buf, "x", -1);
    y = jsonGetInt(buf, "y", -1);
    line = jsonGetInt(buf, "line", -1);
    size = jsonGetInt(buf, "size", 1);

    Serial.printf("[OLED] text='%s' line=%d size=%d\n", text, line, size);

    // Re-init I2C bus in case camera claimed it
    Wire.end();
    delayMicroseconds(50);
    Wire.begin(1, 2);

    display.clearDisplay();
    display.setRotation(oled_rotation);
    display.setTextSize(size);
    display.setTextColor(SSD1306_WHITE);

    if (line >= 0) {
        y = line * 8 * size;
        x = 0;
    }

    display.setCursor(x >= 0 ? x : 0, y >= 0 ? y : 0);
    display.println(text);
    display.display();

    char resp[256];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"text\":\"%s\",\"x\":%d,\"y\":%d,\"size\":%d}", text, x, y, size);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /api/v1/oled/clear
static esp_err_t oledClearHandler(httpd_req_t* req) {
    display.clearDisplay();
    display.display();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", 10);
    return ESP_OK;
}

// POST /api/v1/oled/rotation - Set rotation 0-3
// Body: {"rotation":0}  0=0°, 1=90°, 2=180°, 3=270°
static esp_err_t oledRotationHandler(httpd_req_t* req) {
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = 0;

    int rotation = jsonGetInt(buf, "rotation", -1);
    if (rotation < 0 || rotation > 3) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"rotation must be 0-3\"}", 32);
        return ESP_FAIL;
    }

    display.clearDisplay();
    display.display();
    oled_rotation = rotation;
    display.setRotation(oled_rotation);
    display.clearDisplay();
    display.display();

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"rotation\":%d}", oled_rotation);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /api/v1/oled/draw - Draw shapes
// Body: {"cmd":"line","x0":0,"y0":0,"x1":50,"y1":50,"color":"FFFFFF"}
// Body: {"cmd":"rect","x":10,"y":10,"w":50,"h":30,"color":"FF0000"}
// Body: {"cmd":"circle","x":64,"y":32,"r":20,"color":"0000FF"}
// Body: {"cmd":"pixel","x":10,"y":10,"color":"FFFFFF"}
static esp_err_t oledDrawHandler(httpd_req_t* req) {
    char buf[512];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = 0;

    char cmd[32] = {0};
    jsonGetStr(buf, "cmd", cmd, sizeof(cmd));

    uint32_t color = 0xFFFFFF;  // Default white
    char color_str[16] = {0};
    if (jsonGetStr(buf, "color", color_str, sizeof(color_str))) {
        color = parseHexColor(color_str);
    }

    display.setRotation(oled_rotation);
    display.clearDisplay();

    if (strcmp(cmd, "pixel") == 0) {
        int x = jsonGetInt(buf, "x", -1);
        int y = jsonGetInt(buf, "y", -1);
        if (x < 0 || y < 0) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"error\":\"x,y required\"}", 24);
            return ESP_FAIL;
        }
        display.drawPixel(x, y, color);
        display.display();
        httpd_resp_set_type(req, "application/json");
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"cmd\":\"pixel\",\"x\":%d,\"y\":%d}", x, y);
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }
    else if (strcmp(cmd, "line") == 0) {
        int x0 = jsonGetInt(buf, "x0", 0);
        int y0 = jsonGetInt(buf, "y0", 0);
        int x1 = jsonGetInt(buf, "x1", 0);
        int y1 = jsonGetInt(buf, "y1", 0);
        display.drawLine(x0, y0, x1, y1, color);
        display.display();
        httpd_resp_set_type(req, "application/json");
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"cmd\":\"line\",\"x0\":%d,\"y0\":%d,\"x1\":%d,\"y1\":%d}", x0, y0, x1, y1);
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }
    else if (strcmp(cmd, "rect") == 0) {
        int x = jsonGetInt(buf, "x", 0);
        int y = jsonGetInt(buf, "y", 0);
        int w = jsonGetInt(buf, "w", 10);
        int h = jsonGetInt(buf, "h", 10);
        bool fill = jsonGetInt(buf, "fill", 0) == 1;
        if (fill) {
            display.fillRect(x, y, w, h, color);
        } else {
            display.drawRect(x, y, w, h, color);
        }
        display.display();
        httpd_resp_set_type(req, "application/json");
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"cmd\":\"rect\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"fill\":%s}", x, y, w, h, fill ? "true" : "false");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }
    else if (strcmp(cmd, "circle") == 0) {
        int x = jsonGetInt(buf, "x", 0);
        int y = jsonGetInt(buf, "y", 0);
        int r = jsonGetInt(buf, "r", 10);
        bool fill = jsonGetInt(buf, "fill", 0) == 1;
        if (fill) {
            display.fillCircle(x, y, r, color);
        } else {
            display.drawCircle(x, y, r, color);
        }
        display.display();
        httpd_resp_set_type(req, "application/json");
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"cmd\":\"circle\",\"x\":%d,\"y\":%d,\"r\":%d,\"fill\":%s}", x, y, r, fill ? "true" : "false");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }
    else if (strcmp(cmd, "triangle") == 0) {
        int x0 = jsonGetInt(buf, "x0", 0);
        int y0 = jsonGetInt(buf, "y0", 0);
        int x1 = jsonGetInt(buf, "x1", 0);
        int y1 = jsonGetInt(buf, "y1", 0);
        int x2 = jsonGetInt(buf, "x2", 0);
        int y2 = jsonGetInt(buf, "y2", 0);
        bool fill = jsonGetInt(buf, "fill", 0) == 1;
        if (fill) {
            display.fillTriangle(x0, y0, x1, y1, x2, y2, color);
        } else {
            display.drawTriangle(x0, y0, x1, y1, x2, y2, color);
        }
        display.display();
        httpd_resp_set_type(req, "application/json");
        char resp[160];
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"cmd\":\"triangle\",\"x0\":%d,\"y0\":%d,\"x1\":%d,\"y1\":%d,\"x2\":%d,\"y2\":%d}", x0, y0, x1, y1, x2, y2);
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }
    else if (strcmp(cmd, "lineh") == 0) {
        // Horizontal line
        int x = jsonGetInt(buf, "x", 0);
        int y = jsonGetInt(buf, "y", 0);
        int w = jsonGetInt(buf, "w", 50);
        display.drawFastHLine(x, y, w, color);
        display.display();
        httpd_resp_set_type(req, "application/json");
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"cmd\":\"lineh\",\"x\":%d,\"y\":%d,\"w\":%d}", x, y, w);
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }
    else if (strcmp(cmd, "linev") == 0) {
        // Vertical line
        int x = jsonGetInt(buf, "x", 0);
        int y = jsonGetInt(buf, "y", 0);
        int h = jsonGetInt(buf, "h", 50);
        display.drawFastVLine(x, y, h, color);
        display.display();
        httpd_resp_set_type(req, "application/json");
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"cmd\":\"linev\",\"x\":%d,\"y\":%d,\"h\":%d}", x, y, h);
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }
    else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"unknown cmd\"}", 24);
        return ESP_FAIL;
    }
}

// ========== Audio Handlers ==========

// POST /api/v1/audio/speaker/test - Play test tone
static esp_err_t audioSpeakerTestHandler(httpd_req_t* req) {
    Serial.println("[SPK] test requested");
    initSpeaker();
    playTestTone();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true,\"speaker\":true}", 20);
    return ESP_OK;
}

// GET /api/v1/audio/mic/test - Read mic level
static esp_err_t audioMicTestHandler(httpd_req_t* req) {
    Serial.println("[MIC] test requested");
    initMic();
    int level = readMicLevel();
    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"mic\":true,\"level\":%d}", level);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// GET /api/v1/audio/status
static esp_err_t audioStatusHandler(httpd_req_t* req) {
    char resp[128];
    snprintf(resp, sizeof(resp), "{\"speaker\":%s,\"mic\":%s}",
        speakerEnabled ? "true" : "false",
        micEnabled ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// GET /api/v1/oled/status
static esp_err_t oledStatusHandler(httpd_req_t* req) {
    char resp[128];
    snprintf(resp, sizeof(resp), "{\"width\":%d,\"height\":%d,\"rotation\":%d}",
        OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, oled_rotation);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// GET /api/v1/status
static esp_err_t statusHandler(httpd_req_t* req) {
    float v = readBatteryVoltage();
    int pct = voltageToPercent(v);
    bool charging = isCharging();
    unsigned long uptime = (millis() - boot_time) / 1000;

    char resp[1024];
    snprintf(resp, sizeof(resp),
        "{\"version\":\"%s\",\"uptime\":%lu,\"memory\":{\"free\":%d,\"psram\":%d},"
        "\"wifi\":{\"rssi\":%d},"
        "\"motor\":{\"pan\":{\"pos\":%d,\"busy\":%s},\"tilt\":{\"pos\":%d,\"busy\":%s}},"
        "\"camera\":{\"enabled\":%s},"
        "\"led\":{\"0\":{\"effect\":\"%s\",\"color\":\"%06X\"},\"1\":{\"effect\":\"%s\",\"color\":\"%06X\"}},"
        "\"oled\":{\"rotation\":%d},"
        "\"battery\":{\"voltage\":%.2f,\"percent\":%d,\"charging\":%s}}",
        VERSION, uptime, esp_get_free_heap_size(), 0,
        WiFi.RSSI(),
        motors[0].pos, motors[0].busy ? "true" : "false",
        motors[1].pos, motors[1].busy ? "true" : "false",
        camera_enabled ? "true" : "false",
        leds[0].effect, leds[0].color,
        leds[1].effect, leds[1].color,
        oled_rotation,
        v, pct, charging ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /api/v1/control/selfcheck
static esp_err_t selfcheckHandler(httpd_req_t* req) {
    char resp[256];
    bool motor_ok = !motors[0].busy && !motors[1].busy;
    bool camera_ok = true;

    snprintf(resp, sizeof(resp),
        "{\"motor\":\"%s\",\"camera\":\"%s\",\"led\":\"ok\",\"oled\":\"ok\",\"memory\":%d}",
        motor_ok ? "ok" : "busy",
        camera_ok ? "ok" : "error",
        esp_get_free_heap_size());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /api/v1/control/reboot
static esp_err_t rebootHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"rebooting\"}", 20);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}

// GET /api/v1/ping
static esp_err_t pingHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", 10);
    return ESP_OK;
}

// GET /api/v1/debug - 调试端点
static esp_err_t debugHandler(httpd_req_t* req) {
    char resp[512];
    snprintf(resp, sizeof(resp),
        "{\"motorQueue\":%d,\"ledQueue\":%d,\"motorState\":0x%02X,"
        "\"motor1Seq\":%d,\"motor2Seq\":%d,"
        "\"m0_pos\":%d,\"m0_busy\":%s,\"m0_speed\":%d,"
        "\"m1_pos\":%d,\"m1_busy\":%s,\"m1_speed\":%d}",
        motorQueue ? uxQueueMessagesWaiting(motorQueue) : -1,
        ledQueue ? uxQueueMessagesWaiting(ledQueue) : -1,
        motorState,
        motor1Seq, motor2Seq,
        motors[0].pos, motors[0].busy ? "true" : "false", motors[0].speed,
        motors[1].pos, motors[1].busy ? "true" : "false", motors[1].speed);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// ========== 临时调试端点 ==========

// ========== HTTP 路由表 ==========
static const httpd_uri_t motorRelativeUri = {"/api/v1/motor/relative", HTTP_POST, motorRelativeHandler};
static const httpd_uri_t motorAbsoluteUri = {"/api/v1/motor/absolute", HTTP_POST, motorAbsoluteHandler};
static const httpd_uri_t motorStatusUri = {"/api/v1/motor/status", HTTP_GET, motorStatusHandler};
static const httpd_uri_t motorStopUri = {"/api/v1/motor/stop", HTTP_POST, motorStopHandler};

static const httpd_uri_t cameraConfigUri = {"/api/v1/camera/config", HTTP_POST, cameraConfigHandler};
static const httpd_uri_t cameraCaptureUri = {"/api/v1/camera/capture", HTTP_GET, cameraCaptureHandler};
static const httpd_uri_t cameraPreviewUri = {"/api/v1/camera/preview", HTTP_GET, cameraPreviewHandler};

static const httpd_uri_t ledEffectUri = {"/api/v1/led/effect", HTTP_POST, ledEffectHandler};
static const httpd_uri_t ledSegmentsUri = {"/api/v1/led/segments", HTTP_POST, ledSegmentsHandler};
static const httpd_uri_t ledStatusUri = {"/api/v1/led/status", HTTP_GET, ledStatusHandler};

// OLED URIs
static const httpd_uri_t oledTextUri = {"/api/v1/oled/text", HTTP_POST, oledTextHandler};
static const httpd_uri_t oledClearUri = {"/api/v1/oled/clear", HTTP_POST, oledClearHandler};
static const httpd_uri_t oledRotationUri = {"/api/v1/oled/rotation", HTTP_POST, oledRotationHandler};
static const httpd_uri_t oledDrawUri = {"/api/v1/oled/draw", HTTP_POST, oledDrawHandler};
static const httpd_uri_t oledStatusUri = {"/api/v1/oled/status", HTTP_GET, oledStatusHandler};

// Audio URIs
static const httpd_uri_t audioSpeakerTestUri = {"/api/v1/audio/speaker/test", HTTP_POST, audioSpeakerTestHandler};
static const httpd_uri_t audioMicTestUri = {"/api/v1/audio/mic/test", HTTP_GET, audioMicTestHandler};
static const httpd_uri_t audioStatusUri = {"/api/v1/audio/status", HTTP_GET, audioStatusHandler};

static const httpd_uri_t statusUri = {"/api/v1/status", HTTP_GET, statusHandler};
static const httpd_uri_t selfcheckUri = {"/api/v1/control/selfcheck", HTTP_POST, selfcheckHandler};
static const httpd_uri_t rebootUri = {"/api/v1/control/reboot", HTTP_POST, rebootHandler};
static const httpd_uri_t pingUri = {"/api/v1/ping", HTTP_GET, pingHandler};
static const httpd_uri_t debugUri = {"/api/v1/debug", HTTP_GET, debugHandler};

// 兼容旧路径
static const httpd_uri_t captureCompatUri = {"/capture", HTTP_GET, cameraCaptureHandler};
static const httpd_uri_t imageCompatUri = {"/image", HTTP_GET, cameraCaptureHandler};

// ========== Web 服务器启动 ==========
void startWebServer() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 32;
    cfg.stack_size = 8192;

    Serial.println("[HTTP] server starting...");

    if (httpd_start(&httpServer, &cfg) == ESP_OK) {
        httpd_register_uri_handler(httpServer, &motorRelativeUri);
        httpd_register_uri_handler(httpServer, &motorAbsoluteUri);
        httpd_register_uri_handler(httpServer, &motorStatusUri);
        httpd_register_uri_handler(httpServer, &motorStopUri);

        httpd_register_uri_handler(httpServer, &cameraConfigUri);
        httpd_register_uri_handler(httpServer, &cameraCaptureUri);
        httpd_register_uri_handler(httpServer, &cameraPreviewUri);

        httpd_register_uri_handler(httpServer, &ledEffectUri);
        httpd_register_uri_handler(httpServer, &ledSegmentsUri);
        httpd_register_uri_handler(httpServer, &ledStatusUri);

        httpd_register_uri_handler(httpServer, &oledTextUri);
        httpd_register_uri_handler(httpServer, &oledClearUri);
        httpd_register_uri_handler(httpServer, &oledRotationUri);
        httpd_register_uri_handler(httpServer, &oledDrawUri);
        httpd_register_uri_handler(httpServer, &oledStatusUri);

        httpd_register_uri_handler(httpServer, &audioSpeakerTestUri);
        httpd_register_uri_handler(httpServer, &audioMicTestUri);
        httpd_register_uri_handler(httpServer, &audioStatusUri);

        httpd_register_uri_handler(httpServer, &statusUri);
        httpd_register_uri_handler(httpServer, &selfcheckUri);
        httpd_register_uri_handler(httpServer, &rebootUri);
        httpd_register_uri_handler(httpServer, &pingUri);
        httpd_register_uri_handler(httpServer, &debugUri);

        httpd_register_uri_handler(httpServer, &captureCompatUri);
        httpd_register_uri_handler(httpServer, &imageCompatUri);

        Serial.println("[HTTP] server started");
    } else {
        Serial.println("[HTTP] server FAILED");
    }
}

// ========== WiFi ==========
void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[WiFi] connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
}

// ========== Setup ==========
void setup() {
    Serial.begin(115200);
    delay(1000);
    boot_time = millis();

    Serial.println("\n=== ESP32 Robot Firmware v1.1.0 ===");

    pinMode(MOTOR_SHCP, OUTPUT);
    pinMode(MOTOR_STCP, OUTPUT);
    pinMode(MOTOR_DS, OUTPUT);
    digitalWrite(MOTOR_SHCP, LOW);
    digitalWrite(MOTOR_STCP, LOW);
    digitalWrite(MOTOR_DS, LOW);
    motorState = 0;
    updateMotorState();

    // ========== OLED 初始化 ==========
    Wire.begin(1, 2);  // GPIO1=SDA, GPIO2=SCL (PCB丝印可能是反的，实际SDA=1, SCL=2)
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("[OLED] init failed");
    } else {
        Serial.println("[OLED] OK");
        display.clearDisplay();  // 清除 Adafruit splash screen
        display.display();
        delay(100);
        display.setRotation(oled_rotation);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("ESP32 Robot");
        display.setTextSize(2);
        display.printf("v1.1.0");
        display.display();
        delay(2000);  // 显示 2 秒后继续
        display.clearDisplay();  // 清除，准备被后续 API 调用
        display.display();
    }

    initLED();
    setLedColor(0, 0xFF0000);
    setLedColor(1, 0x00FF00);
    delay(500);

    initBattery();
    delay(500);

    initWiFi();
    delay(500);

    motorQueue = xQueueCreateStatic(8, sizeof(MotorCommand), motorQueueBuffer, &motorQueueStruct);
    ledQueue = xQueueCreateStatic(8, sizeof(LedCommand), ledQueueBuffer, &ledQueueStruct);

    xTaskCreatePinnedToCore(motorTask, "motor", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(ledTask, "led", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(monitorTask, "monitor", 4096, NULL, 1, NULL, 1);

    startWebServer();

    Serial.println("\n=== Ready ===");
    Serial.printf("IP: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Motor status: /api/v1/motor/status\n");
    Serial.printf("Camera capture: /api/v1/camera/capture\n");
    Serial.printf("LED control: /api/v1/led/effect\n");
    Serial.printf("System status: /api/v1/status\n");
}

void loop() {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
