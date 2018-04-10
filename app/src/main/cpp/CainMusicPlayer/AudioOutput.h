//
// Created by admin on 2018/4/9.
//

#ifndef CAINPLAYER_AUDIOOUTPUT_H
#define CAINPLAYER_AUDIOOUTPUT_H

#include "ffplay_def.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

typedef struct AudioOpaque {
    Cond   *wakeup_cond;
    Mutex  *wakeup_mutex;

    Thread *audio_tid;

    AudioSpec        spec;
    SLDataFormat_PCM format_pcm;
    int              bytes_per_frame;
    int              milli_per_buffer;
    int              frames_per_buffer;
    int              bytes_per_buffer;

    SLObjectItf                     slObject;
    SLEngineItf                     slEngine;

    SLObjectItf                     slOutputMixObject;

    SLObjectItf                     slPlayerObject;
    SLAndroidSimpleBufferQueueItf   slBufferQueueItf;
    SLVolumeItf                     slVolumeItf;
    SLPlayItf                       slPlayItf;

    volatile bool  need_set_volume;
    volatile float left_volume;
    volatile float right_volume;

    volatile bool  abort_request;
    volatile bool  pause_on;
    volatile bool  need_flush;
    volatile bool  is_running;

    uint8_t       *buffer;
    size_t         buffer_capacity;
} AudioOpaque;

// 音频输出
class AudioOutput {
public:
    AudioOutput();
    virtual ~AudioOutput();
    // 初始化
    int initAudioOutput();
    // 打开音频设备
    int openAudio(const AudioSpec * desired, AudioSpec * obtained);
    // 暂停设备
    void pauseAudio(int pause_on);
    // 清空音频缓冲
    void flushAudio(void);
    // 设置音量大小
    void setStereoVolume(float left_volume, float right_volume);
    // 关闭音频设备
    void closeAudio(void);
    // 获取延迟时间
    double getLatencySeconds(void);
    // 设置默认的延迟时间
    void setDefaultLatencySeconds(double latency);
    // 获取回调
    int getAudioPerSecondCallbacks(void);

    // 设置播放速度
    void setPlaybackRate(float playbackRate);
    // 设置音量
    void setPlaybackVolume(float volume);

    // 释放音频设备
    void freeAudio(void);

    // 音频回调
    void audioCallback(SLAndroidSimpleBufferQueueItf caller);

    // 音频输出线程
    int aout_thread_n();

private:
    // 音频回调
    static void aout_opensles_callback(SLAndroidSimpleBufferQueueItf caller, void *pContext);
    static int aout_thread(void *arg);

    double minimal_latency_seconds;

    AudioOpaque * audioOpaque;
};

#endif //CAINPLAYER_AUDIOOUTPUT_H
