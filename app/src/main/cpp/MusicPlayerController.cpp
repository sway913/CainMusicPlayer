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
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_setSpeed(JNIEnv *env, jobject instance,
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
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_stop(JNIEnv *env, jobject instance) {

    if (mMusicPlayer) {
        mMusicPlayer->stop();
        mMusicPlayer->exitPlayer();
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_cgfay_cainmusicplayer_media_CainMusicPlayer_nativeRelease(JNIEnv *env, jobject instance) {

    if (mMusicPlayer) {
        mMusicPlayer->release();
        delete mMusicPlayer;
    }
}