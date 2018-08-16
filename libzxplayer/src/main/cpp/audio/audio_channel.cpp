//
// Created by ywl on 2017-12-29.
//

#include "audio_channel.h"

AudioChannel::AudioChannel(int id, AVRational base) {
    channelId = id;
    time_base = base;
}

AudioChannel::AudioChannel(int id, AVRational base, int f) {
    channelId = id;
    time_base = base;
    fps = f;
}
