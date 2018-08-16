//
// Created by ywl on 2017-12-29.
//

#ifndef WLPLAYER_WLAUDIOCHANNEL_H
#define WLPLAYER_WLAUDIOCHANNEL_H


extern "C"
{
#include <libavutil/rational.h>
};



class AudioChannel {
public:
    int channelId = -1;
    AVRational time_base;
    int fps;

public:
    AudioChannel(int id, AVRational base);

    AudioChannel(int id, AVRational base, int fps);
};


#endif //WLPLAYER_WLAUDIOCHANNEL_H
