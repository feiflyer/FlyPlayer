#include <jni.h>
#include <string>


#include "FlyLog.h"

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

// 因为ffmpeg是纯C代码，要在cpp中使用则需要使用 extern "C"
extern "C" {
#include "libavutil/avutil.h"

#include <libavformat/avformat.h>

#include <libswscale/swscale.h>

    //音频重采样的库

#include "libswresample/swresample.h"

}



//顶点着色器glsl的宏
// 第二个#号的意思是自动链接字符串，而不用增加引号，参考ijkplayer的写法

#define GET_STR(x) #x

static const char *vertexShader = GET_STR(

        attribute  vec4 aPosition; //顶点坐标，在外部获取传递进来

        attribute vec2 aTexCoord; //材质（纹理）顶点坐标

        varying vec2 vTexCoord;   //输出的材质（纹理）坐标，给片元着色器使用
        void main() {
            //纹理坐标转换，以左上角为原点的纹理坐标转换成以左下角为原点的纹理坐标，
            // 比如以左上角为原点的（0，0）对应以左下角为原点的纹理坐标的（0，1）
            vTexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
            gl_Position = aPosition;
        }
);

//片元着色器,软解码和部分x86硬解码解码得出来的格式是YUV420p

static const char *fragYUV420P = GET_STR(

        precision mediump float;    //精度

        varying vec2 vTexCoord;     //顶点着色器传递的坐标，相同名字opengl会自动关联

        uniform sampler2D yTexture; //输入的材质（不透明灰度，单像素）

        uniform sampler2D uTexture;

        uniform sampler2D vTexture;
        void main() {
            vec3 yuv;
            vec3 rgb;
            yuv.r = texture2D(yTexture, vTexCoord).r; // y分量
            // 因为UV的默认值是127，所以我们这里要减去0.5（OpenGLES的Shader中会把内存中0～255的整数数值换算为0.0～1.0的浮点数值）
            yuv.g = texture2D(uTexture, vTexCoord).r - 0.5; // u分量
            yuv.b = texture2D(vTexture, vTexCoord).r - 0.5; // v分量
            // yuv转换成rgb，两种方法，一种是RGB按照特定换算公式单独转换
            // 另外一种是使用矩阵转换
            rgb = mat3(1.0, 1.0, 1.0,
                       0.0, -0.39465, 2.03211,
                       1.13983, -0.58060, 0.0) * yuv;
            //输出像素颜色
            gl_FragColor = vec4(rgb, 1.0);
        }
);

GLint InitShader(const char *code, GLint type) {
    //创建shader
    GLint sh = glCreateShader(type);
    if (sh == 0) {
        LOGE("glCreateShader %d failed!", type);
        return 0;
    }
    //加载shader
    glShaderSource(sh,
                   1,    //shader数量
                   &code, //shader代码
                   0);   //代码长度
    //编译shader
    glCompileShader(sh);

    //获取编译情况
    GLint status;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        LOGE("glCompileShader failed!");
        return 0;
    }
    LOGE("glCompileShader success!");
    return sh;
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

