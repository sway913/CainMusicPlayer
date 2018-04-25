package com.cgfay.cainmusicplayer.media;

public class CainMusicPlayer {

    static {
        System.loadLibrary("ffmpeg");
        System.loadLibrary("cainmusicplayer");
    }
    // 初始化
    public native void nativeSetup();
    // 播放
    public native void setDataSource(String path);
    // 设置音调
    public native void setPitch(float pitch);
    // 设置播放速度
    public native void setRate(float speed);
    // 暂停
    public native void pause();
    // 播放
    public native void play();
    // 是否正处于播放状态
    public native boolean isPlaying();
    // 是否处于循环播放状态
    public native boolean isLooping();
    // 停止
    public native void stop();
    // 释放资源
    public native void nativeRelease();
    // 设置是否循环播放
    public native void setLooping(boolean looping);
    // 指定播放位置
    public native void seekTo(long msec);
    // 设置节拍
    public native void setTempo(double tempo);
    // 改变速度 -50 ~ 100%
    public native void setRateChange(double newRate);
    // 改变节拍，-50 ~ 100%
    public native void setTempoChange(double newTempo);
    // 在原音调基础上，以八度音为单位进行调整(-1.0, 1.0)
    public native void setPitchOctaves(double newPitch);
    // 在原音调基础上，以半音为单位进行调整(-12, 12);
    public native void setPitchSemiTones(double newPitch);
    // 当前进度
    public native long getCurrentPosition();
    // 总时长
    public native long getDuration();

    public CainMusicPlayer() {
        nativeSetup();
    }

    @Override
    protected void finalize() throws Throwable {
        nativeRelease();
        super.finalize();
    }
}
