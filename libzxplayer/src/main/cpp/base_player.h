//
// Created by ywl on 2017-12-3.
//

#ifndef WLPLAYER_WLBASEPLAYER_H
#define WLPLAYER_WLBASEPLAYER_H

extern "C"
{
#include <libavcodec/avcodec.h>
};

class ZXbasePlayer {

public:
    int streamIndex;
    int duration;
    double clock = 0;
    double now_time = 0;
    AVCodecContext *avCodecContext = NULL;
    AVRational time_base;

public:
    ZXbasePlayer();
    ~ZXbasePlayer();
};


#endif //WLPLAYER_WLBASEPLAYER_H