extern "C"
JNIEXPORT void JNICALL
Java_com_flyer_ffmpeg_FlyPlayer_playVideoByOpenGL(JNIEnv *env, jobject thiz, jstring video_path,
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

    // 获取视频的宽高,也可以通过解码器获取
    AVStream *as = fmt_ctx->streams[v_idx];
    int width = as->codecpar->width;
    int height = as->codecpar->height;

    LOGE("width:%d", width);
    LOGE("height:%d", height);

    //数据包
    AVPacket *pkt;
    //数据帧
    AVFrame *frame;

    //初始化
    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    //1 获取原始窗口
    ANativeWindow *nwin = ANativeWindow_fromSurface(env, surface);

    ////////////////////
    ///EGL
    //1 EGL display创建和初始化
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed!");
        return;
    }
    if (EGL_TRUE != eglInitialize(display, 0, 0)) {
        LOGE("eglInitialize failed!");
        return;
    }
    //2 surface
    //2-1 surface窗口配置
    //输出配置
    EGLConfig config;
    EGLint configNum;
    EGLint configSpec[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE
    };
    if (EGL_TRUE != eglChooseConfig(display, configSpec, &config, 1, &configNum)) {
        LOGE("eglChooseConfig failed!");
        return;
    }
    //创建surface
    EGLSurface winsurface = eglCreateWindowSurface(display, config, nwin, 0);
    if (winsurface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed!");
        return;
    }

    //3 context 创建关联的上下文
    const EGLint ctxAttr[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttr);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed!");
        return;
    }
    if (EGL_TRUE != eglMakeCurrent(display, winsurface, winsurface, context)) {
        LOGE("eglMakeCurrent failed!");
        return;
    }

    LOGE("EGL Init Success!");

    //顶点和片元shader初始化
    //顶点shader初始化
    GLint vsh = InitShader(vertexShader, GL_VERTEX_SHADER);
    //片元yuv420 shader初始化
    GLint fsh = InitShader(fragYUV420P, GL_FRAGMENT_SHADER);


    /////////////////////////////////////////////////////////////
    //创建渲染程序
    GLint program = glCreateProgram();
    if (program == 0) {
        LOGE("glCreateProgram failed!");
        return;
    }
    //渲染程序中加入着色器代码
    glAttachShader(program, vsh);
    glAttachShader(program, fsh);

    //链接程序
    glLinkProgram(program);
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        LOGE("glLinkProgram failed!");
        return;
    }
    glUseProgram(program);
    LOGE("glLinkProgram success!");
    /////////////////////////////////////////////////////////////


    //加入三维顶点数据 两个三角形组成正方形
    static float vers[] = {
            1.0f, -1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f,
    };
    GLuint apos = (GLuint) glGetAttribLocation(program, "aPosition");
    glEnableVertexAttribArray(apos);
    //传递顶点
    glVertexAttribPointer(apos, 3, GL_FLOAT, GL_FALSE, 12, vers);

    //加入材质坐标数据
    static float txts[] = {
            1.0f, 0.0f, //右下
            0.0f, 0.0f,
            1.0f, 1.0f,
            0.0, 1.0
    };
    GLuint atex = (GLuint) glGetAttribLocation(program, "aTexCoord");
    glEnableVertexAttribArray(atex);
    glVertexAttribPointer(atex, 2, GL_FLOAT, GL_FALSE, 8, txts);

    //材质纹理初始化
    //设置纹理层
    glUniform1i(glGetUniformLocation(program, "yTexture"), 0); //对于纹理第1层
    glUniform1i(glGetUniformLocation(program, "uTexture"), 1); //对于纹理第2层
    glUniform1i(glGetUniformLocation(program, "vTexture"), 2); //对于纹理第3层

    //创建opengl纹理
    GLuint texts[3] = {0};
    //创建三个纹理
    glGenTextures(3, texts);

    //设置纹理属性
    glBindTexture(GL_TEXTURE_2D, texts[0]);
    //缩小的过滤器
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //设置纹理的格式和大小
    glTexImage2D(GL_TEXTURE_2D,
                 0,           //细节基本 0默认
                 GL_LUMINANCE,//gpu内部格式 亮度，灰度图
                 width, height, //拉升到全屏
                 0,             //边框
                 GL_LUMINANCE,//数据的像素格式 亮度，灰度图 要与上面一致
                 GL_UNSIGNED_BYTE, //像素的数据类型
                 NULL                    //纹理的数据
    );

    //设置纹理属性
    glBindTexture(GL_TEXTURE_2D, texts[1]);
    //缩小的过滤器
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //设置纹理的格式和大小
    glTexImage2D(GL_TEXTURE_2D,
                 0,           //细节基本 0默认
                 GL_LUMINANCE,//gpu内部格式 亮度，灰度图
                 width / 2, height / 2, //拉升到全屏
                 0,             //边框
                 GL_LUMINANCE,//数据的像素格式 亮度，灰度图 要与上面一致
                 GL_UNSIGNED_BYTE, //像素的数据类型
                 NULL                    //纹理的数据
    );

    //设置纹理属性
    glBindTexture(GL_TEXTURE_2D, texts[2]);
    //缩小的过滤器
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //设置纹理的格式和大小
    glTexImage2D(GL_TEXTURE_2D,
                 0,           //细节基本 0默认
                 GL_LUMINANCE,//gpu内部格式 亮度，灰度图
                 width / 2, height / 2, //拉升到全屏
                 0,             //边框
                 GL_LUMINANCE,//数据的像素格式 亮度，灰度图 要与上面一致
                 GL_UNSIGNED_BYTE, //像素的数据类型
                 NULL                    //纹理的数据
    );


    //////////////////////////////////////////////////////
    ////纹理的修改和显示
    unsigned char *buf[3] = {0};
    buf[0] = new unsigned char[width * height];
    buf[1] = new unsigned char[width * height / 4];
    buf[2] = new unsigned char[width * height / 4];


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

                    // 解码得到YUV数据

                    // 数据Y
                    buf[0] = frame->data[0];

                    memcpy(buf[0],frame->data[0],width*height);
                    // 数据U
                    memcpy(buf[1],frame->data[1],width*height/4);

                    // 数据V
                    memcpy(buf[2],frame->data[2],width*height/4);

                    //激活第1层纹理,绑定到创建的opengl纹理
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D,texts[0]);
                    //替换纹理内容
                    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_LUMINANCE,GL_UNSIGNED_BYTE,buf[0]);


                    //激活第2层纹理,绑定到创建的opengl纹理
                    glActiveTexture(GL_TEXTURE0+1);
                    glBindTexture(GL_TEXTURE_2D,texts[1]);
                    //替换纹理内容
                    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width/2,height/2,GL_LUMINANCE,GL_UNSIGNED_BYTE,buf[1]);


                    //激活第2层纹理,绑定到创建的opengl纹理
                    glActiveTexture(GL_TEXTURE0+2);
                    glBindTexture(GL_TEXTURE_2D,texts[2]);
                    //替换纹理内容
                    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width/2,height/2,GL_LUMINANCE,GL_UNSIGNED_BYTE,buf[2]);

                    //三维绘制
                    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
                    //窗口显示
                    eglSwapBuffers(display,winsurface);

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

