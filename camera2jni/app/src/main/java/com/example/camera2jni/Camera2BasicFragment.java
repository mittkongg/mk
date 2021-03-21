package com.example.camera2jni;


import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.app.Fragment;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.ImageFormat;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.opengl.EGL14;
import android.opengl.EGLContext;
import android.opengl.EGLConfig;
import android.opengl.EGLSurface;
import android.opengl.EGLDisplay;
import android.opengl.EGLExt;
import android.opengl.GLES11Ext;
import android.opengl.GLES31;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.os.SystemClock;
import android.util.Log;
import android.util.Size;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.RequiresApi;
import androidx.core.app.ActivityCompat;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

import static android.opengl.EGL14.EGL_NO_CONTEXT;
import static android.opengl.GLES10.glFinish;
import static android.opengl.GLES11.GL_BUFFER_SIZE;
import static android.opengl.GLES20.GL_ARRAY_BUFFER;
import static android.opengl.GLES20.glBindBuffer;
import static android.opengl.GLES20.glGetError;
import static android.opengl.GLES30.GL_MAP_READ_BIT;
import static android.opengl.GLES30.GL_STREAM_COPY;
import static android.opengl.GLES30.glMapBufferRange;
import static android.opengl.GLES30.glUnmapBuffer;
import static android.opengl.GLES31.GL_COMPUTE_SHADER;
import static android.opengl.GLES31.GL_READ_ONLY;
import static android.opengl.GLES31.GL_SHADER_STORAGE_BUFFER;


public class Camera2BasicFragment extends Fragment implements View.OnClickListener {

    /**
     * Conversion from screen rotation to JPEG orientation.
     */
    static {
        System.loadLibrary("native-camera2-jni");
    }
    public static native void readSsbo(int texId, int ssboId);
    public static native void writeSsbo(int texId, int ssboId);

    private static final SparseIntArray ORIENTATIONS = new SparseIntArray();

    static {
        ORIENTATIONS.append(Surface.ROTATION_0, 90);
        ORIENTATIONS.append(Surface.ROTATION_90, 0);
        ORIENTATIONS.append(Surface.ROTATION_180, 270);
        ORIENTATIONS.append(Surface.ROTATION_270, 180);
    }
    public EGLDisplay eglDisplay;
    public EGLDisplay gpuDisplay;
    public EGLConfig eglConfig;
    public EGLConfig gpuConfig;
    public EGLSurface eglSurface;
    public EGLSurface gpuSurface;
    public EGLContext eglContext = null;
    public EGLContext gpuContext = null;
    public boolean once = true;
    // test output view
    private AutoFitTextureView glesView;
    int camTexId = 0;
    int camSsboId = 0;
    int camToSsboProgId = 0;
    public SurfaceTexture camSurfTex = null;
    private final Object lock = new Object();
    private boolean runClassifier = false;
    /**
     * Tag for the {@link Log}.
     */
    private static final String TAG = "Camera2BasicFragment";

    /**
     * Camera state: Showing camera preview.
     */
    private static final int STATE_PREVIEW = 0;

    /**
     * Camera state: Waiting for the focus to be locked.
     */
    private static final int STATE_WAITING_LOCK = 1;
    /**
     * Camera state: Waiting for the exposure to be precapture state.
     */
    private static final int STATE_WAITING_PRECAPTURE = 2;
    /**
     * Camera state: Waiting for the exposure state to be something other than precapture.
     */
    private static final int STATE_WAITING_NON_PRECAPTURE = 3;
    /**
     * Camera state: Picture was taken.
     */
    private static final int STATE_PICTURE_TAKEN = 4;

    /**
     * {@link TextureView.SurfaceTextureListener} handles several lifecycle events on a
     * {@link TextureView}.
     */
    private final TextureView.SurfaceTextureListener mSurfaceTextureListener
            = new TextureView.SurfaceTextureListener() {

        @Override
        public void onSurfaceTextureAvailable(SurfaceTexture texture, int width, int height) {
            openCamera(width, height);
        }

        @Override
        public void onSurfaceTextureSizeChanged(SurfaceTexture texture, int width, int height) {
            configureTransform(width, height);
        }

        @Override
        public boolean onSurfaceTextureDestroyed(SurfaceTexture texture) {
            return true;
        }

        @Override
        public void onSurfaceTextureUpdated(SurfaceTexture texture) {
        }

    };

    /**
     * ID of the current {@link CameraDevice}.
     */
    private String mCameraId;

    /**
     * An {@link AutoFitTextureView} for camera preview.
     */
    private AutoFitTextureView mTextureView;

    /**
     * A {@link CameraCaptureSession } for camera preview.
     */

    private CameraCaptureSession mCaptureSession;
    /**
     * A reference to the opened {@link CameraDevice}.
     */

    private CameraDevice mCameraDevice;
    /**
     * The {@link android.util.Size} of camera preview.
     */

    private Size mPreviewSize;

