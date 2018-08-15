

#include "video_player_control.h"

#define LOG_TAG "VideoPlayerControl"

void *decodeThread(void *data) {
    VideoPlayerControl *wlFFmpeg = (VideoPlayerControl *) data;
    wlFFmpeg->decodeFFmpeg();
    pthread_exit(&wlFFmpeg->decodThread);
}


int VideoPlayerControl::preparedFFmpeg() {
    pthread_create(&decodThread, NULL, decodeThread, this);
    return 0;
}

VideoPlayerControl::VideoPlayerControl(JavaJNICallback *javaJNICall, const char *url, bool onlymusic) {
    pthread_mutex_init(&init_mutex, NULL);
    pthread_mutex_init(&seek_mutex, NULL);
    exitByUser = false;
    isOnlyMusic = onlymusic;
    javaJNICallback = javaJNICall;
    urlpath = url;
    wlPlayStatus = new WlPlayStatus();
}

int avformat_interrupt_cb(void *ctx) {
    VideoPlayerControl *playerControl = (VideoPlayerControl *) ctx;
    if (playerControl->wlPlayStatus->exit) {
        if (LOG_SHOW) {
            LOGE("avformat_interrupt_cb return 1")
        }
        return AVERROR_EOF;
    }
    if (LOG_SHOW) {
        LOGE("avformat_interrupt_cb return 0")
    }
    return 0;
}

int VideoPlayerControl::decodeFFmpeg() {
    pthread_mutex_lock(&init_mutex);
    exit = false;
    isavi = false;
    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();
    if (avformat_open_input(&pFormatCtx, urlpath, NULL, NULL) != 0) {
        if (LOG_SHOW) {
            LOGE("can not open url:%s", urlpath);
        }
        if (javaJNICallback != NULL) {
            javaJNICallback->onError(WL_THREAD_CHILD, WL_FFMPEG_CAN_NOT_OPEN_URL, "can not open url");
        }
        exit = true;
        pthread_mutex_unlock(&init_mutex);
        return -1;
    }
    pFormatCtx->interrupt_callback.callback = avformat_interrupt_cb;
    pFormatCtx->interrupt_callback.opaque = this;

    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        if (LOG_SHOW) {
            LOGE("can not find streams from %s", urlpath);
        }
        if (javaJNICallback != NULL) {
            javaJNICallback->onError(WL_THREAD_CHILD, WL_FFMPEG_CAN_NOT_FIND_STREAMS,
                                "can not find streams from url");
        }
        exit = true;
        pthread_mutex_unlock(&init_mutex);
        return -1;
    }

    if (pFormatCtx == NULL) {
        exit = true;
        pthread_mutex_unlock(&init_mutex);
        return -1;
    }

    duration = pFormatCtx->duration / 1000000;
    if (LOG_SHOW) {
        LOGD("channel numbers is %d", pFormatCtx->nb_streams);
    }
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)//音频
        {
            if (LOG_SHOW) {
                LOGE("音频");
            }
            AudioChannel *wl = new AudioChannel(i, pFormatCtx->streams[i]->time_base);
            audiochannels.push_front(wl);
        } else if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)//视频
        {
            if (!isOnlyMusic) {
                if (LOG_SHOW) {
                    LOGE("视频");
                }
                int num = pFormatCtx->streams[i]->avg_frame_rate.num;
                int den = pFormatCtx->streams[i]->avg_frame_rate.den;
                if (num != 0 && den != 0) {
                    int fps = pFormatCtx->streams[i]->avg_frame_rate.num /
                              pFormatCtx->streams[i]->avg_frame_rate.den;
                    AudioChannel *wl = new AudioChannel(i, pFormatCtx->streams[i]->time_base,
                                                            fps);
                    videochannels.push_front(wl);
                }
            }
        }
    }


    if (audiochannels.size() > 0) {
        audioDecoder = new AudioDecoder(wlPlayStatus, javaJNICallback);
        setAudioChannel(0);
        if (audioDecoder->streamIndex >= 0 && audioDecoder->streamIndex < pFormatCtx->nb_streams) {
            if (getAvCodecContext(pFormatCtx->streams[audioDecoder->streamIndex]->codecpar,
                                  audioDecoder) !=
                0) {
                exit = true;
                pthread_mutex_unlock(&init_mutex);
                return 1;
            }
        }


    }
    if (videochannels.size() > 0) {
        videoDecoder = new VideoDecoder(javaJNICallback, audioDecoder, wlPlayStatus);
        setVideoChannel(0);
        if (videoDecoder->streamIndex >= 0 && videoDecoder->streamIndex < pFormatCtx->nb_streams) {
            if (getAvCodecContext(pFormatCtx->streams[videoDecoder->streamIndex]->codecpar,
                                  videoDecoder) !=
                0) {
                exit = true;
                pthread_mutex_unlock(&init_mutex);
                return 1;
            }
        }
    }

    if (audioDecoder == NULL && videoDecoder == NULL) {
        exit = true;
        pthread_mutex_unlock(&init_mutex);
        return 1;
    }
    if (audioDecoder != NULL) {
        audioDecoder->duration = pFormatCtx->duration / 1000000;
        audioDecoder->sample_rate = audioDecoder->avCodecContext->sample_rate;
        if (videoDecoder != NULL) {
            audioDecoder->setVideo(true);
        }
    }
    if (videoDecoder != NULL) {
        if (LOG_SHOW) {
            LOGE("codec name is %s", videoDecoder->avCodecContext->codec->name);
            LOGE("codec long name is %s", videoDecoder->avCodecContext->codec->long_name);
        }
        if (!javaJNICallback->isOnlySoft(WL_THREAD_CHILD)) {
            mimeType = getMimeType(videoDecoder->avCodecContext->codec->name);
        } else {
            mimeType = -1;
        }

        if (mimeType != -1) {
            javaJNICallback->onInitMediacodec(WL_THREAD_CHILD, mimeType,
                                         videoDecoder->avCodecContext->width,
                                         videoDecoder->avCodecContext->height,
                                         videoDecoder->avCodecContext->extradata_size,
                                         videoDecoder->avCodecContext->extradata_size,
                                         videoDecoder->avCodecContext->extradata,
                                         videoDecoder->avCodecContext->extradata);
        }
        videoDecoder->duration = pFormatCtx->duration / 1000000;
    }
    if (LOG_SHOW) {
        LOGE("准备ing");
    }
    javaJNICallback->onParpared(WL_THREAD_CHILD);
    if (LOG_SHOW) {
        LOGE("准备end");
    }
    exit = true;
    pthread_mutex_unlock(&init_mutex);
    return 0;
}

