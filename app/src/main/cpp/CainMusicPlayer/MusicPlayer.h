//
// Created by admin on 2018/3/28.
//

#ifndef CAINMUSICPLAYER_MUSICPLAYER_H
#define CAINMUSICPLAYER_MUSICPLAYER_H

#include <SoundTouchWrapper.h>
#include "ffplay_def.h"
#include "AudioOutput.h"

class MusicPlayer {
public:
    MusicPlayer();

    virtual ~MusicPlayer();

    // 设置数据源
    void setDataSource(const char *path);
    // 获取当前时长
    long getCurrentPosition(void);
    // 获取时长
    long getDuration(void);
    // 是否循环播放
    bool isLooping(void);
    // 是否正在播放
    bool isPlaying(void);
    // 是否处于停止状态
    bool isStopped(void);
    // 暂停
    void pause(void);
    // 开始
    void start(void);
    // 停止
    void stop(void);
    // 异步装载流媒体
    int prepare(void);
    // 重置所有状态
    void reset(void);
    // 回收资源
    void release(void);
    // 指定播放位置
    int seekTo(long msec);
    // 设置是否单曲循环
    void setLooping(bool loop);
    // 设置播放的速度
    void setPlaybackRate(float playbackRate);
    // 设置音调
    void setPlaybackPitch(float pitch);
    // 设置是否精确查找
    void setAccurateSeek(bool accurateSeek);
    // 设置是否自动退出
    void setAutoExit(bool autoExit);
    // 设置无限缓冲区
    void setInfiniteBuffer(bool infinite_buffer);

public:
    // 打开媒体流
    int stream_component_open(AudioState *is, int stream_index);
    // 打开文件
    int stream_open(const char *filename);
    // 关闭媒体流
    void stream_component_close(AudioState *is, int stream_index);
    // 关闭媒体流
    void stream_close(AudioState *is);
    // 退出播放器
    void exitPlayer();
    // 退出播放器
    void do_exit(AudioState *is);

    // 同步类型
    int get_master_sync_type(AudioState *is);
    // 获取主时钟
    double get_master_clock(AudioState *is);

    // 定位操作
    void stream_seek(AudioState *is, int64_t pos, int64_t rel, int seek_by_bytes);

    // 读文件线程
    static int read_thread(void *arg);
    // 音频解码线程
    static int audio_thread(void *arg);

    // 解复用
    int demux();
    // 解码音频
    int audioDecode();

    // 同步音频
    int synchronize_audio(AudioState *is, int nb_samples);
    // 音频解码
    int audio_decode_frame(AudioState *is);
    // 音频回调
    static void sdl_audio_callback(void *opaque, uint8_t *stream, int len);
    // 音频回调
    void audio_callback(uint8_t *stream, int len);
    // 打开音频设备
    int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);

    // 解码中断回调
    static int decode_interrupt_cb(void *ctx);
    // 判断媒体流中是否存在足够的裸数据包
    int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);
    // 是否实时流
    static int is_realtime(AVFormatContext *s);

    // 播放
    void play();

private:

    AudioState *audioState;
    int seek_by_bytes = -1;                     // 以字节方式定位，用于ogg等格式
    int startup_volume = 100;                   // 初始音频
    int av_sync_type = AV_SYNC_AUDIO_MASTER;    // 音视频同步方式，默认是同步到音频
    int64_t start_time = AV_NOPTS_VALUE;        // 开始播放的时间
    int64_t duration = AV_NOPTS_VALUE;          // 时长
    int fast = 0;
    int genpts = 0;
    int lowres = 0;

    char *fileName;                             // 文件名
    float playbackRate = 1.0;                   // 播放速度
    float playbackPitch = 1.0;                  // 播放音调
    int accurate_seek = 1;                      // 精确查找
    int autoexit;                               // 播放结束自动退出
    int loop = 1;                               // 循环播放
    int infinite_buffer = -1;                   // 无限缓冲区，用于流媒体缓存

    /* current context */
    int64_t audio_callback_time; // 音频回调时间

    // 音频输出对象
    AudioOutput *mAudioOutput = NULL;

    // SoundTouch包装类
    SoundTouchWrapper *mSoundTouchWrapper;

};


#endif //CAINMUSICPLAYER_MUSICPLAYER_H
