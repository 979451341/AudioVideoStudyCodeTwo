#include <jni.h>
#include <string>
#include "FFmpegMusic.h"
#include "FFmpegVideo.h"
#include <android/native_window_jni.h>


extern "C" {
//编码
#include "libavcodec/avcodec.h"
//封装格式处理
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
//像素处理
#include "libswscale/swscale.h"

#include <unistd.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "Log.h"
}
const char *inputPath;
int64_t *totalTime;
FFmpegVideo *ffmpegVideo;
FFmpegMusic *ffmpegMusic;
pthread_t p_tid;
int isPlay;
ANativeWindow *window = 0;
int64_t duration;
AVFormatContext *pFormatCtx;
AVPacket *packet;
int step = 0;
jboolean isSeek = false;


void call_video_play(AVFrame *frame) {
    if (!window) {
        return;
    }
    ANativeWindow_Buffer window_buffer;
    if (ANativeWindow_lock(window, &window_buffer, 0)) {
        return;
    }

    LOGE("绘制 宽%d,高%d", frame->width, frame->height);
    LOGE("绘制 宽%d,高%d  行字节 %d ", window_buffer.width, window_buffer.height, frame->linesize[0]);
    uint8_t *dst = (uint8_t *) window_buffer.bits;
    int dstStride = window_buffer.stride * 4;
    uint8_t *src = frame->data[0];
    int srcStride = frame->linesize[0];
    for (int i = 0; i < window_buffer.height; ++i) {
        memcpy(dst + i * dstStride, src + i * srcStride, srcStride);
    }
    ANativeWindow_unlockAndPost(window);
}

void init() {
    LOGE("开启解码线程")
    //1.注册组件
    av_register_all();
    avformat_network_init();
    //封装格式上下文
    pFormatCtx = avformat_alloc_context();

    //2.打开输入视频文件
    if (avformat_open_input(&pFormatCtx, inputPath, NULL, NULL) != 0) {
        LOGE("%s", "打开输入视频文件失败");
    }
    //3.获取视频信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGE("%s", "获取视频信息失败");
    }

    //得到播放总时间
    if (pFormatCtx->duration != AV_NOPTS_VALUE) {
        duration = pFormatCtx->duration;//微秒
    }
}
/*void swap(int *a,int *b)
{
    int t=*a;*a=*b;*b=t;
}*/

void seekTo(int mesc) {
    if (mesc <= 0) {
        mesc=0;
    }
    //清空vector
    ffmpegMusic->queue.clear();
    ffmpegVideo->queue.clear();
    //跳帧
   /* if (av_seek_frame(pFormatCtx, -1,  mesc * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD) < 0) {
        LOGE("failed")
    } else {
        LOGE("success")
    }*/

    av_seek_frame(pFormatCtx, ffmpegVideo->index, (int64_t) (mesc /av_q2d(ffmpegVideo->time_base)), AVSEEK_FLAG_BACKWARD);
    av_seek_frame(pFormatCtx, ffmpegMusic->index, (int64_t) (mesc /av_q2d(ffmpegMusic->time_base)), AVSEEK_FLAG_BACKWARD);

}

void *begin(void *args) {

    //找到视频流和音频流
    for (int i = 0; i < pFormatCtx->nb_streams; ++i) {
        //获取解码器
        AVCodecContext *avCodecContext = pFormatCtx->streams[i]->codec;
        AVCodec *avCodec = avcodec_find_decoder(avCodecContext->codec_id);

        //copy一个解码器，
        AVCodecContext *codecContext = avcodec_alloc_context3(avCodec);
        avcodec_copy_context(codecContext, avCodecContext);
        if (avcodec_open2(codecContext, avCodec, NULL) < 0) {
            LOGE("打开失败")
            continue;
        }
        //如果是视频流
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            ffmpegVideo->index = i;
            ffmpegVideo->setAvCodecContext(codecContext);
            ffmpegVideo->time_base = pFormatCtx->streams[i]->time_base;
            if (window) {
                ANativeWindow_setBuffersGeometry(window, ffmpegVideo->codec->width,
                                                 ffmpegVideo->codec->height,
                                                 WINDOW_FORMAT_RGBA_8888);
            }
        }//如果是音频流
        else if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            ffmpegMusic->index = i;
            ffmpegMusic->setAvCodecContext(codecContext);
            ffmpegMusic->time_base = pFormatCtx->streams[i]->time_base;
        }
    }
