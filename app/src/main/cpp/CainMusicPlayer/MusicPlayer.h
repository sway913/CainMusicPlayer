//
// Created by admin on 2018/3/28.
//

#ifndef MUSICPLAYER_MUSICPLAYER_H
#define MUSICPLAYER_MUSICPLAYER_H

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
    // 播放
    void play();
    // 重置所有状态
    void reset(void);
    // 回收资源
    void release(void);
    // 指定播放位置
    int seekTo(long msec);
    // 设置是否单曲循环
    void setLooping(bool loop);
    // 设置播放的速度，默认1.0
    void setPlaybackRate(float playbackRate);
    // 设置音调，默认1.0
    void setPlaybackPitch(float pitch);
    // 设置节拍, 默认1.0
    void setTempo(double tempo);
    // 设置速度改变(-50 ~ 100%)
    void setRateChange(double newRate);
    // 设置节拍改变(-50 ~ 100%)
    void setTempoChange(double newTempo);
    // 在原音调基础上，以八度音为单位进行调整(-1.0, 1.0)
    void setPitchOctaves(double newPitch);
    // 在原音调基础上，以半音为单位进行调整(-12, 12);
    void setPitchSemiTones(double newPitch);

public:
    // 打开媒体流
    int openStream(int stream_index);
    // 打开文件
    int openMusicFile(const char *filename);
    // 关闭媒体流
    void closeStream(int stream_index);
    // 关闭媒体流
    void closeMusicFile();
    // 退出播放器
    void closePlayer();
    // 定位操作
    void streamSeek(AudioState *is, int64_t pos, int64_t rel, int seek_by_bytes);
    // 解复用
    int demux();
    // 解码音频
    int audioDecode();
    // 音频解码
    int decodeAudioFrame();
    // 音频回调
    static void audioCallback(void *opaque, uint8_t *stream, int len);
    // 音频回调
    void audio_callback(uint8_t *stream, int len);
    // 打开音频设备
    int openAudioDevice(int64_t wanted_channel_layout, int wanted_nb_channels,
                        int wanted_sample_rate, struct AudioParams *audio_hw_params);
    // 解码中断回调
    static int decode_interrupt_cb(void *ctx);
    // 判断媒体流中是否存在足够的裸数据包
    int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);

private:

    // 读文件线程
    static int demuxThreadHandle(void *arg);
    // 音频解码线程
    static int audioDecodeThreadHandle(void *arg);

    // 判断是否需要调整播放声音
    inline bool isNeedToAdjustAudio() {
        return (playbackRate != 1.0f || playbackPitch != 1.0f
                || playbackTempo != 1.0 || playbackRateChanged != 0 || playbackTempoChanged != 0
                || playbackPitchOctaves != 0 || playbackPitchSemiTones != 0)
               && !audioState->abort_request;
    }

    // 调整音频
    int adjustAudio(short *data, int len, int bytes_per_sample, uint channel, uint sampleRate);

private:
    AudioState *audioState;
    int seek_by_bytes = -1;                     // 以字节方式定位，用于ogg等格式
    int64_t start_time = AV_NOPTS_VALUE;        // 开始播放的时间
    int64_t duration = AV_NOPTS_VALUE;          // 时长

    char *fileName;                             // 文件名
    int playLoop = 1;                           // 循环播放

    // 音频变速变调处理
    float playbackRate;                         // 播放速度
    float playbackPitch;                        // 播放音调
    double playbackTempo;                       // 节拍
    double playbackRateChanged;                 // 速度改变(-50 ~ 100%)
    double playbackTempoChanged;                // 节拍改变(-50 ~ 100%)
    double playbackPitchOctaves;                // 八度音调整(-1.0 ~ 1.0)
    double playbackPitchSemiTones;              // 半音调整(-12，12)


    /* current context */
    int64_t audio_callback_time; // 音频回调时间

    // 音频输出对象
    AudioOutput *mAudioOutput = NULL;

    // SoundTouch包装类
    SoundTouchWrapper *mSoundTouchWrapper;

};


#endif //MUSICPLAYER_MUSICPLAYER_H