    /**
     * {@link CameraDevice.StateCallback} is called when {@link CameraDevice} changes its state.
     */
    private final CameraDevice.StateCallback mStateCallback = new CameraDevice.StateCallback() {

        @RequiresApi(api = Build.VERSION_CODES.O)
        @Override
        public void onOpened(CameraDevice cameraDevice) {
            // This method is called when the camera is opened.  We start camera preview here.
            mCameraOpenCloseLock.release();
            mCameraDevice = cameraDevice;
            createCameraPreviewSession();
        }

        @Override
        public void onDisconnected(CameraDevice cameraDevice) {
            mCameraOpenCloseLock.release();
            cameraDevice.close();
            mCameraDevice = null;
        }

        @Override
        public void onError(CameraDevice cameraDevice, int error) {
            mCameraOpenCloseLock.release();
            cameraDevice.close();
            mCameraDevice = null;
            Activity activity = getActivity();
            if (null != activity) {
                activity.finish();
            }
        }

    };

    /**
     * An additional thread for running tasks that shouldn't block the UI.
     */
    private HandlerThread mBackgroundThread;

    /**
     * A {@link Handler} for running tasks in the background.
     */
    private Handler mBackgroundHandler;

    /**
     * An {@link ImageReader} that handles still image capture.
     */
    private ImageReader mImageReader;

    /**
     * This is the output file for our picture.
     */
    private File mFile;

    /**
     * This a callback object for the {@link ImageReader}. "onImageAvailable" will be called when a
     * still image is ready to be saved.
     */
    private final ImageReader.OnImageAvailableListener mOnImageAvailableListener
            = new ImageReader.OnImageAvailableListener() {

        @Override
        public void onImageAvailable(ImageReader reader) {
            mBackgroundHandler.post(new ImageSaver(reader.acquireNextImage(), mFile));
        }

    };

    /**
     * {@link CaptureRequest.Builder} for the camera preview
     */
    private CaptureRequest.Builder mPreviewRequestBuilder;

    /**
     * {@link CaptureRequest} generated by {@link #mPreviewRequestBuilder}
     */
    private CaptureRequest mPreviewRequest;

    /**
     * The current state of camera state for taking pictures.
     *
     * @see #mCaptureCallback
     */
    private int mState = STATE_PREVIEW;

    /**
     * A {@link Semaphore} to prevent the app from exiting before closing the camera.
     */
    private Semaphore mCameraOpenCloseLock = new Semaphore(1);

    /**
     * A {@link CameraCaptureSession.CaptureCallback} that handles events related to JPEG capture.
     */
    private CameraCaptureSession.CaptureCallback mCaptureCallback
            = new CameraCaptureSession.CaptureCallback() {

        private void process(CaptureResult result) {
            switch (mState) {
                case STATE_PREVIEW: {
                    // We have nothing to do when the camera preview is working normally.
                    break;
                }
                case STATE_WAITING_LOCK: {
                    int afState = result.get(CaptureResult.CONTROL_AF_STATE);
                    if (CaptureResult.CONTROL_AF_STATE_FOCUSED_LOCKED == afState ||
                            CaptureResult.CONTROL_AF_STATE_NOT_FOCUSED_LOCKED == afState) {
                        // CONTROL_AE_STATE can be null on some devices
                        Integer aeState = result.get(CaptureResult.CONTROL_AE_STATE);
                        if (aeState == null ||
                                aeState == CaptureResult.CONTROL_AE_STATE_CONVERGED) {
                            mState = STATE_WAITING_NON_PRECAPTURE;
                            captureStillPicture();
                        } else {
                            runPrecaptureSequence();
                        }
                    }
                    break;
                }
                case STATE_WAITING_PRECAPTURE: {
                    // CONTROL_AE_STATE can be null on some devices
                    Integer aeState = result.get(CaptureResult.CONTROL_AE_STATE);
                    if (aeState == null ||
                            aeState == CaptureResult.CONTROL_AE_STATE_PRECAPTURE ||
                            aeState == CaptureRequest.CONTROL_AE_STATE_FLASH_REQUIRED) {
                        mState = STATE_WAITING_NON_PRECAPTURE;
                    }
                    break;
                }
                case STATE_WAITING_NON_PRECAPTURE: {
                    // CONTROL_AE_STATE can be null on some devices
                    Integer aeState = result.get(CaptureResult.CONTROL_AE_STATE);
                    if (aeState == null || aeState != CaptureResult.CONTROL_AE_STATE_PRECAPTURE) {
                        mState = STATE_PICTURE_TAKEN;
                        captureStillPicture();
                    }
                    break;
                }
            }
        }

        @Override
        public void onCaptureProgressed(CameraCaptureSession session, CaptureRequest request,
                                        CaptureResult partialResult) {
            process(partialResult);
        }

        @Override
        public void onCaptureCompleted(CameraCaptureSession session, CaptureRequest request,
                                       TotalCaptureResult result) {
            process(result);
        }

    };

