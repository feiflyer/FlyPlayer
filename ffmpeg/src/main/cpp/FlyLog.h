//
// Created by 梁传飞 on 2020-02-07.
//

#ifndef FLYPLAYER_FLYLOG_H
#define FLYPLAYER_FLYLOG_H

#include <android/log.h>

//定义一个宏，使用log库输出日志

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"FFMPEG",__VA_ARGS__)

#endif //FLYPLAYER_FLYLOG_H
