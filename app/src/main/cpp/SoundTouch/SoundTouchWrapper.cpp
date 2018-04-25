//
// Created by admin on 2018/4/10.
//

#include "SoundTouchWrapper.h"

SoundTouchWrapper::SoundTouchWrapper() {
    create();
}

SoundTouchWrapper::~SoundTouchWrapper() {
    destroy();
}

void SoundTouchWrapper::create() {
    mSoundTouch = new SoundTouch();
}

void SoundTouchWrapper::destroy() {
    if (mSoundTouch) {
        mSoundTouch->clear();
        delete mSoundTouch;
        mSoundTouch = NULL;
    }
}

/**
 * 转换
 * @param data              待采样PCM数据
 * @param len               长度
 * @param bytes_per_sample  采样字节数
 * @param n_channel         声道数
 * @param n_sampleRate      采样率
 * @return                  转换后的PCM数据大小
 */
int SoundTouchWrapper::translate(short *data, int len, int bytes_per_sample,
                                 int n_channel, int n_sampleRate) {
    // 每个声道采样数量
    int put_n_sample = len / n_channel;
    int nb = 0;
    int pcm_data_size = 0;
    if (mSoundTouch == NULL) {
        return 0;
    }
    // 压入采样数据
    mSoundTouch->putSamples((SAMPLETYPE*)data, put_n_sample);

    do {
        // 获取转换后的数据
        nb = mSoundTouch->receiveSamples((SAMPLETYPE*)data, n_sampleRate / n_channel);
        // 计算转换后的数量大小
        pcm_data_size += nb * n_channel * bytes_per_sample;
    } while (nb != 0);
    // 返回转换后的数量大小
    return pcm_data_size;
}