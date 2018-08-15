//
// Created by ywl on 2017-12-3.
//

#include <opensl_media/opensl_es_context.h>
#include "WlAudio.h"

#define LOG_TAG "AUDIO"

WlAudio::WlAudio(WlPlayStatus *playStatus, WlJavaCall *javaCall) {
    streamIndex = -1;
    out_buffer = (uint8_t *) malloc(sample_rate * 2 * 2 * 2 / 3);
    queue = new WlQueue(playStatus);
    wlPlayStatus = playStatus;
    wljavaCall = javaCall;
    dst_format = AV_SAMPLE_FMT_S16;
}

WlAudio::~WlAudio() {
    if (LOG_SHOW) {
        LOGE("~WlAudio() 释放完了");
    }
}

void WlAudio::realease() {
    if (LOG_SHOW) {
        LOGE("开始释放 audio...");
    }
    pause();
    if (queue != NULL) {
        queue->noticeThread();
    }
    int count = 0;
    while (!isExit) {
        if (LOG_SHOW) {
            LOGD("等待缓冲线程结束...%d", count);
        }
        if (count > 1000) {
            isExit = true;
        }
        count++;
        av_usleep(1000 * 10);
    }
    if (queue != NULL) {
        queue->release();
        delete (queue);
        queue = NULL;
    }

    LOG_DEBUG("释放 opensl es start");

    if (pcmPlayerObject != NULL) {
        (*pcmPlayerObject)->Destroy(pcmPlayerObject);
        pcmPlayerObject = NULL;
        pcmPlayerPlay = NULL;
        pcmPlayerVolume = NULL;
        pcmBufferQueue = NULL;
        buffer = NULL;
        pcmsize = 0;
    }
    if (LOG_SHOW) {
        LOGE("释放 opensl es end 1");
    }
    // destroy output mix object, and invalidate all associated interfaces
    if (outputMixObject != NULL) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
        outputMixEnvironmentalReverb = NULL;
    }

    if (LOG_SHOW) {
        LOGE("释放 opensl es end 2");
    }

    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }

    if (audioOutput != NULL) {
        audioOutput->destroyContext();
    }

    if (LOG_SHOW) {
        LOGE("释放 opensl es end");
    }


    if (out_buffer != NULL) {
        free(out_buffer);
        out_buffer = NULL;
    }
    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }
    if (avCodecContext != NULL) {
        avcodec_close(avCodecContext);
        avcodec_free_context(&avCodecContext);
        avCodecContext = NULL;
    }
    if (wlPlayStatus != NULL) {
        wlPlayStatus = NULL;
    }

}

void *audioPlayThread(void *context) {
    WlAudio *audio = (WlAudio *) context;
    //audio->initOpenSL();
    audio->initAudioOutput();
    pthread_exit(&audio->audioThread);
}

void WlAudio::playAudio() {
    pthread_create(&audioThread, NULL, audioPlayThread, this);
}

