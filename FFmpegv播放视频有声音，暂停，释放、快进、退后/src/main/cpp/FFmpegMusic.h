//
// Created by Administrator on 2017/11/20.
//

#ifndef FFMPEGVIDEOPLAY_FFMPEGMUSIC_H
#define FFMPEGVIDEOPLAY_FFMPEGMUSIC_H
#include <queue>
#include<vector>
#include <SLES/OpenSLES_Android.h>
extern "C"{

#include <libavcodec/avcodec.h>
#include <pthread.h>
#include <libswresample/swresample.h>
#include "Log.h"
#include <libavformat/avformat.h>

class FFmpegMusic {

public:
    FFmpegMusic();
    ~FFmpegMusic();
    void setAvCodecContext(AVCodecContext *avCodecContext);//解码器上下文

    int put(AVPacket *avPacket);//压进队列
    int get(AVPacket *avPacket);//弹出队列

    void play();//播放
    void stop();//暂停
    void pause();//pause

    int CreatePlayer();//创建opensl es播放器

    //成员变量
public:
    int index;//流索引
    int isPlay=-1;//是否正在播放
    int isPause=1;
    pthread_t playId;//处理线程
    std::vector<AVPacket*> queue;//队列
   // std::queue<AVPacket*> queueNull;//空队列
    AVCodecContext *codec;//解码器上下文

    SwrContext *swrContext;
    uint8_t *out_buffer;
    int out_channer_nb;

    //同步锁
    pthread_mutex_t mutex;
    //条件变量
    pthread_cond_t cond;

    double clock;//从第一zhen开始所需要时间

    AVRational time_base;

    SLObjectItf engineObject;
    SLEngineItf engineEngine;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb;
    SLObjectItf outputMixObject;
    SLObjectItf bqPlayerObject;
    SLEffectSendItf bqPlayerEffectSend;
    SLVolumeItf bqPlayerVolume;
    SLPlayItf bqPlayerPlay;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
};

};
#endif //FFMPEGMUSIC_FFMPEGAUDIO_H