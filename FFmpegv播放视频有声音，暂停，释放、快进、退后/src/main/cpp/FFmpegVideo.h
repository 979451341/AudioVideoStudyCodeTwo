//
// Created by Administrator on 2017/11/20.
//

#ifndef FFMPEGVIDEOPLAY_FFMPEGVIDEO_H
#define FFMPEGVIDEOPLAY_FFMPEGVIDEO_H
#include <queue>
#include<vector>
#include <SLES/OpenSLES_Android.h>
#include "FFmpegMusic.h"

extern "C"{
#include <unistd.h>
#include <libavcodec/avcodec.h>
#include <pthread.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include "Log.h"
#include <libavutil/imgutils.h>
#include <libavutil/time.h>

class FFmpegVideo {
public:
    FFmpegVideo();
    ~FFmpegVideo();
    void setAvCodecContext(AVCodecContext *avCodecContext);//解码器上下文

    int put(AVPacket *avPacket);//压进队列
    int get(AVPacket *avPacket);//弹出队列

    void play();//播放
    void stop();//暂停
    void pause();//pause

    double synchronize(AVFrame *frame, double play);

    void setFFmepegMusic(FFmpegMusic *ffmpegMusic);

    void setPlayCall(void (*call)(AVFrame* frame));


public:
    int index;//流索引
    int isPlay=-1;//是否正在播放
    int isPause=-1;//是否暂停
    pthread_t playId;//处理线程
    std::vector<AVPacket*> queue;//队列

    AVCodecContext *codec;//解码器上下文

    SwsContext* swsContext;
    //同步锁
    pthread_mutex_t mutex;
    //条件变量
    pthread_cond_t cond;

    FFmpegMusic *ffmpegMusic;

    AVRational time_base;
    double  clock;

};

};

#endif //FFMPEGVIDEOPLAY_FFMPEGVIDEO_H
