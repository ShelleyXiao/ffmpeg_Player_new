//
// Created by ywl on 2017-12-1.
//

#ifndef WLPLAYER_WLFFMPEG_H
#define WLPLAYER_WLFFMPEG_H

#include "common/AndroidLog.h"
#include "pthread.h"
#include "base_player.h"
#include "JavaJNICallback.h"
#include "audio_decoder.h"
#include "video_decoder.h"
#include "WlPlayStatus.h"
#include "audio_channel.h"

extern "C"
{
#include <libavformat/avformat.h>
}


class VideoPlayerControl {

public:
    const char *urlpath = NULL;
    JavaJNICallback *javaJNICallback = NULL;
    pthread_t decodThread;
    AVFormatContext *pFormatCtx = NULL;//封装格式上下文
    int duration = 0;
    AudioDecoder *audioDecoder = NULL;
    VideoDecoder *videoDecoder = NULL;
    WlPlayStatus *wlPlayStatus = NULL;
    bool exit = false;
    bool exitByUser = false;
    int mimeType = 1;
    bool isavi = false;
    bool isOnlyMusic = false;

    std::deque<AudioChannel*> audiochannels;
    std::deque<AudioChannel*> videochannels;

    pthread_mutex_t init_mutex;
    pthread_mutex_t seek_mutex;

public:
    VideoPlayerControl(JavaJNICallback *javaCall, const char *urlpath, bool onlymusic);
    ~VideoPlayerControl();
    int preparedFFmpeg();
    int decodeFFmpeg();
    int start();
    int seek(int64_t sec);
    int getDuration();
    int getAvCodecContext(AVCodecParameters * parameters, ZXbasePlayer *basePlayer);
    void release();
    void pause();
    void resume();
    int getMimeType(const char* codecName);
    void setAudioChannel(int id);
    void setVideoChannel(int id);
    int getAudioChannels();
    int getVideoWidth();
    int getVideoHeight();
};


#endif //WLPLAYER_WLFFMPEG_H