int VideoPlayerControl::getAvCodecContext(AVCodecParameters *parameters, ZXbasePlayer *basePlayer) {

    AVCodec *dec = avcodec_find_decoder(parameters->codec_id);
    if (!dec) {
        javaJNICallback->onError(WL_THREAD_CHILD, 3, "get avcodec fail");
        exit = true;
        return 1;
    }
    basePlayer->avCodecContext = avcodec_alloc_context3(dec);
    if (!basePlayer->avCodecContext) {
        javaJNICallback->onError(WL_THREAD_CHILD, 4, "alloc avcodecctx fail");
        exit = true;
        return 1;
    }
    if (avcodec_parameters_to_context(basePlayer->avCodecContext, parameters) != 0) {
        javaJNICallback->onError(WL_THREAD_CHILD, 5, "copy avcodecctx fail");
        exit = true;
        return 1;
    }
    if (avcodec_open2(basePlayer->avCodecContext, dec, 0) != 0) {
        javaJNICallback->onError(WL_THREAD_CHILD, 6, "open avcodecctx fail");
        exit = true;
        return 1;
    }
    return 0;
}


VideoPlayerControl::~VideoPlayerControl() {
    pthread_mutex_destroy(&init_mutex);
    if (LOG_SHOW) {
        LOGE("video_player_controlayer_control() 释放了");
    }
}


int VideoPlayerControl::getDuration() {
    return duration;
}