//开启播放
    ffmpegVideo->setFFmepegMusic(ffmpegMusic);
    ffmpegMusic->play();
    ffmpegVideo->play();
    isPlay = 1;
    //seekTo(0);
    //解码packet,并压入队列中
    packet = (AVPacket *) av_mallocz(sizeof(AVPacket));
    //跳转到某一个特定的帧上面播放
    int ret;
    while (isPlay) {
        //
        ret = av_read_frame(pFormatCtx, packet);
        if (ret == 0) {
            if (ffmpegVideo && ffmpegVideo->isPlay && packet->stream_index == ffmpegVideo->index
               ) {
                //将视频packet压入队列
                ffmpegVideo->put(packet);
            } else if (ffmpegMusic && ffmpegMusic->isPlay &&
                       packet->stream_index == ffmpegMusic->index) {
                ffmpegMusic->put(packet);
            }
            av_packet_unref(packet);
        } else if (ret == AVERROR_EOF) {
            // 读完了
            //读取完毕 但是不一定播放完毕
            while (isPlay) {
                if (ffmpegVideo->queue.empty() && ffmpegMusic->queue.empty()) {
                    break;
                }
                // LOGE("等待播放完成");
                av_usleep(10000);
            }
        }
    }
    //解码完过后可能还没有播放完
    isPlay = 0;
    if (ffmpegMusic && ffmpegMusic->isPlay) {
        ffmpegMusic->stop();
    }
    if (ffmpegVideo && ffmpegVideo->isPlay) {
        ffmpegVideo->stop();
    }
    //释放
    av_free_packet(packet);
    avformat_free_context(pFormatCtx);
    pthread_exit(0);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_test_ffmpegvideoplay_Play_play(JNIEnv *env, jobject instance, jstring inputPath_) {
    inputPath = env->GetStringUTFChars(inputPath_, 0);
    init();
    ffmpegVideo = new FFmpegVideo;
    ffmpegMusic = new FFmpegMusic;
    ffmpegVideo->setPlayCall(call_video_play);
    pthread_create(&p_tid, NULL, begin, NULL);//开启begin线程
    env->ReleaseStringUTFChars(inputPath_, inputPath);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_test_ffmpegvideoplay_Play_display(JNIEnv *env, jobject instance, jobject surface) {
    //得到界面
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
    window = ANativeWindow_fromSurface(env, surface);
    if (ffmpegVideo && ffmpegVideo->codec) {
        ANativeWindow_setBuffersGeometry(window, ffmpegVideo->codec->width,
                                         ffmpegVideo->codec->height,
                                         WINDOW_FORMAT_RGBA_8888);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_test_ffmpegvideoplay_Play_release(JNIEnv *env, jobject instance) {
    //释放资源
    if (isPlay) {
        isPlay = 0;
        pthread_join(p_tid, 0);
    }
    if (ffmpegVideo) {
        if (ffmpegVideo->isPlay) {
            ffmpegVideo->stop();
        }
        delete (ffmpegVideo);
        ffmpegVideo = 0;
    }
    if (ffmpegMusic) {
        if (ffmpegMusic->isPlay) {
            ffmpegMusic->stop();
        }
        delete (ffmpegMusic);
        ffmpegMusic = 0;
    }
}extern "C"
JNIEXPORT void JNICALL
Java_com_test_ffmpegvideoplay_Play_stop(JNIEnv *env, jobject instance) {
    //点击暂停按钮
    ffmpegMusic->pause();
    ffmpegVideo->pause();

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_test_ffmpegvideoplay_Play_getTotalTime(JNIEnv *env, jobject instance) {

//获取视频总时间
    return (jint) duration;
}
extern "C"
JNIEXPORT double JNICALL
Java_com_test_ffmpegvideoplay_Play_getCurrentPosition(JNIEnv *env, jobject instance) {
    //获取音频播放时间
    return ffmpegMusic->clock;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_test_ffmpegvideoplay_Play_seekTo(JNIEnv *env, jobject instance, jint msec) {
seekTo(msec/1000);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_test_ffmpegvideoplay_Play_stepBack(JNIEnv *env, jobject instance) {

}
extern "C"
JNIEXPORT void JNICALL
Java_com_test_ffmpegvideoplay_Play_stepUp(JNIEnv *env, jobject instance) {
    //点击快进按钮
    seekTo(5);

}