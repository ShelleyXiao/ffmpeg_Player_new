#include <opensl_media/opensl_es_context.h>
#include "audio_decoder.h"

#define LOG_TAG "AUDIO"

AudioDecoder::AudioDecoder(WlPlayStatus
                           *playStatus,
                           JavaJNICallback *javaJNICall
) {
    streamIndex = -1;
    out_buffer = (uint8_t *) malloc(sample_rate * 2 * 2 * 2 / 3);
    queue = new WlQueue(playStatus);
    wlPlayStatus = playStatus;
    this->javaJNICall = javaJNICall;
    dst_format = AV_SAMPLE_FMT_S16;
}

AudioDecoder::~AudioDecoder() {
    if (LOG_SHOW) {
        LOGE("audio_decoderecoder() 释放完了");
    }
}

void AudioDecoder::realease() {
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
    AudioDecoder *audio = (AudioDecoder *) context;
    audio->initAudioOutput();
    pthread_exit(&audio->audioThread);
}

void AudioDecoder::playAudio() {
    pthread_create(&audioThread, NULL, audioPlayThread, this);
}

int AudioDecoder::getPcmData(void **pcm) {
    while (!wlPlayStatus->exit) {
        isExit = false;

        if (wlPlayStatus->pause)//暂停
        {
            av_usleep(1000 * 100);
            continue;
        }
        if (wlPlayStatus->seek) {
            javaJNICall->onLoad(WL_THREAD_CHILD, true);
            wlPlayStatus->load = true;
            isReadPacketFinish = true;
            continue;
        }
        if (!isVideo) {
            if (queue->getAvPacketSize() == 0)//加载
            {
                if (!wlPlayStatus->load) {
                    javaJNICall->onLoad(WL_THREAD_CHILD, true);
                    wlPlayStatus->load = true;
                }
                continue;
            } else {
                if (wlPlayStatus->load) {
                    javaJNICall->onLoad(WL_THREAD_CHILD, false);
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
    AudioDecoder *audioDecoer = (AudioDecoder *) context;
    if (audioDecoer != NULL) {
        if (LOG_SHOW) {
            LOGE("pcm call back...");
        }
        LOG_DEBUG("DEBUG ************************** 4");
        audioDecoer->buffer = NULL;
        audioDecoer->pcmsize = audioDecoer->getPcmData(&audioDecoer->buffer);
        LOG_DEBUG("DEBUG ************************** 1");
        if (audioDecoer->buffer && audioDecoer->pcmsize > 0) {
            audioDecoer->clock += audioDecoer->pcmsize / ((double) (audioDecoer->sample_rate * 2 * 2));
            audioDecoer->javaJNICall->onVideoInfo(WL_THREAD_CHILD, audioDecoer->clock, audioDecoer->duration);
//            (*wlAudio->pcmBufferQueue)->Enqueue(wlAudio->pcmBufferQueue, wlAudio->buffer,
//                                                wlAudio->pcmsize);
            LOG_DEBUG("DEBUG ************************** 2");
            SLAndroidSimpleBufferQueueItf pcmBufferQueue = audioDecoer->audioOutput->getSLQueueItf();
            if (pcmBufferQueue != NULL) {
                (*pcmBufferQueue)->Enqueue(pcmBufferQueue,
                                           audioDecoer->buffer,
                                           audioDecoer->pcmsize);
            }
            LOG_DEBUG("DEBUG ************************** 3");
        }
    }
}


bool AudioDecoder::initAudioOutput() {
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
    if (pcmBufferQueue != NULL) {
        pcmBufferCallBack_sl(pcmBufferQueue, this);
    }

    return true;
}


void AudioDecoder::pause() {
    if (audioOutput != NULL) {
        audioOutput->pause();
    }

}

void AudioDecoder::resume() {
    if (audioOutput != NULL) {
        audioOutput->play();
    }
}

void AudioDecoder::setVideo(bool video) {
    isVideo = video;
}

void AudioDecoder::setClock(int secds) {
    now_time = secds;
    clock = secds;
}




