#include <jni.h>
#include <string>

#include "CainMusicPlayer/MusicPlayer.h"

MusicPlayer *mMusicPlayer = NULL;

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_nativeSetup(JNIEnv *env, jobject instance) {
    if (!mMusicPlayer) {
        mMusicPlayer = new MusicPlayer();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_setDataSource(JNIEnv *env, jobject instance,
                                                          jstring path_) {
    const char *path = env->GetStringUTFChars(path_, 0);

    if (mMusicPlayer) {
        mMusicPlayer->setDataSource(path);
    }

    env->ReleaseStringUTFChars(path_, path);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_setPitch(JNIEnv *env, jobject instance,
                                                              jfloat pitch) {
    if (mMusicPlayer) {
        mMusicPlayer->setPlaybackPitch(pitch);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_setRate(JNIEnv *env, jobject instance,
                                                              jfloat speed) {

    if (mMusicPlayer) {
        mMusicPlayer->setPlaybackRate(speed);
    }

}


extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_pause(JNIEnv *env, jobject instance) {

    if (mMusicPlayer) {
        mMusicPlayer->pause();
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_play(JNIEnv *env, jobject instance) {

    if (mMusicPlayer) {
        int ret = mMusicPlayer->prepare();
        if (!ret) {
            mMusicPlayer->play();
        }
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_isPlaying(JNIEnv *env, jobject instance) {

    if (mMusicPlayer) {
        mMusicPlayer->isPlaying();
    }

    return 0;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_isLooping(JNIEnv *env, jobject instance) {

    if (mMusicPlayer) {
        mMusicPlayer->isLooping();
    }

    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_stop(JNIEnv *env, jobject instance) {

    if (mMusicPlayer) {
        mMusicPlayer->stop();
        mMusicPlayer->closePlayer();
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_nativeRelease(JNIEnv *env, jobject instance) {

    if (mMusicPlayer) {
        mMusicPlayer->release();
        delete mMusicPlayer;
        mMusicPlayer = NULL;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_setLooping(JNIEnv *env, jobject instance,
                                                                jboolean looping) {
    if (mMusicPlayer) {
        mMusicPlayer->setLooping(looping);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_seekTo(JNIEnv *env, jobject instance,
                                                            jlong msec) {
    if (mMusicPlayer) {
        mMusicPlayer->seekTo(msec);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_setTempo(JNIEnv *env, jobject instance,
                                                              jdouble tempo) {
    if (mMusicPlayer) {
        mMusicPlayer->setTempo(tempo);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_setRateChange(JNIEnv *env, jobject instance,
                                                                   jdouble newRate) {
    if (mMusicPlayer) {
        mMusicPlayer->setRateChange(newRate);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_setTempoChange(JNIEnv *env, jobject instance,
                                                                    jdouble newTempo) {
    if (mMusicPlayer) {
        mMusicPlayer->setTempoChange(newTempo);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_setPitchOctaves(JNIEnv *env, jobject instance,
                                                                     jdouble newPitch) {
    if (mMusicPlayer) {
        mMusicPlayer->setPitchOctaves(newPitch);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_setPitchSemiTones(JNIEnv *env,
                                                                       jobject instance,
                                                                       jdouble newPitch) {
    if (mMusicPlayer) {
        mMusicPlayer->setPitchSemiTones(newPitch);
    }
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_getCurrentPosition(JNIEnv *env,
                                                                        jobject instance) {
    if (mMusicPlayer) {
        return mMusicPlayer->getCurrentPosition();
    }
    return 0;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_getDuration(JNIEnv *env, jobject instance) {

    if (mMusicPlayer) {
        return mMusicPlayer->getDuration();
    }
    return 0;
}