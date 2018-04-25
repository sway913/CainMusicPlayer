package com.cgfay.cainmusicplayer.activity;

import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;

import com.cgfay.cainmusicplayer.R;
import com.cgfay.cainmusicplayer.media.CainMusicPlayer;
import com.cgfay.cainmusicplayer.utils.StringUtils;

public class MusicPlayActivity extends AppCompatActivity
        implements View.OnClickListener, SeekBar.OnSeekBarChangeListener {

    private static final String TAG = "MusicPlayActivity";
    private String mPath;

    private CainMusicPlayer mMusicPlayer;

    private TextView mTvCurrent;
    private TextView mTvDuration;

    private SeekBar mSeekProcess;

    private Button mBtnReset;

    private SeekBar mSbSpeed;
    private SeekBar mSbPitch;
    private SeekBar mSbTempo;
    private SeekBar mSbPitchOctaves;
    private SeekBar mSbPitchSemiTones;

    private TextView mTvMusicValue;

    private float mSpeed = 1.0f;
    private float mPitch = 1.0f;
    private float mTempo = 1.0f;
    private float mPitchOctaves = 0.0f;
    private float mPitchSemiTones = 0.0f;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_music_play);
        mPath = getIntent().getStringExtra("music_path");
        mMusicPlayer = new CainMusicPlayer();
        mMusicPlayer.setDataSource(mPath);
        mMusicPlayer.play();
        mMusicPlayer.setRate(1.0f);
        initView();

    }

    public void initView() {

        mTvCurrent = (TextView) findViewById(R.id.tv_current);
        mTvDuration = (TextView) findViewById(R.id.tv_duration);
        mSeekProcess = (SeekBar) findViewById(R.id.play_progress);
        mSeekProcess.setOnSeekBarChangeListener(this);

        mBtnReset = (Button) findViewById(R.id.btn_reset);
        mBtnReset.setOnClickListener(this);

        mTvMusicValue = (TextView) findViewById(R.id.tv_music_value);
        updateMusicTextView();
        mSbSpeed = (SeekBar) findViewById(R.id.sb_speed);
        mSbPitch = (SeekBar) findViewById(R.id.sb_pitch);
        mSbTempo = (SeekBar) findViewById(R.id.sb_tempo);
        mSbPitchOctaves = (SeekBar) findViewById(R.id.sb_pitch_octaves);
        mSbPitchSemiTones = (SeekBar) findViewById(R.id.sb_pitch_semitones);


        mSbSpeed.setOnSeekBarChangeListener(this);
        mSbSpeed.setMax(100);
        mSbSpeed.setProgress(50);

        mSbPitch.setOnSeekBarChangeListener(this);
        mSbPitch.setMax(100);
        mSbPitch.setProgress(50);

        mSbTempo.setOnSeekBarChangeListener(this);
        mSbTempo.setMax(100);
        mSbTempo.setProgress(50);

        mSbPitchOctaves.setOnSeekBarChangeListener(this);
        mSbPitchOctaves.setMax(100);
        mSbPitchOctaves.setProgress(50);

        mSbPitchSemiTones.setOnSeekBarChangeListener(this);
        mSbPitchSemiTones.setMax(100);
        mSbPitchSemiTones.setProgress(50);

        mProcessHandler.sendEmptyMessage(0);
    }


    private Handler mProcessHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mTvCurrent.setText(StringUtils.generateStandardTime((int)mMusicPlayer.getCurrentPosition()));
                    mTvDuration.setText(StringUtils.generateStandardTime((int)mMusicPlayer.getDuration()));
                    mSeekProcess.setMax((int) mMusicPlayer.getDuration());
                    mSeekProcess.setProgress((int) mMusicPlayer.getCurrentPosition());
                }
            });

            if (!backPress && mProcessHandler != null) {
                mProcessHandler.sendEmptyMessageDelayed(0, 50);
            }
        }
    };

    private volatile boolean backPress = false;

    @Override
    public void onBackPressed() {
        if (mMusicPlayer != null) {
            mMusicPlayer.stop();
            mMusicPlayer = null;
        }
        backPress = true;
        mProcessHandler.removeCallbacksAndMessages(null);
        mProcessHandler = null;
        super.onBackPressed();
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        switch (seekBar.getId()) {

            case R.id.play_progress:
                if (fromUser) {
                    mMusicPlayer.seekTo(progress);
                }
                break;

            case R.id.sb_speed:
                if (fromUser) {
                    // 0.5 ~ 2.0
                    if (progress < 50) {
                        mSpeed  = 0.5f + (float) progress / 100f;
                    } else {
                        mSpeed = 1.0f + (float) (progress - 50) * 2 / 100f;
                    }
                    mMusicPlayer.setRate(mSpeed);
                }
                break;

            case R.id.sb_pitch:
                if (fromUser) {
                    // 0.5 ~ 2.0
                    if (progress < 50) {
                        mPitch = 0.5f + (float) progress / 100f;
                    } else {
                        mPitch = 1.0f + (float) (progress - 50) * 2 / 100;
                    }
                    mMusicPlayer.setPitch(mPitch);
                }
                break;

            case R.id.sb_tempo:
                if (fromUser) {
                    // 0.5 ~ 2.0
                    if (progress < 50) {
                        mTempo = 0.5f + (float) progress / 100f;
                    } else {
                        mTempo = 1.0f + (float) (progress - 50) * 2 / 100;
                    }
                    mMusicPlayer.setTempo(mTempo);
                }
                break;

            case R.id.sb_pitch_octaves:
                if (fromUser) {
                    // -1 ~ 1
                    mPitchOctaves =  2 * (((float)progress / 100f) - 0.5f);
                    mMusicPlayer.setPitchOctaves(mPitchOctaves);
                }
                break;

            case R.id.sb_pitch_semitones:
                if (fromUser) {
                    // -12 ~ 12
                    mPitchSemiTones = 24 * (((float)progress / 100f) - 0.5f);
                    mMusicPlayer.setPitchSemiTones(mPitchSemiTones);
                }
                break;
        }
        updateMusicTextView();
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.btn_reset:
                mSpeed = 1.0f;
                mPitch = 1.0f;
                mTempo = 1.0f;
                mPitchOctaves = 0.0f;
                mPitchSemiTones = 0.0f;
                mMusicPlayer.setRate(mSpeed);
                mMusicPlayer.setPitch(mPitch);
                mMusicPlayer.setTempo(mTempo);
                mMusicPlayer.setPitchOctaves(mPitchOctaves);
                mMusicPlayer.setPitchSemiTones(mPitchSemiTones);

                mSbSpeed.setProgress(50);
                mSbPitch.setProgress(50);
                mSbTempo.setProgress(50);
                mSbPitchOctaves.setProgress(50);
                mSbPitchSemiTones.setProgress(50);

                updateMusicTextView();
                break;
        }
    }

    private void updateMusicTextView() {
        String value = "速度: " + mSpeed + "\n"
                + "音调：" + mPitch + "\n"
                + "节拍：" + mTempo + "\n"
                + "八度音调节：" + mPitchOctaves + "\n"
                + "半音阶调节：" + mPitchSemiTones + "\n";
        mTvMusicValue.setText(value);
    }

}
