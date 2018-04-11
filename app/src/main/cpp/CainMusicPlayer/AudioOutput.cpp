//
// Created by admin on 2018/4/9.
//

#include <assert.h>
#include "AudioOutput.h"
#include "../common/native_log.h"

#define OPENSLES_BUFFERS 255 /* maximum number of buffers */
#define OPENSLES_BUFLEN  10 /* ms */

static inline SLmillibel android_amplification_to_sles(float volumeLevel) {
    // FIXME use the FX Framework conversions
    if (volumeLevel < 0.00000001)
        return SL_MILLIBEL_MIN;

    SLmillibel mb = lroundf(2000.f * log10f(volumeLevel));
    if (mb < SL_MILLIBEL_MIN)
        mb = SL_MILLIBEL_MIN;
    else if (mb > 0)
        mb = 0; /* maximum supported level could be higher: GetMaxVolumeLevel */
    return mb;
}

AudioOutput::AudioOutput() {
    audioOpaque = NULL;
    minimal_latency_seconds = 0;
}

AudioOutput::~AudioOutput() {

}

/**
 * 初始化音频设备
 * @return
 */
int AudioOutput::initAudioOutput() {
    audioOpaque = (AudioOpaque *) mallocz(sizeof(AudioOpaque));
    if (!audioOpaque) {
        return -1;
    }
    audioOpaque->wakeup_cond = CondCreate();
    audioOpaque->wakeup_mutex = MutexCreate();

    int ret = 0;

    SLObjectItf slObject = NULL;
    ret = slCreateEngine(&slObject, 0, NULL, 0, NULL, NULL);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slCreateEngine() failed", __func__);
        freeAudio();
        return -1;
    }
    audioOpaque->slObject = slObject;

    ret = (*slObject)->Realize(slObject, SL_BOOLEAN_FALSE);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slObject->Realize() failed", __func__);
        freeAudio();
        return -1;
    }

    SLEngineItf slEngine = NULL;
    ret = (*slObject)->GetInterface(slObject, SL_IID_ENGINE, &slEngine);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slObject->GetInterface() failed", __func__);
        freeAudio();
        return -1;
    }
    audioOpaque->slEngine = slEngine;

    SLObjectItf slOutputMixObject = NULL;
    const SLInterfaceID ids1[] = {SL_IID_VOLUME};
    const SLboolean req1[] = {SL_BOOLEAN_FALSE};
    ret = (*slEngine)->CreateOutputMix(slEngine, &slOutputMixObject, 1, ids1, req1);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slEngine->CreateOutputMix() failed", __func__);
        freeAudio();
        return -1;
    }
    audioOpaque->slOutputMixObject = slOutputMixObject;

    ret = (*slOutputMixObject)->Realize(slOutputMixObject, SL_BOOLEAN_FALSE);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slOutputMixObject->Realize() failed", __func__);
        freeAudio();
        return -1;
    }

    return 0;
}