int WlAudio::getPcmData(void **pcm) {
    while (!wlPlayStatus->exit) {
        isExit = false;

        if (wlPlayStatus->pause)//暂停
        {
            av_usleep(1000 * 100);
            continue;
        }
        if (wlPlayStatus->seek) {
            wljavaCall->onLoad(WL_THREAD_CHILD, true);
            wlPlayStatus->load = true;
            isReadPacketFinish = true;
            continue;
        }
        if (!isVideo) {
            if (queue->getAvPacketSize() == 0)//加载
            {
                if (!wlPlayStatus->load) {
                    wljavaCall->onLoad(WL_THREAD_CHILD, true);
                    wlPlayStatus->load = true;
                }
                continue;
            } else {
                if (wlPlayStatus->load) {
                    wljavaCall->onLoad(WL_THREAD_CHILD, false);
                    wlPlayStatus->load = false;
                }
            }
        }
        if (isReadPacketFinish) {
            isReadPacketFinish = false;
            packet = av_packet_alloc();
            if (queue->getAvpacket(packet) != 0) {
                av_packet_free(&packet);
                av_free(packet);
                packet = NULL;
                isReadPacketFinish = true;
                continue;
            }
            ret = avcodec_send_packet(avCodecContext, packet);
            if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                av_packet_free(&packet);
                av_free(packet);
                packet = NULL;
                isReadPacketFinish = true;
                continue;
            }
        }

        AVFrame *frame = av_frame_alloc();
        if (avcodec_receive_frame(avCodecContext, frame) == 0) {
            // 设置通道数或channel_layout
            if (frame->channels > 0 && frame->channel_layout == 0)
                frame->channel_layout = av_get_default_channel_layout(frame->channels);
            else if (frame->channels == 0 && frame->channel_layout > 0)
                frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);

            SwrContext *swr_ctx;
            //重采样为立体声
            dst_layout = AV_CH_LAYOUT_STEREO;
            // 设置转换参数
            swr_ctx = swr_alloc_set_opts(NULL, dst_layout, dst_format, frame->sample_rate,
                                         frame->channel_layout,
                                         (enum AVSampleFormat) frame->format,
                                         frame->sample_rate, 0, NULL);
            if (!swr_ctx || (ret = swr_init(swr_ctx)) < 0) {
                av_frame_free(&frame);
                av_free(frame);
                frame = NULL;
                swr_free(&swr_ctx);
                av_packet_free(&packet);
                av_free(packet);
                packet = NULL;
                continue;
            }
            // 计算转换后的sample个数 a * b / c
            dst_nb_samples = (int) av_rescale_rnd(
                    swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples,
                    frame->sample_rate, frame->sample_rate, AV_ROUND_INF);
            // 转换，返回值为转换后的sample个数
            nb = swr_convert(swr_ctx, &out_buffer, dst_nb_samples,
                             (const uint8_t **) frame->data, frame->nb_samples);

            //根据布局获取声道数
            out_channels = av_get_channel_layout_nb_channels(dst_layout);
            data_size = out_channels * nb * av_get_bytes_per_sample(dst_format);
            now_time = frame->pts * av_q2d(time_base);
            if (now_time < clock) {
                now_time = clock;
            }
            clock = now_time;
            av_frame_free(&frame);
            av_free(frame);
            frame = NULL;
            swr_free(&swr_ctx);
            *pcm = out_buffer;
            break;
        } else {
            isReadPacketFinish = true;
            av_frame_free(&frame);
            av_free(frame);
            frame = NULL;
            av_packet_free(&packet);
            av_free(packet);
            packet = NULL;
            continue;
        }
    }
    isExit = true;
    return data_size;
}

void pcmBufferCallBack_sl(SLAndroidSimpleBufferQueueItf bf, void *context) {
    LOG_DEBUG("DEBUG ************************** 6");
    WlAudio *wlAudio = (WlAudio *) context;
    if (wlAudio != NULL) {
        if (LOG_SHOW) {
            LOGE("pcm call back...");
        }
        LOG_DEBUG("DEBUG ************************** 4");
        wlAudio->buffer = NULL;
        wlAudio->pcmsize = wlAudio->getPcmData(&wlAudio->buffer);
        LOG_DEBUG("DEBUG ************************** 1");
        if (wlAudio->buffer && wlAudio->pcmsize > 0) {
            wlAudio->clock += wlAudio->pcmsize / ((double) (wlAudio->sample_rate * 2 * 2));
            wlAudio->wljavaCall->onVideoInfo(WL_THREAD_CHILD, wlAudio->clock, wlAudio->duration);
//            (*wlAudio->pcmBufferQueue)->Enqueue(wlAudio->pcmBufferQueue, wlAudio->buffer,
//                                                wlAudio->pcmsize);
            LOG_DEBUG("DEBUG ************************** 2");
            SLAndroidSimpleBufferQueueItf pcmBufferQueue = wlAudio->audioOutput->getSLQueueItf();
            if(pcmBufferQueue != NULL) {
                (*pcmBufferQueue)->Enqueue(pcmBufferQueue,
                                           wlAudio->buffer,
                                           wlAudio->pcmsize);
            }
//
//            (*bf)->Enqueue(bf,
//                           wlAudio->buffer,
//                           wlAudio->pcmsize);
            LOG_DEBUG("DEBUG ************************** 3");

        }
    }
}

