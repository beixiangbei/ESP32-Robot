#include "audio.h"
#include <driver/i2s.h>
#include <math.h>

bool speakerEnabled = false;
bool micEnabled = false;

// ========== 扬声器模块 (I2S) ==========
void initSpeaker() {
    if (speakerEnabled) {
        Serial.println("[SPK] already initialized");
        return;
    }
    
    Serial.printf("[SPK] pins BCK=%d WS=%d DOUT=%d\n", SPK_BCK, SPK_WS, SPK_DOUT);
    
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 2,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = SPK_BCK,
        .ws_io_num = SPK_WS,
        .data_out_num = SPK_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[SPK] driver fail: %d\n", err);
        return;
    }
    
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[SPK] pin fail: %d\n", err);
        i2s_driver_uninstall(I2S_NUM_0);
        return;
    }
    
    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_start(I2S_NUM_0);
    
    speakerEnabled = true;
    Serial.println("[SPK] init ok");
}

void playTestTone() {
    Serial.println("[SPK] playTestTone called");
    Serial.printf("[SPK] heap: %d\n", esp_get_free_heap_size());
    
    if (!speakerEnabled) {
        Serial.println("[SPK] calling initSpeaker");
        initSpeaker();
    }
    
    if (!speakerEnabled) {
        Serial.println("[SPK] init failed");
        return;
    }
    
    // 生成 1秒 1kHz 正弦波
    const int sampleRate = 16000;
    const int duration = 1;
    const int freq = 1000;
    const int samples = sampleRate * duration;
    
    int16_t* buffer = (int16_t*)malloc(samples * 2);
    if (!buffer) {
        Serial.println("[SPK] buffer fail");
        return;
    }
    
    for (int i = 0; i < samples; i++) {
        float t = (float)i / sampleRate;
        buffer[i] = (int16_t)(sin(2 * 3.14159 * freq * t) * 16000);
    }
    
    Serial.println("[SPK] writing...");
    size_t written = 0;
    i2s_write(I2S_NUM_0, buffer, samples * 2, &written, portMAX_DELAY);
    Serial.printf("[SPK] written %d bytes\n", written);
    free(buffer);
    
    Serial.println("[SPK] test tone played");
}

// ========== 麦克风模块 (I2S) ==========
void initMic() {
    if (micEnabled) {
        Serial.println("[MIC] already initialized");
        return;
    }
    
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 128,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = MIC_BCK,
        .ws_io_num = MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_DIN
    };
    
    esp_err_t err = i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[MIC] driver fail: %d\n", err);
        return;
    }
    
    err = i2s_set_pin(I2S_NUM_1, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[MIC] pin fail: %d\n", err);
        i2s_driver_uninstall(I2S_NUM_1);
        return;
    }
    
    i2s_zero_dma_buffer(I2S_NUM_1);
    i2s_start(I2S_NUM_1);
    
    micEnabled = true;
    Serial.println("[MIC] init ok");
}

int readMicLevel() {
    Serial.println("[MIC] readMicLevel called");
    
    if (!micEnabled) {
        Serial.println("[MIC] calling initMic");
        initMic();
    }
    
    if (!micEnabled) {
        Serial.println("[MIC] init failed");
        return 0;
    }
    
    int32_t buffer[512];
    size_t bytesRead = 0;
    
    Serial.println("[MIC] reading...");
    esp_err_t err = i2s_read(I2S_NUM_1, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
    
    if (err != ESP_OK) {
        Serial.printf("[MIC] read fail: %d\n", err);
        return 0;
    }
    
    if (bytesRead == 0) {
        Serial.println("[MIC] no data");
        return 0;
    }
    
    Serial.printf("[MIC] read %d bytes\n", bytesRead);
    
    // 打印左右声道
    if (bytesRead >= 8) {
        Serial.printf("[MIC] L=%d R=%d\n", buffer[0], buffer[1]);
    }
    
    // 计算音量 RMS
    int count = bytesRead / 4;  // 32bit
    int32_t sum = 0;
    for (int i = 0; i < count; i++) {
        int32_t s = buffer[i];
        sum += s * s;
    }
    
    int rms = (int)(sqrt((double)sum / count) / 65536);
    if (rms > 255) rms = 255;
    if (rms < 0) rms = 0;
    
    Serial.printf("[MIC] level: %d\n", rms);
    return rms;
}
