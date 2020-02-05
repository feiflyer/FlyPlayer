#include <jni.h>
#include <string>

// 因为ffmpeg是纯C代码，要在cpp中使用则需要使用 extern "C"
extern "C" {
#include "libavutil/avutil.h"
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_flyer_ffmpeg_FFmpegUtils_getFFmpegVersion(JNIEnv *env, jclass clazz) {
    return env -> NewStringUTF(av_version_info());
}