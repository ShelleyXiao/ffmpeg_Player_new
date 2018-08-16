//
// Created by ywl on 2017-11-12.
//
#pragma once
#ifndef WLPLAYER_ANDROIDLOG_H
#define WLPLAYER_ANDROIDLOG_H

#include <android/log.h>

#define LOG_SHOW false

#define LOGD(FORMAT, ...) __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,FORMAT,##__VA_ARGS__);
#define LOGI(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,FORMAT,##__VA_ARGS__);

#define DEBUG 1

#define LOG_DEBUG(MSG) do {    \
            if(DEBUG)    \
            LOGD(MSG) ;               \
    }while(0);\

#endif //WLPLAYER_ANDROIDLOG_H
