//
// Created by Administrator on 2017/11/20.
//



#include "FFmpegVideo.h"

static void (*video_call)(AVFrame *frame);
void *videoPlay(void *args){
    FFmpegVideo *ffmpegVideo = (FFmpegVideo *) args;
    //申请AVFrame
    AVFrame *frame = av_frame_alloc();//分配一个AVFrame结构体,AVFrame结构体一般用于存储原始数据，指向解码后的原始帧
    AVFrame *rgb_frame = av_frame_alloc();//分配一个AVFrame结构体，指向存放转换成rgb后的帧
    AVPacket *packet = (AVPacket *) av_mallocz(sizeof(AVPacket));
    //输出文件
    //FILE *fp = fopen(outputPath,"wb");


    //缓存区
    uint8_t  *out_buffer= (uint8_t *)av_mallocz(avpicture_get_size(AV_PIX_FMT_RGBA,
                                                                  ffmpegVideo->codec->width,ffmpegVideo->codec->height));
    //与缓存区相关联，设置rgb_frame缓存区
    avpicture_fill((AVPicture *)rgb_frame,out_buffer,AV_PIX_FMT_RGBA,ffmpegVideo->codec->width,ffmpegVideo->codec->height);


    LOGE("转换成rgba格式")
    ffmpegVideo->swsContext = sws_getContext(ffmpegVideo->codec->width,ffmpegVideo->codec->height,ffmpegVideo->codec->pix_fmt,
                                            ffmpegVideo->codec->width,ffmpegVideo->codec->height,AV_PIX_FMT_RGBA,
                                            SWS_BICUBIC,NULL,NULL,NULL);



    LOGE("LC XXXXX  %f",ffmpegVideo->codec);

    double  last_play  //上一帧的播放时间
    ,play             //当前帧的播放时间
    ,last_delay    // 上一次播放视频的两帧视频间隔时间
    ,delay         //两帧视频间隔时间
    ,audio_clock //音频轨道 实际播放时间
    ,diff   //音频帧与视频帧相差时间
    ,sync_threshold
    ,start_time  //从第一帧开始的绝对时间
    ,pts
    ,actual_delay//真正需要延迟时间
    ;

    //从第一帧开始的绝对时间
    start_time = av_gettime() / 1000000.0;
    int frameCount;
    int h =0;
    LOGE("解码 ")
    while (ffmpegVideo->isPlay) {
        ffmpegVideo->get(packet);
        LOGE("解码 %d",packet->stream_index)
        avcodec_decode_video2(ffmpegVideo->codec, frame, &frameCount, packet);
        if(!frameCount){
            continue;
        }
        //转换为rgb格式
        sws_scale(ffmpegVideo->swsContext,(const uint8_t *const *)frame->data,frame->linesize,0,
                  frame->height,rgb_frame->data,
                  rgb_frame->linesize);
        LOGE("frame 宽%d,高%d",frame->width,frame->height);
        LOGE("rgb格式 宽%d,高%d",rgb_frame->width,rgb_frame->height);

        if((pts=av_frame_get_best_effort_timestamp(frame))==AV_NOPTS_VALUE){
            pts=0;
        }

        play = pts*av_q2d(ffmpegVideo->time_base);
        //纠正时间
        play = ffmpegVideo->synchronize(frame,play);

        delay = play - last_play;
        if (delay <= 0 || delay > 1) {
            delay = last_delay;
        }
        audio_clock = ffmpegVideo->ffmpegMusic->clock;
        last_delay = delay;
        last_play = play;
//音频与视频的时间差
        diff = ffmpegVideo->clock - audio_clock;
//        在合理范围外  才会延迟  加快
        sync_threshold = (delay > 0.01 ? 0.01 : delay);

        if (fabs(diff) < 10) {
            if (diff <= -sync_threshold) {
                delay = 0;
            } else if (diff >=sync_threshold) {
                delay = 2 * delay;
            }
        }
        start_time += delay;
        actual_delay=start_time-av_gettime()/1000000.0;
        if (actual_delay < 0.01) {
            actual_delay = 0.01;
        }
        av_usleep(actual_delay*1000000.0+6000);
        LOGE("播放视频")
        video_call(rgb_frame);
//        av_packet_unref(packet);
//        av_frame_unref(rgb_frame);
//        av_frame_unref(frame);
    }
    LOGE("free packet");
    av_free(packet);
    LOGE("free packet ok");
    LOGE("free packet");
    av_frame_free(&frame);
    av_frame_free(&rgb_frame);
    sws_freeContext(ffmpegVideo->swsContext);
    size_t size = ffmpegVideo->queue.size();
    for (int i = 0; i < size; ++i) {
        AVPacket *pkt = ffmpegVideo->queue.front();
        av_free(pkt);
        ffmpegVideo->queue.erase(ffmpegVideo->queue.begin());
    }
    LOGE("VIDEO EXIT");
    pthread_exit(0);

}



