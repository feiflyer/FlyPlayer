package com.flyer.flyplayer

import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.view.View
import com.flyer.ffmpeg.FFmpegUtils
import kotlinx.android.synthetic.main.activity_main.*

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        sample_text.text = "ffmpeg版本:${FFmpegUtils.getFFmpegVersion()}"
    }


    fun analyzeMedia(view: View) {
        val videoPath = "/sdcard/Download/video.mp4"
        FFmpegUtils.analyzeMedia(videoPath)
    }

    fun decode2yuv(view: View) {

        val videoPath = "/sdcard/Download/video.mov"
        val yuvPath = "/sdcard/Download/video_yuv.yuv"
        FFmpegUtils.decodeVideo2YUV(videoPath,yuvPath)
    }

    fun playVideo(view: View) {
        startActivity(Intent(this,PlayerActivity::class.java))
    }

    fun playAudio(view: View) {
        FFmpegUtils.playAudio("/sdcard/Download/video.mov")
    }

}