int AudioOutput::openAudio(const AudioSpec *desired, AudioSpec *obtained) {
    assert(desired);
    SLEngineItf       slEngine   = audioOpaque->slEngine;
    SLDataFormat_PCM *format_pcm = &audioOpaque->format_pcm;
    int               ret = 0;

    audioOpaque->spec = *desired;

    // config audio src
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            OPENSLES_BUFFERS
    };

    format_pcm->formatType       = SL_DATAFORMAT_PCM;
    format_pcm->numChannels      = desired->channels;
    format_pcm->samplesPerSec    = desired->freq * 1000; // milli Hz
    // format_pcm->numChannels      = 2;
    // format_pcm->samplesPerSec    = SL_SAMPLINGRATE_44_1;

    format_pcm->bitsPerSample    = SL_PCMSAMPLEFORMAT_FIXED_16;
    format_pcm->containerSize    = SL_PCMSAMPLEFORMAT_FIXED_16;
    switch (desired->channels) {
        case 2:
            format_pcm->channelMask  = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
            break;

        case 1:
            format_pcm->channelMask  = SL_SPEAKER_FRONT_CENTER;
            break;

        default: {
            ALOGE("%s, invalid channel %d", __func__, desired->channels);
            closeAudio();
            return -1;
        }
    }
    format_pcm->endianness       = SL_BYTEORDER_LITTLEENDIAN;

    SLDataSource audio_source = {&loc_bufq, format_pcm};

    // config audio sink
    SLDataLocator_OutputMix loc_outmix = {
            SL_DATALOCATOR_OUTPUTMIX,
            audioOpaque->slOutputMixObject
    };
    SLDataSink audio_sink = {&loc_outmix, NULL};

    SLObjectItf slPlayerObject = NULL;
    const SLInterfaceID ids2[] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME, SL_IID_PLAY };
    static const SLboolean req2[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };
    ret = (*slEngine)->CreateAudioPlayer(slEngine, &slPlayerObject, &audio_source,
                                         &audio_sink, sizeof(ids2) / sizeof(*ids2),
                                         ids2, req2);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slEngine->CreateAudioPlayer() failed", __func__);
        closeAudio();
        return -1;
    }
    audioOpaque->slPlayerObject = slPlayerObject;

    ret = (*slPlayerObject)->Realize(slPlayerObject, SL_BOOLEAN_FALSE);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slPlayerObject->Realize() failed", __func__);
        closeAudio();
        return -1;
    }

    ret = (*slPlayerObject)->GetInterface(slPlayerObject, SL_IID_PLAY, &audioOpaque->slPlayItf);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slPlayerObject->GetInterface(SL_IID_PLAY) failed", __func__);
        closeAudio();
        return -1;
    }

    ret = (*slPlayerObject)->GetInterface(slPlayerObject, SL_IID_VOLUME, &audioOpaque->slVolumeItf);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slPlayerObject->GetInterface(SL_IID_VOLUME) failed", __func__);
        closeAudio();
        return -1;
    }

    ret = (*slPlayerObject)->GetInterface(slPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &audioOpaque->slBufferQueueItf);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slPlayerObject->GetInterface(SL_IID_ANDROIDSIMPLEBUFFERQUEUE) failed", __func__);
        closeAudio();
        return -1;
    }

    ret = (*audioOpaque->slBufferQueueItf)->RegisterCallback(audioOpaque->slBufferQueueItf, aout_opensles_callback, (void*)this);
    if ((ret) != SL_RESULT_SUCCESS)  {
        ALOGE("%s: slBufferQueueItf->RegisterCallback() failed", __func__);
        closeAudio();
        return -1;
    }

    audioOpaque->bytes_per_frame   = format_pcm->numChannels * format_pcm->bitsPerSample / 8;
    audioOpaque->milli_per_buffer  = OPENSLES_BUFLEN;
    audioOpaque->frames_per_buffer = audioOpaque->milli_per_buffer * format_pcm->samplesPerSec / 1000000; // samplesPerSec audioState in milli
    audioOpaque->bytes_per_buffer  = audioOpaque->bytes_per_frame * audioOpaque->frames_per_buffer;
    audioOpaque->buffer_capacity   = OPENSLES_BUFFERS * audioOpaque->bytes_per_buffer;
    ALOGI("OpenSL-ES: bytes_per_frame  = %d bytes\n",  (int)audioOpaque->bytes_per_frame);
    ALOGI("OpenSL-ES: milli_per_buffer = %d ms\n",     (int)audioOpaque->milli_per_buffer);
    ALOGI("OpenSL-ES: frame_per_buffer = %d frames\n", (int)audioOpaque->frames_per_buffer);
    ALOGI("OpenSL-ES: bytes_per_buffer = %d bytes\n",  (int)audioOpaque->bytes_per_buffer);
    ALOGI("OpenSL-ES: buffer_capacity  = %d bytes\n",  (int)audioOpaque->buffer_capacity);
    audioOpaque->buffer = (uint8_t *)malloc(audioOpaque->buffer_capacity);
    if (!audioOpaque->buffer) {
        ALOGE("%s: failed to alloc buffer %d\n", __func__, (int)audioOpaque->buffer_capacity);
        closeAudio();
        return -1;
    }

    // enqueue empty buffer to start play
    memset(audioOpaque->buffer, 0, audioOpaque->buffer_capacity);
    for(int i = 0; i < OPENSLES_BUFFERS; ++i) {
        ret = (*audioOpaque->slBufferQueueItf)->Enqueue(audioOpaque->slBufferQueueItf, audioOpaque->buffer + i * audioOpaque->bytes_per_buffer, audioOpaque->bytes_per_buffer);
        if ((ret) != SL_RESULT_SUCCESS)  {
            ALOGE("%s: slBufferQueueItf->Enqueue(000...) failed", __func__);
            closeAudio();
            return -1;
        }
    }

    audioOpaque->pause_on = 1;
    audioOpaque->abort_request = 0;
    audioOpaque->audio_tid = ThreadCreate(aout_thread, this, "AudioOutputThread");
    if (!audioOpaque->audio_tid) {
        ALOGE("%s: failed to SDL_CreateThreadEx", __func__);
        closeAudio();
        return -1;
    }

    if (obtained) {
        *obtained      = *desired;
        obtained->size = audioOpaque->buffer_capacity;
        obtained->freq = format_pcm->samplesPerSec / 1000;
    }

    return audioOpaque->buffer_capacity;

}

void AudioOutput::pauseAudio(int pause_on) {
    MutexLock(audioOpaque->wakeup_mutex);
    audioOpaque->pause_on = pause_on;
    if (!pause_on) {
        CondSignal(audioOpaque->wakeup_cond);
    }
    MutexUnlock(audioOpaque->wakeup_mutex);
}