extern "C"
JNIEXPORT jint JNICALL
Java_com_flyer_ffmpeg_FFmpegUtils_playAudio(JNIEnv *env, jclass clazz, jstring audio_path) {

    const char *path = env->GetStringUTFChars(audio_path, 0);

    AVFormatContext *fmt_ctx;
    // 初始化格式化上下文
    fmt_ctx = avformat_alloc_context();

    // 使用ffmpeg打开文件
    int re = avformat_open_input(&fmt_ctx, path, nullptr, nullptr);
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
    int audio_idx = av_find_best_stream(
            fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (audio_idx == -1) {
        LOGE("获取音频流索引失败");
        return -1;
    }
    //解码器参数
    AVCodecParameters *c_par;
    //解码器上下文
    AVCodecContext *cc_ctx;
    //声明一个解码器
    const AVCodec *codec;

    c_par = fmt_ctx->streams[audio_idx]->codecpar;

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

    //音频重采样

    int dataSize = av_samples_get_buffer_size(NULL, av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO) , cc_ctx->frame_size,AV_SAMPLE_FMT_S16, 0);

    uint8_t *resampleOutBuffer = (uint8_t *) malloc(dataSize);

    //音频重采样上下文初始化
    SwrContext *actx = swr_alloc();
    actx = swr_alloc_set_opts(actx,
                              AV_CH_LAYOUT_STEREO,
                              AV_SAMPLE_FMT_S16,44100,
                              cc_ctx->channels,
                              cc_ctx->sample_fmt,cc_ctx->sample_rate,
                              0,0 );
    re = swr_init(actx);
    if(re != 0)
    {
        LOGE("swr_init failed:%s",av_err2str(re));
        return re;
    }

    // JNI创建AudioTrack

    jclass jAudioTrackClass = env->FindClass("android/media/AudioTrack");
    jmethodID jAudioTrackCMid = env->GetMethodID(jAudioTrackClass,"<init>","(IIIIII)V"); //构造

    //  public static final int STREAM_MUSIC = 3;
    int streamType = 3;
    int sampleRateInHz = 44100;
    // public static final int CHANNEL_OUT_STEREO = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT);
    int channelConfig = (0x4 | 0x8);
    // public static final int ENCODING_PCM_16BIT = 2;
    int audioFormat = 2;
    // getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat)
    jmethodID jGetMinBufferSizeMid = env->GetStaticMethodID(jAudioTrackClass, "getMinBufferSize", "(III)I");
    int bufferSizeInBytes = env->CallStaticIntMethod(jAudioTrackClass, jGetMinBufferSizeMid, sampleRateInHz, channelConfig, audioFormat);
    // public static final int MODE_STREAM = 1;
    int mode = 1;

    //创建了AudioTrack
    jobject jAudioTrack = env->NewObject(jAudioTrackClass,jAudioTrackCMid, streamType, sampleRateInHz, channelConfig, audioFormat, bufferSizeInBytes, mode);

    //play方法
    jmethodID jPlayMid = env->GetMethodID(jAudioTrackClass,"play","()V");
    env->CallVoidMethod(jAudioTrack,jPlayMid);

    // write method
    jmethodID jAudioTrackWriteMid = env->GetMethodID(jAudioTrackClass, "write", "([BII)I");

    while (av_read_frame(fmt_ctx, pkt) >= 0) {//持续读帧
        // 只解码音频流
        if (pkt->stream_index == audio_idx) {

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

                    //音频重采样
                    int len = swr_convert(actx,&resampleOutBuffer,
                                          frame->nb_samples,
                                          (const uint8_t**)frame->data,
                                          frame->nb_samples);

                    jbyteArray jPcmDataArray = env->NewByteArray(dataSize);
                    // native 创建 c 数组
                    jbyte *jPcmData = env->GetByteArrayElements(jPcmDataArray, NULL);

                    //内存拷贝
                    memcpy(jPcmData, resampleOutBuffer, dataSize);

                    // 同步刷新到 jbyteArray ，并释放 C/C++ 数组
                    env->ReleaseByteArrayElements(jPcmDataArray, jPcmData, 0);


                    LOGE("解码成功%d  dataSize:%d ",len,dataSize);

                    // 写入播放数据
                    env->CallIntMethod(jAudioTrack, jAudioTrackWriteMid, jPcmDataArray, 0, dataSize);

                    // 解除 jPcmDataArray 的持有，让 javaGC 回收
                    env->DeleteLocalRef(jPcmDataArray);

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

    LOGE("音频播放完毕");

    env->ReleaseStringUTFChars(audio_path, path);

    return 0;
}