int WlAudio::audioCallbackFillData(void *outData, size_t bufferSize, void *ctx) {
    LOG_DEBUG("DEBUG ************************** 1");
    WlAudio *audio = (WlAudio *) ctx;
    int actualSize = audio->getPcmData(&outData);
    LOGD("DEBUG ************************** 2 = actualSize : %d", actualSize);
    if (outData && actualSize > 0) {
        audio->clock += audio->pcmsize / ((double) (audio->sample_rate * 2 * 2));
        audio->wljavaCall->onVideoInfo(WL_THREAD_CHILD, audio->clock, audio->duration);
        LOG_DEBUG("DEBUG ************************** 3");
    }
}

bool WlAudio::initAudioOutput() {
    LOGI("VideoPlayerController::initAudioOutput");
    audioOutput = new AudioOutput();
    SLresult result = audioOutput->initSoundTrack(channels, sample_rate, pcmBufferCallBack_sl,
                                                  this);

    if (SL_RESULT_SUCCESS != result) {
        LOGI("audio manager failed on initialized...");
        delete audioOutput;
        audioOutput = NULL;
        return false;
    }
    audioOutput->start();

    SLAndroidSimpleBufferQueueItf pcmBufferQueue = this->audioOutput->getSLQueueItf();
    if(pcmBufferQueue != NULL) {
        pcmBufferCallBack_sl(pcmBufferQueue, this);
    }

    return true;
}

int WlAudio::initOpenSL() {
    if (LOG_SHOW) {
        LOGD("initopensl");
    }
    SLresult result;

    OpenSLESContext *openSLESContext = OpenSLESContext::GetInstance();
    engineEngine = openSLESContext->getEngine();

    //第二步，创建混音器
    const SLInterfaceID mids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean mreq[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, mids, mreq);
    (void) result;
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    (void) result;
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&outputMix, 0};


    // 第三步，配置PCM格式信息
    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                            2};

    SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,//播放pcm格式的数据
            2,//2个声道（立体声）
            opensl_get_sample_rate(sample_rate),//44100hz的频率
            SL_PCMSAMPLEFORMAT_FIXED_16,//位数 16位
            SL_PCMSAMPLEFORMAT_FIXED_16,//和位数一致就行
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,//立体声（前左前右）
            SL_BYTEORDER_LITTLEENDIAN//结束标志
    };
    SLDataSource slDataSource = {&android_queue, &pcm};


    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &pcmPlayerObject, &slDataSource,
                                                &audioSnk, 3, ids, req);
    //初始化播放器
    (*pcmPlayerObject)->Realize(pcmPlayerObject, SL_BOOLEAN_FALSE);

//    得到接口后调用  获取Player接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_PLAY, &pcmPlayerPlay);

//    注册回调缓冲区 获取缓冲队列接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_BUFFERQUEUE, &pcmBufferQueue);
    //缓冲接口回调
//    (*pcmBufferQueue)->RegisterCallback(pcmBufferQueue, pcmBufferCallBack, this);
//    获取音量接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_VOLUME, &pcmPlayerVolume);

//    获取播放状态接口
    (*pcmPlayerPlay)->SetPlayState(pcmPlayerPlay, SL_PLAYSTATE_PLAYING);

//    pcmBufferCallBack(pcmBufferQueue, this);
    if (LOG_SHOW) {
        LOGE("initopensl 2");
    }
    return 0;
}


void WlAudio::pause() {
    if (pcmPlayerPlay != NULL) {
        (*pcmPlayerPlay)->SetPlayState(pcmPlayerPlay, SL_PLAYSTATE_PAUSED);
    }

}

void WlAudio::resume() {
    if (pcmPlayerPlay != NULL) {
        (*pcmPlayerPlay)->SetPlayState(pcmPlayerPlay, SL_PLAYSTATE_PLAYING);
    }
}

void WlAudio::setVideo(bool video) {
    isVideo = video;
}

void WlAudio::setClock(int secds) {
    now_time = secds;
    clock = secds;
}