    /**
     * A {@link Handler} for showing {@link Toast}s.
     */
    private Handler mMessageHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            Activity activity = getActivity();
            if (activity != null) {
                Toast.makeText(activity, (String) msg.obj, Toast.LENGTH_SHORT).show();
            }
        }
    };

    /**
     * Shows a {@link Toast} on the UI thread.
     *
     * @param text The message to show
     */
    private void showToast(String text) {
        // We show a Toast by sending request message to mMessageHandler. This makes sure that the
        // Toast is shown on the UI thread.
        Message message = Message.obtain();
        message.obj = text;
        mMessageHandler.sendMessage(message);
    }

    /**
     * Given {@code choices} of {@code Size}s supported by a camera, chooses the smallest one whose
     * width and height are at least as large as the respective requested values, and whose aspect
     * ratio matches with the specified value.
     *
     * @param choices     The list of sizes that the camera supports for the intended output class
     * @param width       The minimum desired width
     * @param height      The minimum desired height
     * @param aspectRatio The aspect ratio
     * @return The optimal {@code Size}, or an arbitrary one if none were big enough
     */
    private static Size chooseOptimalSize(Size[] choices, int width, int height, Size aspectRatio) {
        // Collect the supported resolutions that are at least as big as the preview Surface
        List<Size> bigEnough = new ArrayList<Size>();
        int w = aspectRatio.getWidth();
        int h = aspectRatio.getHeight();
        for (Size option : choices) {
            if (option.getHeight() == option.getWidth() * h / w &&
                    option.getWidth() >= width && option.getHeight() >= height) {
                bigEnough.add(option);
            }
        }

        // Pick the smallest of those, assuming we found any
        if (bigEnough.size() > 0) {
            return Collections.min(bigEnough, new CompareSizesByArea());
        } else {
            Log.e(TAG, "Couldn't find any suitable preview size");
            return choices[0];
        }
    }

    public static Camera2BasicFragment newInstance() {
        Camera2BasicFragment fragment = new Camera2BasicFragment();
        fragment.setRetainInstance(true);
        return fragment;
    }
    @RequiresApi(api = Build.VERSION_CODES.O)
    public void initGLES (EGLContext[] context, EGLDisplay[] display, EGLConfig[] config, EGLSurface[] surface, EGLContext sharedCntx){

        EGLDisplay disp = null;
        EGLContext cntx = null;
        EGLConfig  cfig = null;
        EGLSurface surf = null;

        // egl init
        disp = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        if (disp == null) {
            throw new RuntimeException("unable to get EGL14 display");
        }

        int[] vers = new int[2];
        if (!EGL14.eglInitialize(disp, vers, 0, vers, 1)) {
            throw new RuntimeException("unable to initialize EGL14 display");
        }

        int[] configAttr = {
                EGL14.EGL_RED_SIZE, 8,
                EGL14.EGL_GREEN_SIZE, 8,
                EGL14.EGL_BLUE_SIZE, 8,
                EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT | EGLExt.EGL_OPENGL_ES3_BIT_KHR,
                EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
                EGL14.EGL_NONE
        };

        EGLConfig[] configs = new EGLConfig[1];
        int[] numConfig = new int[1];
        EGL14.eglChooseConfig(disp, configAttr, 0,
                configs, 0, 1, numConfig, 0);
        if (numConfig[0] == 0) {
            throw new RuntimeException("unable to choose config for EGL14 display");
        }
        cfig = configs[0];

        int[] ctxAttrib = {
                EGL14.EGL_CONTEXT_CLIENT_VERSION, 3,
                EGL14.EGL_NONE
        };

        // create egl context
        cntx = EGL14.eglCreateContext(disp, cfig, sharedCntx, ctxAttrib, 0);  // needs a shared context for a shared ssbo.
        if (cntx.equals(EGL_NO_CONTEXT)) {
            throw new RuntimeException("unable to create EGL14 context");
        }
        //Log.i(TAG, "Camera2BasicFragment - created egl context");


        context[0] = cntx;
        display[0] = disp;
        config [0] = cfig;

        if (surface != null) {
            // create a dummy surface as no on-screen drawing is needed
            SurfaceTexture dummySurf = new SurfaceTexture(true);

            // the surface is to display the output to the screen
            int[] dummySurfAttrib = {
                    EGL14.EGL_NONE
            };
            surf = EGL14.eglCreateWindowSurface(disp, cfig, dummySurf, dummySurfAttrib, 0);

            surface[0] = surf;
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.O)
    public void initSsbo(SurfaceTexture winObj) {
        // add an off-screen surface target, so pixels can be retrieved from cam preview frames.
        int preview_cx = mPreviewSize.getWidth();
        int preview_cy = mPreviewSize.getHeight();

        // init GLES
        EGLDisplay[] display = new EGLDisplay[1];
        EGLContext[] context = new EGLContext[1];
        EGLConfig [] config  = new EGLConfig[1];
        EGLSurface[] surface = new EGLSurface[1];

        initGLES(context, display, config, null, EGL_NO_CONTEXT);
        eglDisplay = display[0];
        eglContext = context[0];
        eglConfig  = config [0];
        //eglSurface = surface[0];

        // create a real on-display surface that associates GLES
        int[] eglSurfaceAttribs = {
                EGL14.EGL_NONE
        };

        // On Pixel 3 (Android P, Snapdragon 845), a real on-display surface needs to be associated to make
        //      compute shader works.
        // On LG G4 (Android M, Snapdragon 808), however, only a dummy surface is needed.
        // For Pixel 3 and LG G4, a real on-display surface works on both devices.
//        SurfaceTexture outputSurf = glesView.getSurfaceTexture();
        eglSurface = EGL14.eglCreateWindowSurface(eglDisplay, eglConfig, winObj, eglSurfaceAttribs, 0);

        // make the context current for current thread, display and surface.
        EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

        // TODO: remove hard code.
        int img_cx = 224;
        int img_cy = 224;

        //==========  cam -> tex -> ssbo  ==========
        // create a texture name
        camTexId = createTextureName();
        camSsboId = createSSBO(img_cx, img_cy);
        camToSsboProgId = createShaderProgram_TexToSsbo(preview_cx, preview_cy, img_cx, img_cy);

    }
    // create a texture name
    private int createTextureName () {
        // create texture name
        int[] texIds = new int[1];
        GLES31.glGenTextures(texIds.length, texIds, 0);

        if (texIds[0] == 0) {
            throw new RuntimeException("cannot create texture name.");
        }

        int texId = texIds[0];
        // GLES31.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, texIds[0]);
        Log.d(TAG, "createTextureName: texId :" + texId);
        return texId;
    }

    // create an SSBO buffer object and returns the ID name
    // default to 3 channels and float size = 4.
    private int createSSBO (int cx, int cy) {
        int[] ssboIds = new int[1];
        GLES31.glGenBuffers(ssboIds.length, ssboIds, 0);

        int ssboId = ssboIds[0];
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboId);

        if (ssboId == 0) {
            throw new RuntimeException("cannot create SSBO.");
        }

        ByteBuffer ssboData = null;

        int PIXEL_SIZE = 3;
        int FLOAT_BYTE_SIZE = 4;
        int ssboSize = cx * cy * PIXEL_SIZE * FLOAT_BYTE_SIZE;


        GLES31.glBufferData(GL_SHADER_STORAGE_BUFFER, ssboSize, ssboData, GL_STREAM_COPY);

        int ssboSizeCreated [] = new int[1];
        GLES31.glGetBufferParameteriv (GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, ssboSizeCreated, 0);
        if (ssboSizeCreated[0] != ssboSize) {
            throw new RuntimeException("cannot create SSBO with needed size.");
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);  // unbind output ssbo
        Log.d(TAG, "createSSBO ssbo id: " + ssboId);
        return ssboId;
    }

    // image size should be always smaller than camera size
    //  cam_cx >= img_cy
    //  cam_cy >= img_cy
    private int createShaderProgram_TexToSsbo (int cam_cx, int cam_cy, int img_cx, int img_cy) {
        // create ssbo --> texture shader program
        String shaderCode =
                "   #version 310 es\n" +
                        "   #extension GL_OES_EGL_image_external_essl3: enable\n" +
                        "   precision mediump float;\n" + // need to specify 'mediump' for float
                        //"   layout(local_size_x = 16, local_size_y = 16) in;\n" +
                        "   layout(local_size_x = 8, local_size_y = 8) in;\n" +
                        //"   layout(binding = 0) uniform sampler2D in_data; \n" +
                        "   layout(binding = 0) uniform samplerExternalOES in_data; \n" +
                        "   layout(std430) buffer;\n" +
                        "   layout(binding = 1) buffer Input { float elements[]; } out_data;\n" +
                        "   void main() {\n" +
                        "     ivec2 gid = ivec2(gl_GlobalInvocationID.xy);\n" +
                        "     if (gid.x >= " + img_cx + " || gid.y >= " + img_cy + ") return;\n" +
                        "     vec2 uv = vec2(gl_GlobalInvocationID.xy) / " + img_cx + ".0;\n" +
                        "     vec4 pixel = texture (in_data, uv);\n" +
                        "     int idx = 3 * (gid.y * " + img_cx + " + gid.x);\n" +
                        //"     if (gid.x >= 120) pixel.x = 1.0;\n" + // DEBUG...
                        "     out_data.elements[idx + 0] = 1.0;\n" +
                        "     out_data.elements[idx + 1] = pixel.y;\n" +
                        "     out_data.elements[idx + 2] = pixel.z;\n" +
                        "   }";

        int shader = GLES31.glCreateShader(GL_COMPUTE_SHADER);
        GLES31.glShaderSource(shader, shaderCode);
        GLES31.glCompileShader(shader);

        int[] compiled = new int [1];
        GLES31.glGetShaderiv(shader, GLES31.GL_COMPILE_STATUS, compiled, 0);
        if (compiled[0] == 0) {
            // shader compilation failed
            String log = "shader - compilation error: " + GLES31.glGetShaderInfoLog(shader);

            Log.i(TAG, log);
            throw new RuntimeException(log);
        }

        int progId = GLES31.glCreateProgram();
        if (progId == 0) {
            String log = "shader - cannot create program";

            Log.i(TAG, log);
            throw new RuntimeException(log);
        }

        GLES31.glAttachShader(progId, shader);
        GLES31.glLinkProgram (progId);

        int[] linked = new int[1];
        GLES31.glGetProgramiv(progId, GLES31.GL_LINK_STATUS, linked, 0);
        if (linked[0] == 0) {
            String log = "shader - link error - log: " + GLES31.glGetProgramInfoLog(progId);

            Log.i(TAG, log);
            throw new RuntimeException(log);
        }

        return progId;
    }
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_camera2_basic, container, false);
    }

    @Override
    public void onViewCreated(final View view, Bundle savedInstanceState) {
        view.findViewById(R.id.picture).setOnClickListener(this);
        view.findViewById(R.id.info).setOnClickListener(this);
        // a real on-display view
        glesView = (AutoFitTextureView) view.findViewById(R.id.gles);
        mTextureView = (AutoFitTextureView) view.findViewById(R.id.texture);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mFile = new File(getActivity().getExternalFilesDir(null), "pic.jpg");
    }

    @RequiresApi(api = Build.VERSION_CODES.O)
    @Override
    public void onResume() {
        super.onResume();
        startBackgroundThread();

        // When the screen is turned off and turned back on, the SurfaceTexture is already
        // available, and "onSurfaceTextureAvailable" will not be called. In that case, we can open
        // a camera and start preview from here (otherwise, we wait until the surface is ready in
        // the SurfaceTextureListener).
        if (mTextureView.isAvailable()) {
            openCamera(mTextureView.getWidth(), mTextureView.getHeight());
        } else {
            mTextureView.setSurfaceTextureListener(mSurfaceTextureListener);
        }
    }

    @Override
    public void onPause() {
        closeCamera();
        stopBackgroundThread();
        super.onPause();
    }

    /**
     * Sets up member variables related to camera.
     *
     * @param width  The width of available size for camera preview
     * @param height The height of available size for camera preview
     */
    private void setUpCameraOutputs(int width, int height) {
        Activity activity = getActivity();
        CameraManager manager = (CameraManager) activity.getSystemService(Context.CAMERA_SERVICE);
        try {
            for (String cameraId : manager.getCameraIdList()) {
                CameraCharacteristics characteristics
                        = manager.getCameraCharacteristics(cameraId);

                // We don't use a front facing camera in this sample.
                if (characteristics.get(CameraCharacteristics.LENS_FACING)
                        == CameraCharacteristics.LENS_FACING_FRONT) {
                    continue;
                }

                StreamConfigurationMap map = characteristics.get(
                        CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);

                // For still image captures, we use the largest available size.
                Size largest = Collections.max(
                        Arrays.asList(map.getOutputSizes(ImageFormat.JPEG)),
                        new CompareSizesByArea());
                mImageReader = ImageReader.newInstance(largest.getWidth(), largest.getHeight(),
                        ImageFormat.JPEG, /*maxImages*/2);
                mImageReader.setOnImageAvailableListener(
                        mOnImageAvailableListener, mBackgroundHandler);

                // Danger, W.R.! Attempting to use too large a preview size could  exceed the camera
                // bus' bandwidth limitation, resulting in gorgeous previews but the storage of
                // garbage capture data.
                mPreviewSize = chooseOptimalSize(map.getOutputSizes(SurfaceTexture.class),
                        width, height, largest);

                // We fit the aspect ratio of TextureView to the size of preview we picked.
                int orientation = getResources().getConfiguration().orientation;
                if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
                    mTextureView.setAspectRatio(
                            mPreviewSize.getWidth(), mPreviewSize.getHeight());
                } else {
                    mTextureView.setAspectRatio(
                            mPreviewSize.getHeight(), mPreviewSize.getWidth());
                }

                mCameraId = cameraId;
                return;
            }
        } catch (CameraAccessException e) {
            e.printStackTrace();
        } catch (NullPointerException e) {
            // Currently an NPE is thrown when the Camera2API is used but not supported on the
            // device this code runs.
            new ErrorDialog().show(getFragmentManager(), "dialog");
        }
    }

    /**
     * Opens the camera specified by {@link Camera2BasicFragment#mCameraId}.
     */
    private void openCamera(int width, int height) {
        setUpCameraOutputs(width, height);
        configureTransform(width, height);
        Activity activity = getActivity();
        CameraManager manager = (CameraManager) activity.getSystemService(Context.CAMERA_SERVICE);
        try {
            if (!mCameraOpenCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS)) {
                throw new RuntimeException("Time out waiting to lock camera opening.");
            }
            if (ActivityCompat.checkSelfPermission(this.getContext(), Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
                // TODO: Consider calling
                //    ActivityCompat#requestPermissions
                // here to request the missing permissions, and then overriding
                //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
                //                                          int[] grantResults)
                // to handle the case where the user grants the permission. See the documentation
                // for ActivityCompat#requestPermissions for more details.
                Camera2BasicFragment.this.requestPermissions(new String[]{Manifest.permission.CAMERA},
                        1);

                return;
            }
            manager.openCamera(mCameraId, mStateCallback, mBackgroundHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        } catch (InterruptedException e) {
            throw new RuntimeException("Interrupted while trying to lock camera opening.", e);
        }
    }

    /**
     * Closes the current {@link CameraDevice}.
     */
    private void closeCamera() {
        try {
            mCameraOpenCloseLock.acquire();
            if (null != mCaptureSession) {
                mCaptureSession.close();
                mCaptureSession = null;
            }
            if (null != mCameraDevice) {
                mCameraDevice.close();
                mCameraDevice = null;
            }
            if (null != mImageReader) {
                mImageReader.close();
                mImageReader = null;
            }
        } catch (InterruptedException e) {
            throw new RuntimeException("Interrupted while trying to lock camera closing.", e);
        } finally {
            mCameraOpenCloseLock.release();
        }
    }
    // classify the frame using SSBO
    private void classifyFrameSSBO() {
        if (getActivity() == null || mCameraDevice == null || eglContext == null) {
            // It's important to not call showToast every frame, or else the app will starve and
            // hang. updateActiveModel() already puts a error message up with showToast.
            // showToast("Uninitialized Classifier or invalid context.");
            return;
        }


        camSurfTex.updateTexImage();
        EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
//        Log.d(TAG, "classifyFrameSSBO: updateTexImage");
        // set gles context

        // copy the surface to texture
        long copy_t0 = SystemClock.uptimeMillis();
        if (once) {
            once = false;
            writeSsbo(camTexId, camSsboId);
//            copyCamTexToSsbo();
        }
        long copy_t1 = SystemClock.uptimeMillis();
        glFinish();
//        readSsbo(camTexId, camSsboId);
//        // classify the frame in the SSBO
        //EGL14.eglMakeCurrent(gpuDisplay, gpuSurface, gpuSurface, gpuContext);
//        classifier.classifyFrameSSBO(textToShow, copy_t1 - copy_t0);
//        writeSsbo(camTexId, camSsboId);

        // resumes the normal egl context
        //EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

    }
    // copies camera surface texture to ssbo
    void copyCamTexToSsbo () {
        // bind camera input texture to GL_TEXTURE_EXTERNAL_OES.

        // only copy up to classifier image size
        int img_cx = 256;
        int img_cy = 256;

        int FLOAT_BYTE_SIZE = 4;
        int camSsboSize = img_cx * img_cy * 3 * FLOAT_BYTE_SIZE;  // input ssbo to tflite gpu delegate has 3 channels

        // updateTexImage() binds the texture to GL_TEXTURE_EXTERNAL_OES
        GLES31.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, camTexId);
        GLES31.glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, camSsboId, 0, camSsboSize);

        GLES31.glUseProgram(camToSsboProgId);
        //GLES31.glDispatchCompute(img_cx / 16, img_cy / 16, 1);  // these are work group sizes
        GLES31.glDispatchCompute(img_cx / 8, img_cy / 8, 1);  // smaller work group sizes for lower end GPU.

        GLES31.glMemoryBarrier(GLES31.GL_SHADER_STORAGE_BARRIER_BIT);

        readSsbo(camTexId, camSsboId);
        GLES31.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, camTexId);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, camSsboId);
        ByteBuffer data = (ByteBuffer) glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 256*256*3, GL_MAP_READ_BIT );
        if (data != null) {
           // Log.d("write ssbo data: %d %d %d %d %d %d", Arrays.toString(data.array()));
        } else {
            Log.d(TAG, " gl error" + glGetError());
        }
