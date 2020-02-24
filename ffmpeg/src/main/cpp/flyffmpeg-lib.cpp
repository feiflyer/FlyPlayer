#include <jni.h>
#include <string>


#include "FlyLog.h"

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

// 因为ffmpeg是纯C代码，要在cpp中使用则需要使用 extern "C"
extern "C" {
#include "libavutil/avutil.h"

#include <libavformat/avformat.h>

#include <libswscale/swscale.h>

}

/**
 * 将数据转换成double类型的一个方法
 * @param r
 * @return
 */
static double r2d(AVRational r) {
    return r.num == 0 || r.den == 0 ? 0 : (double) r.num / (double) r.den;
}


extern "C"
JNIEXPORT jstring JNICALL
Java_com_flyer_ffmpeg_FFmpegUtils_getFFmpegVersion(JNIEnv *env, jclass clazz) {
    //通过av_version_info()获取版本号
    return env->NewStringUTF(av_version_info());
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_flyer_ffmpeg_FFmpegUtils_decodeVideo2YUV(JNIEnv *env, jclass clazz, jstring video_path,
                                                  jstring yuvout_path) {

    const char *inPath = env->GetStringUTFChars(video_path, 0);
    const char *outPath = env->GetStringUTFChars(yuvout_path, 0);

    AVFormatContext *fmt_ctx;
    // 初始化格式化上下文
    fmt_ctx = avformat_alloc_context();

    // 使用ffmpeg打开文件
    int re = avformat_open_input(&fmt_ctx, inPath, nullptr, nullptr);
    if (re != 0) {
        LOGE("打开文件失败：%s", av_err2str(re));
        return re;
    }

    //探测流索引
    re = avformat_find_stream_info(fmt_ctx, nullptr);

    if (re < 0) {
        LOGE("索引探测失败：%s", av_err2str(re));
        return re;
    }

    //寻找视频流索引
    int v_idx = av_find_best_stream(
            fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (v_idx == -1) {
        LOGE("获取视频流索引失败");
        return -1;
    }
    //解码器参数
    AVCodecParameters *c_par;
    //解码器上下文
    AVCodecContext *cc_ctx;
    //声明一个解码器
    const AVCodec *codec;

    c_par = fmt_ctx->streams[v_idx]->codecpar;

    //通过id查找解码器
    codec = avcodec_find_decoder(c_par->codec_id);

    if (!codec) {

        LOGE("查找解码器失败");
        return -2;
    }

    //用参数c_par实例化编解码器上下文，，并打开编解码器
    cc_ctx = avcodec_alloc_context3(codec);

    // 关联解码器上下文
    re = avcodec_parameters_to_context(cc_ctx, c_par);

    if (re < 0) {
        LOGE("解码器上下文关联失败:%s", av_err2str(re));
        return re;
    }

    //打开解码器
    re = avcodec_open2(cc_ctx, codec, nullptr);

    if (re != 0) {
        LOGE("打开解码器失败:%s", av_err2str(re));
        return re;
    }

    //数据包
    AVPacket *pkt;
    //数据帧
    AVFrame *frame;

    //初始化
    pkt = av_packet_alloc();
    frame = av_frame_alloc();


    FILE *yuvFile = fopen(outPath, "wb+");//黑皇实体sh_768x432.yuv

    while (av_read_frame(fmt_ctx, pkt) >= 0) {//持续读帧
        // 只解码视频流
        if (pkt->stream_index == v_idx) {

            //发送数据包到解码器
            avcodec_send_packet(cc_ctx, pkt);

            //清理
            av_packet_unref(pkt);

            //这里为什么要使用一个for循环呢？
            // 因为avcodec_send_packet和avcodec_receive_frame并不是一对一的关系的
            //一个avcodec_send_packet可能会出发多个avcodec_receive_frame
            for (;;) {
                // 接受解码的数据
                re = avcodec_receive_frame(cc_ctx, frame);
                if (re != 0) {
                    break;
                } else {
                    // 数据y
                    fwrite(frame->data[0], 1, cc_ctx->width * cc_ctx->height, yuvFile);
                    // 数据U
                    fwrite(frame->data[1], 1, cc_ctx->width / 2 * cc_ctx->height / 2, yuvFile);
                    // 数据V
                    fwrite(frame->data[2], 1, cc_ctx->width / 2 * cc_ctx->height / 2, yuvFile);
                }
            }

        }
    }
    //关闭文件句柄
    fclose(yuvFile);

    //关闭环境
    avcodec_free_context(&cc_ctx);
    // 释放资源
    av_frame_free(&frame);
    av_packet_free(&pkt);

    avformat_free_context(fmt_ctx);

    LOGE("解码YUV成功");

    return 0;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_flyer_ffmpeg_FFmpegUtils_analyzeMedia(JNIEnv *env, jclass clazz, jstring media_path) {

    //初始化解封装
    av_register_all();
    //初始化网络
    avformat_network_init();

    //打开文件
    AVFormatContext *ic = NULL;
    const char *path = env->GetStringUTFChars(media_path, 0);
    int re = avformat_open_input(&ic, path, 0, 0);
    if (re != 0) {
        LOGE("avformat_open_input failed!:%s", av_err2str(re));
        return -1;
    }
    LOGE("avformat_open_input %s success!", path);
    //获取流信息
    re = avformat_find_stream_info(ic, 0);
    if (re != 0) {
        LOGE("avformat_find_stream_info failed!");
        return -3;
    }
    LOGE("duration = %lld nb_streams = %d", ic->duration, ic->nb_streams);

    int fps = 0;
    int videoStream = -1;
    int audioStream = -1;

    // 有两个方法获取视频或者视频流的索引，一种是遍历，一种是使用自动探测的api
    // 通过遍历的方法获取索引
    for (int i = 0; i < ic->nb_streams; i++) {
        AVStream *as = ic->streams[i];
        if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            LOGE("视频数据");
            videoStream = i;
            fps = r2d(as->avg_frame_rate);

            LOGE("videoStream=%d,fps = %d,width=%d height=%d codeid=%d pixformat=%d",
                 videoStream, fps,
                 as->codecpar->width,
                 as->codecpar->height,
                 as->codecpar->codec_id,
                 as->codecpar->format
            );
        } else if (as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            LOGE("音频数据");
            audioStream = i;
            LOGE("audioStream=%d,sample_rate=%d channels=%d sample_format=%d",
                 audioStream,
                 as->codecpar->sample_rate,
                 as->codecpar->channels,
                 as->codecpar->format
            );
        }
    }

    // 通过探测的方式获取索引
    //获取音频流信息
    audioStream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    LOGE("av_find_best_stream audioStream = %d", audioStream);

    env->ReleaseStringUTFChars(media_path, path);

    //关闭上下文
    avformat_close_input(&ic);

    LOGE("解封装成功", "");
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_flyer_ffmpeg_FlyPlayer_playVideo(JNIEnv *env, jobject thiz, jstring video_path,
                                          jobject surface) {
    const char *path = env->GetStringUTFChars(video_path, 0);

    AVFormatContext *fmt_ctx;
    // 初始化格式化上下文
    fmt_ctx = avformat_alloc_context();

    // 使用ffmpeg打开文件
    int re = avformat_open_input(&fmt_ctx, path, nullptr, nullptr);
    if (re != 0) {
        LOGE("打开文件失败：%s", av_err2str(re));
        return;
    }

    //探测流索引
    re = avformat_find_stream_info(fmt_ctx, nullptr);

    if (re < 0) {
        LOGE("索引探测失败：%s", av_err2str(re));
        return;
    }

    //寻找视频流索引
    int v_idx = av_find_best_stream(
            fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (v_idx == -1) {
        LOGE("获取视频流索引失败");
        return;
    }
    //解码器参数
    AVCodecParameters *c_par;
    //解码器上下文
    AVCodecContext *cc_ctx;
    //声明一个解码器
    const AVCodec *codec;

    c_par = fmt_ctx->streams[v_idx]->codecpar;

    //通过id查找解码器
    codec = avcodec_find_decoder(c_par->codec_id);

    if (!codec) {

        LOGE("查找解码器失败");
        return;
    }

    //用参数c_par实例化编解码器上下文，，并打开编解码器
    cc_ctx = avcodec_alloc_context3(codec);

    // 关联解码器上下文
    re = avcodec_parameters_to_context(cc_ctx, c_par);

    if (re < 0) {
        LOGE("解码器上下文关联失败:%s", av_err2str(re));
        return;
    }

    //打开解码器
    re = avcodec_open2(cc_ctx, codec, nullptr);

    if (re != 0) {
        LOGE("打开解码器失败:%s", av_err2str(re));
        return;
    }

    //数据包
    AVPacket *pkt;
    //数据帧
    AVFrame *frame;

    //初始化
    pkt = av_packet_alloc();
    frame = av_frame_alloc();


    //初始化像素格式转换的上下文
    SwsContext *vctx = NULL;
    int outWidth = 1920;
    int outHeight = 1080;
    char *rgb = new char[outWidth * outHeight * 4];
    char *pcm = new char[48000 * 4 * 2];

    //显示窗口初始化
    ANativeWindow *nwin = ANativeWindow_fromSurface(env, surface);
    ANativeWindow_setBuffersGeometry(nwin, outWidth, outHeight, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer wbuf;

    while (av_read_frame(fmt_ctx, pkt) >= 0) {//持续读帧
        // 只解码视频流
        if (pkt->stream_index == v_idx) {

            //发送数据包到解码器
            avcodec_send_packet(cc_ctx, pkt);

            //清理
            av_packet_unref(pkt);

            //这里为什么要使用一个for循环呢？
            // 因为avcodec_send_packet和avcodec_receive_frame并不是一对一的关系的
            //一个avcodec_send_packet可能会出发多个avcodec_receive_frame
            for (;;) {
                // 接受解码的数据
                re = avcodec_receive_frame(cc_ctx, frame);
                if (re != 0) {
                    break;
                } else {

                    // 将YUV数据转换成RGB数据显示

                    vctx = sws_getCachedContext(vctx,
                                                frame->width,
                                                frame->height,
                                                (AVPixelFormat) frame->format,
                                                outWidth,
                                                outHeight,
                                                AV_PIX_FMT_RGBA,
                                                SWS_FAST_BILINEAR,
                                                0, 0, 0
                    );
                    if (!vctx) {
                        LOGE("sws_getCachedContext failed!");
                    } else {
                        uint8_t *data[AV_NUM_DATA_POINTERS] = {0};
                        data[0] = (uint8_t *) rgb;
                        int lines[AV_NUM_DATA_POINTERS] = {0};
                        lines[0] = outWidth * 4;
                        int h = sws_scale(vctx,
                                          (const uint8_t **) frame->data,
                                          frame->linesize, 0,
                                          frame->height,
                                          data, lines);
                        LOGE("sws_scale = %d", h);
                        if (h > 0) {
                            // 绘制
                            ANativeWindow_lock(nwin, &wbuf, 0);
                            uint8_t *dst = (uint8_t *) wbuf.bits;
                            memcpy(dst, rgb, outWidth * outHeight * 4);
                            ANativeWindow_unlockAndPost(nwin);
                        }
                    }

                }
            }

        }
    }
    //关闭环境
    avcodec_free_context(&cc_ctx);
    // 释放资源
    av_frame_free(&frame);
    av_packet_free(&pkt);

    avformat_free_context(fmt_ctx);

    LOGE("播放完毕");

    env->ReleaseStringUTFChars(video_path, path);
}