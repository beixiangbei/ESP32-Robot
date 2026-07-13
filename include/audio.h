#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>
#include <driver/i2s.h>

// 扬声器引脚 (I2S_NUM_0)
#define SPK_BCK  10
#define SPK_WS   9
#define SPK_DOUT 11

// 麦克风引脚 (I2S_NUM_1)
#define MIC_BCK  12
#define MIC_WS   14
#define MIC_DIN  13

// 状态标志
extern bool speakerEnabled;
extern bool micEnabled;

// 扬声器函数
void initSpeaker();
void playTestTone();
void deinitSpeaker();

// 麦克风函数
void initMic();
int readMicLevel();
void deinitMic();

#endif