//        glUnmapBuffer( GL_SHADER_STORAGE_BUFFER );
//        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);  // unbind

        GLES31.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);  // unbind
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);  // unbind


    }
    /** Takes photos and classify them periodically. */
    private Runnable periodicClassify =
            new Runnable() {
                @Override
                public void run() {
                    synchronized (lock) {
                        if (runClassifier) {

                                // wait until next frame in the SurfaceTexture frame listener in onFrameAvailable()
                                classifyFrameSSBO();
                                runClassifier = false;

                            }
                        }
                        mBackgroundHandler.post(periodicClassify);
                    }
            };
    /**
     * Starts a background thread and its {@link Handler}.
     */
    @RequiresApi(api = Build.VERSION_CODES.O)
    private void updateActiveModel() {
        // Get UI information before delegating to background


        mBackgroundHandler.post(() -> {

                    if (eglContext == null) {

                        return;
                    }

                    if (gpuContext == null) {
                        //classifier.useGpu();

                        // useGpu() calls modifyGraphWithDelegate(), which changes the current gles context.
                        //     this makes current context no longer available to current thread.
                        //     one way to work around this is to create a new context and share the previously
                        //     created context.
                        EGLDisplay[] display = new EGLDisplay[1];
                        EGLContext[] context = new EGLContext[1];
                        EGLConfig[] config = new EGLConfig[1];
                        EGLSurface[] surface = new EGLSurface[1];

                        initGLES(context, display, config, surface, eglContext);
                        gpuDisplay = display[0];
                        gpuContext = context[0];
                        gpuConfig = config[0];
                        gpuSurface = surface[0];
                    }

                    // make the gpu context current before calling useGpu(), which in turn calls
                    //      modifyGraphWithDelegate()
//                    EGL14.eglMakeCurrent(gpuDisplay, gpuSurface, gpuSurface, gpuContext);

                    writeSsbo(camTexId, camSsboId);


                    // resumes normal egl context
                    EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

        });
    }
    @RequiresApi(api = Build.VERSION_CODES.O)
    private void startBackgroundThread() {
        mBackgroundThread = new HandlerThread("CameraBackground");
        mBackgroundThread.start();
        mBackgroundHandler = new Handler(mBackgroundThread.getLooper());
        synchronized (lock) {
            //runClassifier = true;
            // only run when there is a frame available
            runClassifier = false;
        }
        mBackgroundHandler.post(periodicClassify);
        updateActiveModel();
    }

    /**
     * Stops the background thread and its {@link Handler}.
     */
    private void stopBackgroundThread() {
        mBackgroundThread.quitSafely();
        try {
            mBackgroundThread.join();
            mBackgroundThread = null;
            mBackgroundHandler = null;
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    /**
     * Creates a new {@link CameraCaptureSession} for camera preview.
     */
    @RequiresApi(api = Build.VERSION_CODES.O)
    private void createCameraPreviewSession() {
        try {
            SurfaceTexture texture = mTextureView.getSurfaceTexture();
            assert texture != null;

            // We configure the size of default buffer to be the size of camera preview we want.
            texture.setDefaultBufferSize(mPreviewSize.getWidth(), mPreviewSize.getHeight());

            // This is the output Surface we need to start preview.
            Surface surface = new Surface(texture);

            // We set up a CaptureRequest.Builder with the output Surface.
            mPreviewRequestBuilder
                    = mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            mPreviewRequestBuilder.addTarget(surface);

            initSsbo(glesView.getSurfaceTexture());
            Log.d(TAG, "createCameraPreviewSession: initSsbo");
            // create a new surface texture to get the camera frame by using GLES texture name
            int preview_cx = mPreviewSize.getWidth();
            int preview_cy = mPreviewSize.getHeight();

            camSurfTex = new SurfaceTexture(camTexId);
            camSurfTex.setDefaultBufferSize(preview_cx, preview_cy);

            // create a camera surface target
            Surface camSurf = new Surface (camSurfTex);
            SurfaceTexture.OnFrameAvailableListener camFrameListener = new SurfaceTexture.OnFrameAvailableListener() {
                @Override
                public void onFrameAvailable(SurfaceTexture surfaceTexture) {
                    //camSurfTex.updateTexImage();

                    synchronized(lock) {
                        runClassifier = true;
                    }
                    //if (classifier != null && camSsboId != 0 && classifier.getOutputSsboId() != 0) {
                    //  long startTime = SystemClock.uptimeMillis();
                    //  classifier.displayOutputSsboToTextureView();
                    //  long endTime = SystemClock.uptimeMillis();
                    //  Log.d(TAG, "Time cost to display output SSBO: " + Long.toString(endTime - startTime));
                    //  Log.d(TAG, "------------------------------------------------------------");
                    //}
                }
            };

            camSurfTex.setOnFrameAvailableListener(camFrameListener);

            mPreviewRequestBuilder.addTarget(camSurf);

            // Here, we create a CameraCaptureSession for camera preview.
            mCameraDevice.createCaptureSession(Arrays.asList(surface, camSurf, mImageReader.getSurface()),
                    new CameraCaptureSession.StateCallback() {

                        @Override
                        public void onConfigured(CameraCaptureSession cameraCaptureSession) {
                            // The camera is already closed
                            if (null == mCameraDevice) {
                                return;
                            }

                            // When the session is ready, we start displaying the preview.
                            mCaptureSession = cameraCaptureSession;
                            try {
                                // Auto focus should be continuous for camera preview.
                                mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AF_MODE,
                                        CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);
                                // Flash is automatically enabled when necessary.
//                                mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AE_MODE,
//                                        CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH);

                                // Finally, we start displaying the camera preview.
                                mPreviewRequest = mPreviewRequestBuilder.build();
                                mCaptureSession.setRepeatingRequest(mPreviewRequest,
                                        mCaptureCallback, mBackgroundHandler);
                            } catch (CameraAccessException e) {
                                e.printStackTrace();
                            }
                        }

                        @Override
                        public void onConfigureFailed(CameraCaptureSession cameraCaptureSession) {
                            showToast("Failed");
                        }
                    }, null
            );
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    /**
     * Configures the necessary {@link android.graphics.Matrix} transformation to `mTextureView`.
     * This method should be called after the camera preview size is determined in
     * setUpCameraOutputs and also the size of `mTextureView` is fixed.
     *
     * @param viewWidth  The width of `mTextureView`
     * @param viewHeight The height of `mTextureView`
     */
    private void configureTransform(int viewWidth, int viewHeight) {
        Activity activity = getActivity();
        if (null == mTextureView || null == mPreviewSize || null == activity) {
            return;
        }
        int rotation = activity.getWindowManager().getDefaultDisplay().getRotation();
        Matrix matrix = new Matrix();
        RectF viewRect = new RectF(0, 0, viewWidth, viewHeight);
        RectF bufferRect = new RectF(0, 0, mPreviewSize.getHeight(), mPreviewSize.getWidth());
        float centerX = viewRect.centerX();
        float centerY = viewRect.centerY();
        if (Surface.ROTATION_90 == rotation || Surface.ROTATION_270 == rotation) {
            bufferRect.offset(centerX - bufferRect.centerX(), centerY - bufferRect.centerY());
            matrix.setRectToRect(viewRect, bufferRect, Matrix.ScaleToFit.FILL);
            float scale = Math.max(
                    (float) viewHeight / mPreviewSize.getHeight(),
                    (float) viewWidth / mPreviewSize.getWidth());
            matrix.postScale(scale, scale, centerX, centerY);
            matrix.postRotate(90 * (rotation - 2), centerX, centerY);
        }
        mTextureView.setTransform(matrix);
    }

    /**
     * Initiate a still image capture.
     */
    @RequiresApi(api = Build.VERSION_CODES.O)
    private void takePicture() {
        updateActiveModel();
        lockFocus();
    }

    /**
     * Lock the focus as the first step for a still image capture.
     */
    private void lockFocus() {
        try {
            // This is how to tell the camera to lock focus.
            mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AF_TRIGGER,
                    CameraMetadata.CONTROL_AF_TRIGGER_START);
            // Tell #mCaptureCallback to wait for the lock.
            mState = STATE_WAITING_LOCK;
            mCaptureSession.setRepeatingRequest(mPreviewRequestBuilder.build(), mCaptureCallback,
                    mBackgroundHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    /**
     * Run the precapture sequence for capturing a still image. This method should be called when we
     * get a response in {@link #mCaptureCallback} from {@link #lockFocus()}.
     */
    private void runPrecaptureSequence() {
        try {
            // This is how to tell the camera to trigger.
            mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                    CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_START);
            // Tell #mCaptureCallback to wait for the precapture sequence to be set.
            mState = STATE_WAITING_PRECAPTURE;
            mCaptureSession.capture(mPreviewRequestBuilder.build(), mCaptureCallback,
                    mBackgroundHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    /**
     * Capture a still picture. This method should be called when we get a response in
     * {@link #mCaptureCallback} from both {@link #lockFocus()}.
     */
    private void captureStillPicture() {
        try {
            final Activity activity = getActivity();
            if (null == activity || null == mCameraDevice) {
                return;
            }
            // This is the CaptureRequest.Builder that we use to take a picture.
            final CaptureRequest.Builder captureBuilder =
                    mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE);
            captureBuilder.addTarget(mImageReader.getSurface());

            // Use the same AE and AF modes as the preview.
            captureBuilder.set(CaptureRequest.CONTROL_AF_MODE,
                    CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);
            captureBuilder.set(CaptureRequest.CONTROL_AE_MODE,
                    CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH);

            // Orientation
            int rotation = activity.getWindowManager().getDefaultDisplay().getRotation();
            captureBuilder.set(CaptureRequest.JPEG_ORIENTATION, ORIENTATIONS.get(rotation));

            CameraCaptureSession.CaptureCallback CaptureCallback
                    = new CameraCaptureSession.CaptureCallback() {

                @Override
                public void onCaptureCompleted(CameraCaptureSession session, CaptureRequest request,
                                               TotalCaptureResult result) {
                    showToast("Saved: " + mFile);
                    unlockFocus();
                }
            };

            mCaptureSession.stopRepeating();
            mCaptureSession.capture(captureBuilder.build(), CaptureCallback, null);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    /**
     * Unlock the focus. This method should be called when still image capture sequence is finished.
     */
    private void unlockFocus() {
        try {
            // Reset the autofucos trigger
            mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AF_TRIGGER,
                    CameraMetadata.CONTROL_AF_TRIGGER_CANCEL);
            mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AE_MODE,
                    CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH);
            mCaptureSession.capture(mPreviewRequestBuilder.build(), mCaptureCallback,
                    mBackgroundHandler);
            // After this, the camera will go back to the normal state of preview.
            mState = STATE_PREVIEW;
            mCaptureSession.setRepeatingRequest(mPreviewRequest, mCaptureCallback,
                    mBackgroundHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.O)
    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.picture: {
                takePicture();
                break;
            }
            case R.id.info: {
                Activity activity = getActivity();
                if (null != activity) {
                    new AlertDialog.Builder(activity)
                            .setMessage(R.string.intro_message)
                            .setPositiveButton(android.R.string.ok, null)
                            .show();
                }
                break;
            }
        }
    }

    /**
     * Saves a JPEG {@link Image} into the specified {@link File}.
     */
    private static class ImageSaver implements Runnable {

        /**
         * The JPEG image
         */
        private final Image mImage;
        /**
         * The file we save the image into.
         */
        private final File mFile;

        public ImageSaver(Image image, File file) {
            mImage = image;
            mFile = file;
        }

        @Override
        public void run() {
            ByteBuffer buffer = mImage.getPlanes()[0].getBuffer();
            byte[] bytes = new byte[buffer.remaining()];
            buffer.get(bytes);
            FileOutputStream output = null;
            try {
                output = new FileOutputStream(mFile);
                output.write(bytes);
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            } catch (IOException e) {
                e.printStackTrace();
            } finally {
                mImage.close();
                if (null != output) {
                    try {
                        output.close();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }
        }

    }

    /**
     * Compares two {@code Size}s based on their areas.
     */
    static class CompareSizesByArea implements Comparator<Size> {

        @Override
        public int compare(Size lhs, Size rhs) {
            // We cast here to ensure the multiplications won't overflow
            return Long.signum((long) lhs.getWidth() * lhs.getHeight() -
                    (long) rhs.getWidth() * rhs.getHeight());
        }

    }

    public static class ErrorDialog extends DialogFragment {

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final Activity activity = getActivity();
            return new AlertDialog.Builder(activity)
                    .setMessage("This device doesn't support Camera2 API.")
                    .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialogInterface, int i) {
                            activity.finish();
                        }
                    })
                    .create();
        }

    }

}
