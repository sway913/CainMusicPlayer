package com.cgfay.cainmusicplayer.activity;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;

import com.cgfay.cainmusicplayer.R;
import com.cgfay.cainmusicplayer.media.CainMusicPlayer;

public class MusicPlayActivity extends AppCompatActivity {

    private static final String TAG = "MusicPlayActivity";
    private String mPath;

    private CainMusicPlayer mMusicPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_music_play);
        mPath = getIntent().getStringExtra("music_path");

        mMusicPlayer = new CainMusicPlayer();
        mMusicPlayer.setDataSource(mPath);
        mMusicPlayer.play();
        mMusicPlayer.setSpeed(1.0f);
    }

    @Override
    public void onBackPressed() {
        if (mMusicPlayer != null) {
            mMusicPlayer.stop();
            mMusicPlayer = null;
        }
        super.onBackPressed();
    }
}