void AudioOutput::flushAudio() {
    MutexLock(audioOpaque->wakeup_mutex);
    audioOpaque->need_flush = 1;
    CondSignal(audioOpaque->wakeup_cond);
    MutexUnlock(audioOpaque->wakeup_mutex);
}

void AudioOutput::setStereoVolume(float left_volume, float right_volume) {
    MutexLock(audioOpaque->wakeup_mutex);
    ALOGI("aout_set_volume(%f, %f)", left_volume, right_volume);
    audioOpaque->left_volume = left_volume;
    audioOpaque->right_volume = right_volume;
    audioOpaque->need_set_volume = 1;
    CondSignal(audioOpaque->wakeup_cond);
    MutexUnlock(audioOpaque->wakeup_mutex);
}

void AudioOutput::closeAudio() {
    if (!audioOpaque) {
        return;
    }

    MutexLock(audioOpaque->wakeup_mutex);
    audioOpaque->abort_request = true;
    CondSignal(audioOpaque->wakeup_cond);
    MutexUnlock(audioOpaque->wakeup_mutex);

    ThreadWait(audioOpaque->audio_tid, NULL);
    audioOpaque->audio_tid = NULL;

    if (audioOpaque->slPlayItf) {
        (*audioOpaque->slPlayItf)->SetPlayState(audioOpaque->slPlayItf, SL_PLAYSTATE_STOPPED);
    }
    if (audioOpaque->slBufferQueueItf)
        (*audioOpaque->slBufferQueueItf)->Clear(audioOpaque->slBufferQueueItf);

    if (audioOpaque->slBufferQueueItf) {
        audioOpaque->slBufferQueueItf = NULL;
    }
    if (audioOpaque->slVolumeItf) {
        audioOpaque->slVolumeItf = NULL;
    }
    if (audioOpaque->slPlayItf) {
        audioOpaque->slPlayItf = NULL;
    }

    if (audioOpaque->slPlayerObject) {
        (*audioOpaque->slPlayerObject)->Destroy(audioOpaque->slPlayerObject);
        audioOpaque->slPlayerObject = NULL;
    }

    freep((void **)&audioOpaque->buffer);
}

double AudioOutput::getLatencySeconds() {
    SLAndroidSimpleBufferQueueState state = {0};
    SLresult slRet = (*audioOpaque->slBufferQueueItf)->GetState(audioOpaque->slBufferQueueItf, &state);
    if (slRet != SL_RESULT_SUCCESS) {
        ALOGE("%s failed\n", __func__);
        return ((double)audioOpaque->milli_per_buffer) * OPENSLES_BUFFERS / 1000;
    }

    // assume there audioState always a buffer in coping
    double latency = ((double)audioOpaque->milli_per_buffer) * state.count / 1000;
    return latency;
}

void AudioOutput::setDefaultLatencySeconds(double latency) {
    minimal_latency_seconds = latency;
}

int AudioOutput::getAudioPerSecondCallbacks() {

    return SDL_AUDIO_MAX_CALLBACKS_PER_SEC;
}

void AudioOutput::setPlaybackRate(float playbackRate) {

}

void AudioOutput::setPlaybackVolume(float volume) {

}

void AudioOutput::freeAudio() {
    flushAudio();
    closeAudio();

    if (!audioOpaque) {
        return;
    }

    if (audioOpaque->slOutputMixObject) {
        (*audioOpaque->slOutputMixObject)->Destroy(audioOpaque->slOutputMixObject);
        audioOpaque->slOutputMixObject = NULL;
    }

    audioOpaque->slEngine = NULL;

    if (audioOpaque->slObject) {
        (*audioOpaque->slObject)->Destroy(audioOpaque->slObject);
        audioOpaque->slObject = NULL;
    }

    CondDestroyPointer(&audioOpaque->wakeup_cond);
    MutexDestroyPointer(&audioOpaque->wakeup_mutex);

    if (audioOpaque) {
        free(audioOpaque);
        audioOpaque = NULL;
    }
}

void AudioOutput::audioCallback(SLAndroidSimpleBufferQueueItf caller) {
    if (audioOpaque) {
        MutexLock(audioOpaque->wakeup_mutex);
        audioOpaque->is_running = true;
        CondSignal(audioOpaque->wakeup_cond);
        MutexUnlock(audioOpaque->wakeup_mutex);
    }
}

void AudioOutput::aout_opensles_callback(SLAndroidSimpleBufferQueueItf caller, void *pContext) {
    AudioOutput * output = (AudioOutput *) pContext;
    output->audioCallback(caller);
}

int AudioOutput::aout_thread(void *arg) {
    AudioOutput *output = (AudioOutput *) arg;
    return output->aout_thread_n();
}

