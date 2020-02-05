package com.flyer.flyplayer

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import com.flyer.ffmpeg.FFmpegUtils
import kotlinx.android.synthetic.main.activity_main.*

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        sample_text.text = "ffmpeg版本:${FFmpegUtils.getFFmpegVersion()}"
    }

}
