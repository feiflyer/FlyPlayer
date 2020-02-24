package com.flyer.flyplayer;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;

import com.flyer.ffmpeg.FFmpegUtils;
import com.flyer.ffmpeg.FlyPlayer;

public class PlayerActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_player);

        FlyPlayer flyPlayer = findViewById(R.id.player);

    }
}
