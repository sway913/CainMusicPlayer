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
    public native void setSpeed(float speed);
    // 暂停
    public native void pause();
    // 播放
    public native void play();
    // 是否正处于播放状态
    public native boolean isPlaying();
    // 停止
    public native void stop();
    // 释放资源
    public native void nativeRelease();

    public CainMusicPlayer() {
        nativeSetup();
    }

    @Override
    protected void finalize() throws Throwable {
        nativeRelease();
        super.finalize();
    }
}
