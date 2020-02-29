package com.flyer.ffmpeg;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class FlyPlayer extends SurfaceView implements SurfaceHolder.Callback,Runnable {

    public FlyPlayer(Context context, AttributeSet attrs) {
        super(context, attrs);
        //设置callback
        getHolder().addCallback(this);
    }

    @Override
    public void surfaceCreated(SurfaceHolder surfaceHolder) {
        // 开启线程
        new Thread(this).start();
    }

    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {

    }

    @Override
    public void run() {

//      playVideo("/sdcard/Download/video.mov",getHolder().getSurface());
      playVideoByOpenGL("/sdcard/Download/video.mp4",getHolder().getSurface());
    }


    public native void playVideo(String videoPath, Surface surface);

    public native void playVideoByOpenGL(String videoPath, Surface surface);
}
