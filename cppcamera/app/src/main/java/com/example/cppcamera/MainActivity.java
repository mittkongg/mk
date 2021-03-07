package com.example.cppcamera;

import android.Manifest;
import android.content.pm.PackageManager;
import android.graphics.SurfaceTexture;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.os.Bundle;

import com.google.android.material.floatingactionbutton.FloatingActionButton;
import com.google.android.material.snackbar.Snackbar;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.util.Log;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

import android.view.Menu;
import android.view.MenuItem;
import android.view.ViewGroup;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import static android.opengl.GLES11Ext.GL_TEXTURE_EXTERNAL_OES;

public class MainActivity extends AppCompatActivity {

    static final String TAG = "NativeCamera2";

    private static final int PERMISSION_REQUEST_CAMERA = 1;

    public static native void startPreview(Surface surface);

    public static native void stopPreview();

    public static native void startExtraView(Surface surface);

    public static native void stopExtraView();



    LayoutInflater extraViewLayoutInflater = null;

    boolean isBurstModeOn = false;

    static {
        System.loadLibrary("native-camera2-jni");
    }

    SurfaceView surfaceView;
    SurfaceHolder surfaceHolder;

    SurfaceView extraView;
    SurfaceHolder extraViewHolder;

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setContentView(R.layout.activity_main);

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {

            ActivityCompat.requestPermissions(
                    this,
                    new String[] { Manifest.permission.CAMERA },
                    PERMISSION_REQUEST_CAMERA);
            return;
        }

//        surfaceView = findViewById(R.id.extraview);
//        surfaceHolder = surfaceView.getHolder();
//
//        surfaceHolder.addCallback(new SurfaceHolder.Callback() {
//            @Override
//            public void surfaceCreated(SurfaceHolder holder) {
//
//                Log.v(TAG, "surface created.");
//                startPreview(holder.getSurface());
//            }
//
//            @Override
//            public void surfaceDestroyed(SurfaceHolder holder) {
//                stopPreview();
//            }
//
//            @Override
//            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
//                Log.v(TAG, "format=" + format + " w/h : (" + width + ", " + height + ")");
//            }
//        });


        extraViewLayoutInflater = LayoutInflater.from(getBaseContext());

        View view = extraViewLayoutInflater.inflate(R.layout.extraviewlayout, null);
        ViewGroup.LayoutParams layoutParamsControl
                = new ViewGroup.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);

        this.addContentView(view, layoutParamsControl);

        extraView = (SurfaceView) findViewById(R.id.extraview);
//        extraView.setVisibility(View.INVISIBLE);

        extraViewHolder = extraView.getHolder();
        extraViewHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
//                startExtraView(extraViewHolder.getSurface());
//                startPreview(extraViewHolder.getSurface());
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {

//                stopExtraView();
//                stopPreview();
            }
        });

//        surfaceView.setOnClickListener(new View.OnClickListener() {
//            @Override
//            public void onClick(View v) {
//                isBurstModeOn = !isBurstModeOn;
//
//                if (isBurstModeOn) {
//                    extraView.setVisibility(View.VISIBLE);
//                } else {
//                    extraView.setVisibility(View.INVISIBLE);
//                }
//            }
//        });
    }

    @Override
    protected void onDestroy() {
        stopPreview();
        super.onDestroy();
    }
}