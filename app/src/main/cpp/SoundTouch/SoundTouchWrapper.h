//
// Created by admin on 2018/4/10.
//

#ifndef MUSICPLAYER_SOUNDTOUCHWRAPPER_H
#define MUSICPLAYER_SOUNDTOUCHWRAPPER_H

#include <stdint.h>
#include "SoundTouch.h"

using namespace std;

using namespace soundtouch;

class SoundTouchWrapper {

public:
    SoundTouchWrapper();

    virtual ~SoundTouchWrapper();
    // 初始化
    void create();
    // 转换
    int translate(short* data, int len, int bytes_per_sample, int n_channel, int n_sampleRate);
    // 销毁
    void destroy();

    // 设置声道
    inline void setChannels(uint numChannels) {
        if (mSoundTouch != NULL) {
            mSoundTouch->setChannels(numChannels);
        }
    }

    // 设置采样率
    inline void setSampleRate(uint srate) {
        if (mSoundTouch != NULL) {
            mSoundTouch->setSampleRate(srate);
        }
    }

    // 设置播放速度
    inline void setRate(double rate) {
        if (mSoundTouch != NULL) {
            mSoundTouch->setRate(rate);
        }
    }

    // 设置音调
    inline void setPitch(double pitch) {
        if (mSoundTouch != NULL) {
            mSoundTouch->setPitch(pitch);
        }
    }

    // 设置节拍
    inline void setTempo(double tempo) {
        if (mSoundTouch != NULL) {
            mSoundTouch->setTempo(tempo);
        }
    }

    // 设置速度改变
    inline void setRateChange(double rateChange) {
        if (mSoundTouch != NULL) {
            mSoundTouch->setRateChange(rateChange);
        }
    }

    // 设置节拍改变
    inline void setTempoChange(double tempoChange) {
        if (mSoundTouch != NULL) {
            mSoundTouch->setTempoChange(tempoChange);
        }
    }

    // 设置八度音调节
    inline void setPitchOctaves(double pitchOctaves) {
        if (mSoundTouch != NULL) {
            mSoundTouch->setPitchOctaves(pitchOctaves);
        }
    }

    // 设置半音调节
    inline void setPitchSemiTones(double semiTones) {
        if (mSoundTouch != NULL) {
            mSoundTouch->setPitchSemiTones(semiTones);
        }
    }

private:
    SoundTouch *mSoundTouch;
};


#endif //MUSICPLAYER_SOUNDTOUCHWRAPPER_H