/**
 * 音频输出线程实体
 * @return
 */
int AudioOutput::aout_thread_n() {
    SLPlayItf                      slPlayItf        = audioOpaque->slPlayItf;
    SLAndroidSimpleBufferQueueItf  slBufferQueueItf = audioOpaque->slBufferQueueItf;
    SLVolumeItf                    slVolumeItf      = audioOpaque->slVolumeItf;
    AudioCallback                  audio_cblk       = audioOpaque->spec.callback;
    void                          *userdata         = audioOpaque->spec.userdata;
    uint8_t                       *next_buffer      = NULL;
    int                            next_buffer_index = 0;
    size_t                         bytes_per_buffer = audioOpaque->bytes_per_buffer;

    ThreadSetPriority(THREAD_PRIORITY_HIGH);

    if (!audioOpaque->abort_request && !audioOpaque->pause_on) {
        (*slPlayItf)->SetPlayState(slPlayItf, SL_PLAYSTATE_PLAYING);
    }

    while (!audioOpaque->abort_request) {
        SLAndroidSimpleBufferQueueState slState = {0};

        SLresult slRet = (*slBufferQueueItf)->GetState(slBufferQueueItf, &slState);
        if (slRet != SL_RESULT_SUCCESS) {
            ALOGE("%s: slBufferQueueItf->GetState() failed\n", __func__);
            MutexUnlock(audioOpaque->wakeup_mutex);
        }

        MutexLock(audioOpaque->wakeup_mutex);
        if (!audioOpaque->abort_request && (audioOpaque->pause_on || slState.count >= OPENSLES_BUFFERS)) {
            while (!audioOpaque->abort_request && (audioOpaque->pause_on || slState.count >= OPENSLES_BUFFERS)) {
                if (!audioOpaque->pause_on) {
                    (*slPlayItf)->SetPlayState(slPlayItf, SL_PLAYSTATE_PLAYING);
                }
                CondWaitTimeout(audioOpaque->wakeup_cond, audioOpaque->wakeup_mutex, 1000);
                SLresult slRet = (*slBufferQueueItf)->GetState(slBufferQueueItf, &slState);
                if (slRet != SL_RESULT_SUCCESS) {
                    ALOGE("%s: slBufferQueueItf->GetState() failed\n", __func__);
                    MutexUnlock(audioOpaque->wakeup_mutex);
                }

                if (audioOpaque->pause_on) {
                    (*slPlayItf)->SetPlayState(slPlayItf, SL_PLAYSTATE_PAUSED);
                }
            }
            if (!audioOpaque->abort_request && !audioOpaque->pause_on) {
                (*slPlayItf)->SetPlayState(slPlayItf, SL_PLAYSTATE_PLAYING);
            }
        }
        if (audioOpaque->need_flush) {
            audioOpaque->need_flush = 0;
            (*slBufferQueueItf)->Clear(slBufferQueueItf);
        }

        if (audioOpaque->need_set_volume) {
            audioOpaque->need_set_volume = 0;
            SLmillibel level = android_amplification_to_sles((audioOpaque->left_volume + audioOpaque->right_volume) / 2);
            ALOGI("slVolumeItf->SetVolumeLevel((%f, %f) -> %d)\n", audioOpaque->left_volume, audioOpaque->right_volume, (int)level);
            slRet = (*slVolumeItf)->SetVolumeLevel(slVolumeItf, level);
            if (slRet != SL_RESULT_SUCCESS) {
                ALOGE("slVolumeItf->SetVolumeLevel failed %d\n", (int)slRet);
                // just ignore error
            }
        }
        MutexUnlock(audioOpaque->wakeup_mutex);

        next_buffer = audioOpaque->buffer + next_buffer_index * bytes_per_buffer;
        next_buffer_index = (next_buffer_index + 1) % OPENSLES_BUFFERS;
        audio_cblk(userdata, next_buffer, bytes_per_buffer);
        if (audioOpaque->need_flush) {
            (*slBufferQueueItf)->Clear(slBufferQueueItf);
            audioOpaque->need_flush = false;
        }

        if (audioOpaque->need_flush) {
            ALOGE("flush");
            audioOpaque->need_flush = 0;
            (*slBufferQueueItf)->Clear(slBufferQueueItf);
        } else {
            slRet = (*slBufferQueueItf)->Enqueue(slBufferQueueItf, next_buffer, bytes_per_buffer);
            if (slRet == SL_RESULT_SUCCESS) {
                // do nothing
            } else if (slRet == SL_RESULT_BUFFER_INSUFFICIENT) {
                // don't retry, just pass through
                ALOGE("SL_RESULT_BUFFER_INSUFFICIENT\n");
            } else {
                ALOGE("slBufferQueueItf->Enqueue() = %d\n", (int)slRet);
                break;
            }
        }
    }

    return 0;
}