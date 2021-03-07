package com.example.cppcamera;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;

public class CamSurfaceView extends GLSurfaceView {

    CamRenderer camRenderer;
    public CamSurfaceView(Context context) {
        super(context);
    }


    public CamSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setEGLContextClientVersion(2);
        camRenderer = new CamRenderer();
        setRenderer(camRenderer);
    }

}