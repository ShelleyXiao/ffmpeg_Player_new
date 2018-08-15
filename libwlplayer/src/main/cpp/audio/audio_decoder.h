//
// Created by ywl on 2017-12-3.
//
#pragma once
#ifndef WLPLAYER_WLAUDIO_H
#define WLPLAYER_WLAUDIO_H


#include "../base_player.h"
#include "../common/WlQueue.h"
#include "../common/AndroidLog.h"
#include "../WlPlayStatus.h"
#include "../JavaJNICallback.h"
#include "audio_output.h"

extern "C"
{
#include "libswresample/swresample.h"
#include "libavutil/time.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
};

class AudioDecoder : public ZXbasePlayer{

public:
    WlQueue *queue = NULL;
    WlPlayStatus *wlPlayStatus = NULL;
    JavaJNICallback *javaJNICall = NULL;
    pthread_t audioThread;

    int ret = 0;//函数调用返回结果
    int64_t dst_layout = 0;//重采样为立体声
    int dst_nb_samples = 0;// 计算转换后的sample个数 a * b / c
    int nb = 0;//转换，返回值为转换后的sample个数
    uint8_t *out_buffer = NULL;//buffer 内存区域
    int out_channels = 0;//输出声道数
    int data_size = 0;//buffer大小
    enum AVSampleFormat dst_format;
    //opensl es

    void *buffer = NULL;
    int pcmsize = 0;
    int sample_rate = 44100;
    int channels = 2;
    bool isExit = false;
    bool isVideo = false;

    bool isReadPacketFinish = true;
    AVPacket *packet;


public:
    AudioDecoder(WlPlayStatus *playStatus, JavaJNICallback *javaCall);
    ~AudioDecoder();

    void setVideo(bool video);

    void playAudio();
    int getPcmData(void **pcm);
    int initOpenSL();
    void pause();
    void resume();
    void realease();
    void setClock(int secds);

    //当音频播放器播放完毕一段buffer之后，会回调这个方法，这个方法要做的就是用数据将这个buffer再填充起来

    AudioOutput* audioOutput;
    bool initAudioOutput();

};


#endif //WLPLAYER_WLAUDIO_H