int VideoPlayerControl::start() {
    exit = false;
    int count = 0;
    int ret = -1;
    if (audioDecoder != NULL) {
        audioDecoder->playAudio();
    }
    if (videoDecoder != NULL) {
        if (mimeType == -1) {
            videoDecoder->playVideo(0);
        } else {
            videoDecoder->playVideo(1);
        }
    }

    AVBitStreamFilterContext *mimType = NULL;
    if (mimeType == 1) {
        mimType = av_bitstream_filter_init("h264_mp4toannexb");
    } else if (mimeType == 2) {
        mimType = av_bitstream_filter_init("hevc_mp4toannexb");
    } else if (mimeType == 3) {
        mimType = av_bitstream_filter_init("h264_mp4toannexb");
    } else if (mimeType == 4) {
        mimType = av_bitstream_filter_init("h264_mp4toannexb");
    }

    while (!wlPlayStatus->exit) {
        exit = false;
        if (wlPlayStatus->pause)//暂停
        {
            av_usleep(1000 * 100);
            continue;
        }
        if (audioDecoder != NULL && audioDecoder->queue->getAvPacketSize() > 100) {
//            LOGE("wlAudio 等待..........");
            av_usleep(1000 * 100);
            continue;
        }
        if (videoDecoder != NULL && videoDecoder->queue->getAvPacketSize() > 100) {
//            LOGE("wlVideo 等待..........");
            av_usleep(1000 * 100);
            continue;
        }
        AVPacket *packet = av_packet_alloc();
        pthread_mutex_lock(&seek_mutex);
        ret = av_read_frame(pFormatCtx, packet);
        pthread_mutex_unlock(&seek_mutex);
        if (wlPlayStatus->seek) {
            av_packet_free(&packet);
            av_free(packet);
            continue;
        }
        if (ret == 0) {
            if (audioDecoder != NULL && packet->stream_index == audioDecoder->streamIndex) {
                count++;
                if (LOG_SHOW) {
                    LOGE("解码第 %d 帧", count);
                }
                audioDecoder->queue->putAvpacket(packet);
            } else if (videoDecoder != NULL && packet->stream_index == videoDecoder->streamIndex) {
                if (mimType != NULL && !isavi) {
                    uint8_t *data;
                    av_bitstream_filter_filter(mimType,
                                               pFormatCtx->streams[videoDecoder->streamIndex]->codec,
                                               NULL, &data, &packet->size, packet->data,
                                               packet->size, 0);
                    uint8_t *tdata = NULL;
                    tdata = packet->data;
                    packet->data = data;
                    if (tdata != NULL) {
                        av_free(tdata);
                    }
                }
                videoDecoder->queue->putAvpacket(packet);
            } else {
                av_packet_free(&packet);
                av_free(packet);
                packet = NULL;
            }
        } else {
            av_packet_free(&packet);
            av_free(packet);
            packet = NULL;
            if ((videoDecoder != NULL && videoDecoder->queue->getAvFrameSize() == 0) ||
                (audioDecoder != NULL && audioDecoder->queue->getAvPacketSize() == 0)) {
                wlPlayStatus->exit = true;
                break;
            }
        }
    }
    if (mimType != NULL) {
        av_bitstream_filter_close(mimType);
    }
    if (!exitByUser && javaJNICallback != NULL) {
        javaJNICallback->onComplete(WL_THREAD_CHILD);
    }
    exit = true;
    return 0;
}

void VideoPlayerControl::release() {
    wlPlayStatus->exit = true;
    pthread_mutex_lock(&init_mutex);
    if (LOG_SHOW) {
        LOGE("开始释放 wlffmpeg");
    }
    int sleepCount = 0;
    while (!exit) {
        if (sleepCount > 1000)//十秒钟还没有退出就自动强制退出
        {
            exit = true;
        }
        if (LOG_SHOW) {
            LOGE("wait ffmpeg  exit %d", sleepCount);
        }

        sleepCount++;
        av_usleep(1000 * 10);//暂停10毫秒
    }
    if (LOG_SHOW) {
        LOGE("释放audio....................................");
    }

    if (audioDecoder != NULL) {
        if (LOG_SHOW) {
            LOGE("释放audio....................................2");
        }

        audioDecoder->realease();
        delete (audioDecoder);
        audioDecoder = NULL;
    }
    if (LOG_SHOW) {
        LOGE("释放video....................................");
    }

    if (videoDecoder != NULL) {
        if (LOG_SHOW) {
            LOGE("释放video....................................2");
        }

        videoDecoder->release();
        delete (videoDecoder);
        videoDecoder = NULL;
    }
    if (LOG_SHOW) {
        LOGE("释放format...................................");
    }

    if (pFormatCtx != NULL) {
        avformat_close_input(&pFormatCtx);
        avformat_free_context(pFormatCtx);
        pFormatCtx = NULL;
    }
    if (LOG_SHOW) {
        LOGE("释放javacall.................................");
    }

    if (javaJNICallback != NULL) {
        javaJNICallback = NULL;
    }
    pthread_mutex_unlock(&init_mutex);
}

