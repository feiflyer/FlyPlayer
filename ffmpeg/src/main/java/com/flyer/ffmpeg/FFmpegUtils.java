package com.flyer.ffmpeg;

public final class FFmpegUtils {

    static {
        System.loadLibrary("flyffmpeg-lib");
    }

    public native static String getFFmpegVersion();
}
