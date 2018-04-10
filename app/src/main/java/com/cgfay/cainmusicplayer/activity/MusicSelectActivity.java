package com.cgfay.cainmusicplayer.activity;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.os.AsyncTask;
import android.os.Environment;
import android.provider.MediaStore;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

import com.cgfay.cainmusicplayer.media.Music;
import com.cgfay.cainmusicplayer.media.MusicAdapter;
import com.cgfay.cainmusicplayer.R;
import com.cgfay.cainmusicplayer.utils.PermissionUtils;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class MusicSelectActivity extends AppCompatActivity {

    public static final int RESULT_MUSIC = 0x100;

    private static final int REQUEST_CODE = 0;

    private ListView mListView;
    private MusicAdapter mAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_music_select);
        mListView = (ListView) findViewById(R.id.music_list);
        checkPermissions();
    }

    /**
     * 检查权限
     */
    private void checkPermissions() {
        boolean storageWriteEnable = PermissionUtils.permissionChecking(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE);
        if (!storageWriteEnable) {
            ActivityCompat.requestPermissions(this,
                    new String[] {
                            Manifest.permission.WRITE_EXTERNAL_STORAGE
                    }, REQUEST_CODE);
        } else {
            scanMusic();
        }
    }


    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_CODE && grantResults.length > 0
                && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            scanMusic();
        }
    }


    private void scanMusic() {
        new MusicSelectTask().execute();
    }

    private class MusicSelectTask extends AsyncTask<Void, Void, List<Music>> implements AdapterView.OnItemClickListener {
        @Override
        protected void onPreExecute() {
            super.onPreExecute();
        }

        @Override
        protected List<Music> doInBackground(Void... voids) {
            List<Music> musics = new ArrayList<>();
            Cursor cursor = getApplicationContext().getContentResolver().query(
                    MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, null,
                    MediaStore.Audio.Media.DATA + " like ?",
                    new String[]{Environment.getExternalStorageDirectory() + File.separator + "%"},
                    MediaStore.Audio.Media.DEFAULT_SORT_ORDER);
            if (cursor != null) {
                for (cursor.moveToFirst(); !cursor.isAfterLast(); cursor.moveToNext()) {
                    String isMusic = cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.IS_MUSIC));
                    if (isMusic != null && isMusic.equals("")) continue;
                    int duration = cursor.getInt(cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DURATION));
                    String path = cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DATA));
                    if (!path.endsWith(".mp3") || duration<60 * 1000) {
                        continue;
                    }
                    Music music = new Music();
                    music.setDuration(duration);
                    String title = cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.TITLE));
                    String artist = cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.ARTIST));
                    music.setId(cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Audio.Media._ID)));
                    music.setName(title);
                    music.setSingerName(artist);
                    music.setSongUrl(path);
                    musics.add(music);
                }
                cursor.close();
            }
            return musics;
        }

        @Override
        protected void onPostExecute(List<Music> musics) {
            super.onPostExecute(musics);
            mAdapter = new MusicAdapter(MusicSelectActivity.this, musics);
            mListView.setAdapter(mAdapter);
            mListView.setOnItemClickListener(this);
        }

        @Override
        public void onItemClick(AdapterView<?> adapterView, View view, int i, long l) {
            Music music = mAdapter.getItem(i);
            Intent intent = new Intent(MusicSelectActivity.this, MusicPlayActivity.class);
            intent.putExtra("music_path", music.getSongUrl());
            startActivity(intent);
        }
    }
}