void VideoPlayerControl::pause() {
    if (wlPlayStatus != NULL) {
        wlPlayStatus->pause = true;
        if (audioDecoder != NULL) {
            audioDecoder->pause();
        }
    }
}

void VideoPlayerControl::resume() {
    if (wlPlayStatus != NULL) {
        wlPlayStatus->pause = false;
        if (audioDecoder != NULL) {
            audioDecoder->resume();
        }
    }
}

int VideoPlayerControl::getMimeType(const char *codecName) {

    if (strcmp(codecName, "h264") == 0) {
        return 1;
    }
    if (strcmp(codecName, "hevc") == 0) {
        return 2;
    }
    if (strcmp(codecName, "mpeg4") == 0) {
        isavi = true;
        return 3;
    }
    if (strcmp(codecName, "wmv3") == 0) {
        isavi = true;
        return 4;
    }

    return -1;
}

int VideoPlayerControl::seek(int64_t sec) {
    if (sec >= duration) {
        return -1;
    }
    if (wlPlayStatus->load) {
        return -1;
    }
    if (pFormatCtx != NULL) {
        wlPlayStatus->seek = true;
        pthread_mutex_lock(&seek_mutex);
        int64_t rel = sec * AV_TIME_BASE;
        int ret = avformat_seek_file(pFormatCtx, -1, INT64_MIN, rel, INT64_MAX, 0);
        if (audioDecoder != NULL) {
            audioDecoder->queue->clearAvpacket();
//            av_seek_frame(pFormatCtx, wlAudio->streamIndex, sec * wlAudio->time_base.den, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
            audioDecoder->setClock(0);
        }
        if (videoDecoder != NULL) {
            videoDecoder->queue->clearAvFrame();
            videoDecoder->queue->clearAvpacket();
//            av_seek_frame(pFormatCtx, wlVideo->streamIndex, sec * wlVideo->time_base.den, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
            videoDecoder->setClock(0);
        }
        audioDecoder->clock = 0;
        audioDecoder->now_time = 0;
        pthread_mutex_unlock(&seek_mutex);
        wlPlayStatus->seek = false;
    }
    return 0;
}

void VideoPlayerControl::setAudioChannel(int index) {
    if (audioDecoder != NULL) {
        int channelsize = audiochannels.size();
        if (index < channelsize) {
            for (int i = 0; i < channelsize; i++) {
                if (i == index) {
                    audioDecoder->time_base = audiochannels.at(i)->time_base;
                    audioDecoder->streamIndex = audiochannels.at(i)->channelId;
                }
            }
        }
    }

}

void VideoPlayerControl::setVideoChannel(int id) {
    if (videoDecoder != NULL) {
        videoDecoder->streamIndex = videochannels.at(id)->channelId;
        videoDecoder->time_base = videochannels.at(id)->time_base;
        videoDecoder->rate = 1000 / videochannels.at(id)->fps;
        if (videochannels.at(id)->fps >= 60) {
            videoDecoder->frameratebig = true;
        } else {
            videoDecoder->frameratebig = false;
        }

    }
}

int VideoPlayerControl::getAudioChannels() {
    return audiochannels.size();
}

int VideoPlayerControl::getVideoWidth() {
    if (videoDecoder != NULL && videoDecoder->avCodecContext != NULL) {
        return videoDecoder->avCodecContext->width;
    }
    return 0;
}

int VideoPlayerControl::getVideoHeight() {
    if (videoDecoder != NULL && videoDecoder->avCodecContext != NULL) {
        return videoDecoder->avCodecContext->height;
    }
    return 0;
}