FFmpegVideo::FFmpegVideo() {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
}

FFmpegVideo::~FFmpegVideo() {
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
}

void FFmpegVideo::setAvCodecContext(AVCodecContext *avCodecContext) {
codec = avCodecContext;
}

int FFmpegVideo::put(AVPacket *avPacket) {
    LOGE("插入队列 video")
    AVPacket *avPacket1 = (AVPacket *) av_mallocz(sizeof(AVPacket));
    //克隆
    if(av_packet_ref(avPacket1,avPacket)){
        //克隆失败
        return 0;
    }
    //push的时候需要锁住，有数据的时候再解锁
    pthread_mutex_lock(&mutex);
    queue.push_back(avPacket1);//将packet压入队列
    //压入过后发出消息并且解锁
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    return 1;
}

int FFmpegVideo::get(AVPacket *avPacket) {
    LOGE("取出队列")
    pthread_mutex_lock(&mutex);
    while (isPlay){
        if(!queue.empty()&&isPause){
            //如果队列中有数据可以拿出来
            if(av_packet_ref(avPacket,queue.front())){
                break;
            }
            //取成功了，弹出队列，销毁packet
            AVPacket *packet2 = queue.front();
            queue.erase(queue.begin());
            av_free(packet2);
            break;
        } else{
           pthread_cond_wait(&cond,&mutex);
        }
    }
    LOGE("解锁")
    pthread_mutex_unlock(&mutex);
    return 0;
}

void FFmpegVideo::play() {
    LOGE("开启播放线程")
    LOGE("ISPAUSE AAAA %d",isPause);
    if(isPause==0){
        LOGE("sssssssssssssssssssssssssssssssssssssssssssss")
    }
        isPlay=1;
        isPause=1;
        pthread_create(&playId, NULL, videoPlay, this);//开启begin线程

}

void FFmpegVideo::stop() {
    LOGE("VIDEO stop");

    pthread_mutex_lock(&mutex);
    isPlay = 0;
    //因为可能卡在 deQueue
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    pthread_join(playId, 0);
    LOGE("VIDEO join pass");
    if (this->codec) {
        if (avcodec_is_open(this->codec))
            avcodec_close(this->codec);
        avcodec_free_context(&this->codec);
        this->codec = 0;
    }
    LOGE("VIDEO close");

}

double FFmpegVideo::synchronize(AVFrame *frame, double play) {
    //clock是当前播放的时间位置
    if (play != 0)
        clock=play;
    else //pst为0 则先把pts设为上一帧时间
        play = clock;
    //可能有pts为0 则主动增加clock
    //frame->repeat_pict = 当解码时，这张图片需要要延迟多少
    //需要求出扩展延时：
    //extra_delay = repeat_pict / (2*fps) 显示这样图片需要延迟这么久来显示
    double repeat_pict = frame->repeat_pict;
    //使用AvCodecContext的而不是stream的
    double frame_delay = av_q2d(codec->time_base);
    //如果time_base是1,25 把1s分成25份，则fps为25
    //fps = 1/(1/25)
    double fps = 1 / frame_delay;
    //pts 加上 这个延迟 是显示时间
    double extra_delay = repeat_pict / (2 * fps);
    double delay = extra_delay + frame_delay;
//    LOGI("extra_delay:%f",extra_delay);
    clock += delay;
    return play;
}

void FFmpegVideo::setFFmepegMusic(FFmpegMusic *ffmpegMusic) {
    this->ffmpegMusic = ffmpegMusic;

}

void FFmpegVideo::setPlayCall(void (*call)(AVFrame *)) {
    video_call=call;
}

void FFmpegVideo::pause() {
    LOGE("执行了pause")
   /* if(isPause==1){
        isPause=0;
    } else{
        isPause=1;
        pthread_mutex_unlock(&mutex);
    }*/
    if(isPause==1){
        isPause=0;
    } else{
        isPause=1;
      //  videoPlay(this);
        pthread_cond_signal(&cond);
    }
}
