package com.flyer.ffmpeg;

public final class FFmpegUtils {

    static {
        System.loadLibrary("flyffmpeg-lib");
    }

    public native static String getFFmpegVersion();

    /**
     *
     * @param videoPath 视频文件路径
     * @param YUVoutPath yuv文件输出路径
     * @return
     */
    public native static int decodeVideo2YUV(String videoPath,String YUVoutPath);


    /**
     *解封装多媒体文件，获取多媒体信息
     * @param mediaPath 多媒体路径
     * @return 0表示成功，其他代表失败
     */
    public native static int analyzeMedia(String mediaPath);
}
