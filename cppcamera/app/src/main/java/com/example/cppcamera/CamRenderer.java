package com.example.cppcamera;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import static android.opengl.GLES11Ext.GL_TEXTURE_EXTERNAL_OES;

public class CamRenderer implements SurfaceTexture.OnFrameAvailableListener, GLSurfaceView.Renderer {
    public SurfaceTexture surfaceTexture;
    public float[] texMatrix = new float[16];
    public Boolean frameAvailable = false;
    public Object lock = new Object();

    public static native void onSurfaceCreated(int textureId, Surface surface);
    public static native void glStartPreview(int textureId, Surface surface);
    public static native void onSurfaceChanged(int width, int height);
    public static native void onDrawFrame(float []texMat);
    static {
        System.loadLibrary("native-camera2-jni");
    }

    @Override
    public void onFrameAvailable(SurfaceTexture sur) {
        synchronized (lock) {
            frameAvailable = true;
        }
    }

    @Override
    public void onDrawFrame(GL10 p10) {
        synchronized (lock) {
            if (frameAvailable) {
                Log.d("sixo", "Frame available...updating");
                surfaceTexture.updateTexImage();
                surfaceTexture.getTransformMatrix(texMatrix);
                frameAvailable = false;
            }
        }

        onDrawFrame(texMatrix);
    }

    @Override
    public void onSurfaceChanged(GL10 p0, int width, int height) {
        onSurfaceChanged(width, height);
    }

    @Override
    public void onSurfaceCreated(GL10 p0, EGLConfig p1) {
        Log.d("com.example.cppcamera", "hrlqq");
        // Prepare texture and surface
        int[] textures = new int[1];
        GLES30.glGenTextures(1, textures, 0);
        GLES30.glBindTexture(GL_TEXTURE_EXTERNAL_OES, textures[0]);


        surfaceTexture = new SurfaceTexture(textures[0]);
        surfaceTexture.setOnFrameAvailableListener(this);


        Surface surface = new Surface(surfaceTexture);

//        onSurfaceCreated(textures[0], surface);
        glStartPreview(textures[0], surface);
    }


}
