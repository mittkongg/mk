/*
 * Copyright (C) 2016-2017, Collabora Ltd.
 *   Author: Justin Kim <justin.kim@collabora.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <jni.h>
#include <pthread.h>

#include <android/native_window_jni.h>

#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "log.h"
#include "gl_helper.h"
#include "cam_utils.h"

#include "messages-internal.h"

static ACameraManager* cameraManager = nullptr;
static ANativeWindow *theNativeWindow;
static ACameraDevice *cameraDevice;
static ACaptureRequest *captureRequest;
static ACameraOutputTarget *cameraOutputTarget;
static ACaptureSessionOutput *sessionOutput;
static ACaptureSessionOutputContainer *captureSessionOutputContainer;
static ACameraCaptureSession *captureSession;

static ACameraDevice_StateCallbacks deviceStateCallbacks;
static ACameraCaptureSession_stateCallbacks captureSessionStateCallbacks;


static ACameraOutputTarget* textureTarget = nullptr;

static ACaptureRequest* request = nullptr;

static ANativeWindow* textureWindow = nullptr;

static ACameraCaptureSession* textureSession = nullptr;

static ACaptureSessionOutput* textureOutput = nullptr;

#ifdef WITH_IMAGE_READER
static ANativeWindow* imageWindow = nullptr;

static ACameraOutputTarget* imageTarget = nullptr;

static AImageReader* imageReader = nullptr;

static ACaptureSessionOutput* imageOutput = nullptr;
#endif

static ACaptureSessionOutput* output = nullptr;

static ACaptureSessionOutputContainer* outputs = nullptr;

static GLuint prog;
static GLuint vtxShader;
static GLuint fragShader;

static GLint vtxPosAttrib;
static GLint uvsAttrib;
static GLint mvpMatrix;
static GLint texMatrix;
static GLint texSampler;
static GLint color;
static GLint size;
static GLuint buf[2];
static GLuint textureId;

static int width = 640;
static int height = 480;

static const char* vertexShaderSrc = R"(
        precision highp float;
        attribute vec3 vertexPosition;
        attribute vec2 uvs;
        varying vec2 varUvs;
        uniform mat4 texMatrix;
        uniform mat4 mvp;

        void main()
        {
            varUvs = (texMatrix * vec4(uvs.x, uvs.y, 0, 1.0)).xy;
            gl_Position = mvp * vec4(vertexPosition, 1.0);
        }
)";

static const char* fragmentShaderSrc = R"(
        #extension GL_OES_EGL_image_external : require
        precision mediump float;

        varying vec2 varUvs;
        uniform samplerExternalOES texSampler;
        uniform vec4 color;
        uniform vec2 size;

        void main()
        {
            if (gl_FragCoord.x/size.x < 0.5) {
                gl_FragColor = texture2D(texSampler, varUvs) * color;
            }
            else {
                const float threshold = 1.1;
                vec4 c = texture2D(texSampler, varUvs);
                if (length(c) > threshold) {
                    gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);
                } else {
                    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
                }
            }
        }
)";


static void camera_device_on_disconnected(void *context, ACameraDevice *device) {
    LOGI("Camera(id: %s) is diconnected.\n", ACameraDevice_getId(device));
}

static void camera_device_on_error(void *context, ACameraDevice *device, int error) {
    LOGE("Error(code: %d) on Camera(id: %s).\n", error, ACameraDevice_getId(device));
}

static void capture_session_on_ready(void *context, ACameraCaptureSession *session) {
    LOGI("Session is ready. %p\n", session);
}

static void capture_session_on_active(void *context, ACameraCaptureSession *session) {
    LOGI("Session is activated. %p\n", session);
}

static void capture_session_on_closed(void *context, ACameraCaptureSession *session) {
    LOGI("Session is closed. %p\n", session);
}

extern "C" {
JNIEXPORT void JNICALL Java_com_example_cppcamera_MainActivity_stopPreview(JNIEnv *env,
                                                                                    jclass clazz);
JNIEXPORT void JNICALL Java_com_example_cppcamera_MainActivity_startPreview(JNIEnv *env,
                                                                                     jclass clazz,
                                                                                     jobject surface);
JNIEXPORT void JNICALL Java_com_example_cppcamera_MainActivity_stopExtraView(JNIEnv *env,
                                                                                    jclass clazz);
JNIEXPORT void JNICALL Java_com_example_cppcamera_MainActivity_startExtraView(JNIEnv *env,
                                                                                     jclass clazz,
                                                                                     jobject surface);
JNIEXPORT void JNICALL
Java_com_example_cppcamera_CamRenderer_onSurfaceCreated(JNIEnv *env, jclass clazz, jint texId, jobject surface);

JNIEXPORT void JNICALL
Java_com_example_cppcamera_CamRenderer_onSurfaceChanged(JNIEnv *env, jclass clazz, jint w, jint h);
JNIEXPORT void JNICALL
Java_com_example_cppcamera_CamRenderer_onDrawFrame(JNIEnv *env, jclass clazz, jfloatArray texMatArray);

JNIEXPORT void JNICALL Java_com_example_cppcamera_CamRenderer_glStartPreview(JNIEnv *env,
                                                                               jclass clazz,
                                                                               jint texId,
                                                                               jobject surface);
}



static void openCamera(ACameraDevice_request_template templateId)
{
    ACameraIdList *cameraIdList = NULL;
    ACameraMetadata *cameraMetadata = NULL;

    const char *selectedCameraId = NULL;
    camera_status_t camera_status = ACAMERA_OK;
    ACameraManager *cameraManager = ACameraManager_create();

    camera_status = ACameraManager_getCameraIdList(cameraManager, &cameraIdList);
    if (camera_status != ACAMERA_OK) {
        LOGE("Failed to get camera id list (reason: %d)\n", camera_status);
        return;
    }

    if (cameraIdList->numCameras < 1) {
        LOGE("No camera device detected.\n");
        return;
    }

    selectedCameraId = cameraIdList->cameraIds[0];

    LOGI("Trying to open Camera2 (id: %s, num of camera : %d)\n", selectedCameraId,
         cameraIdList->numCameras);

    camera_status = ACameraManager_getCameraCharacteristics(cameraManager, selectedCameraId,
                                                            &cameraMetadata);

    if (camera_status != ACAMERA_OK) {
        LOGE("Failed to get camera meta data of ID:%s\n", selectedCameraId);
    }

    deviceStateCallbacks.onDisconnected = camera_device_on_disconnected;
    deviceStateCallbacks.onError = camera_device_on_error;

    camera_status = ACameraManager_openCamera(cameraManager, selectedCameraId,
                                              &deviceStateCallbacks, &cameraDevice);

    if (camera_status != ACAMERA_OK) {
        LOGE("Failed to open camera device (id: %s)\n", selectedCameraId);
    }

    camera_status = ACameraDevice_createCaptureRequest(cameraDevice, templateId,
                                                       &captureRequest);

    if (camera_status != ACAMERA_OK) {
        LOGE("Failed to create preview capture request (id: %s)\n", selectedCameraId);
    }

    ACaptureSessionOutputContainer_create(&captureSessionOutputContainer);

    captureSessionStateCallbacks.onReady = capture_session_on_ready;
    captureSessionStateCallbacks.onActive = capture_session_on_active;
    captureSessionStateCallbacks.onClosed = capture_session_on_closed;

    ACameraMetadata_free(cameraMetadata);
    ACameraManager_deleteCameraIdList(cameraIdList);
    ACameraManager_delete(cameraManager);
}

static void closeCamera(void)
{
    camera_status_t camera_status = ACAMERA_OK;

    if (captureRequest != NULL) {
        ACaptureRequest_free(captureRequest);
        captureRequest = NULL;
    }

    if (cameraOutputTarget != NULL) {
        ACameraOutputTarget_free(cameraOutputTarget);
        cameraOutputTarget = NULL;
    }

    if (cameraDevice != NULL) {
        camera_status = ACameraDevice_close(cameraDevice);

        if (camera_status != ACAMERA_OK) {
            LOGE("Failed to close CameraDevice.\n");
        }
        cameraDevice = NULL;
    }

    if (sessionOutput != NULL) {
        ACaptureSessionOutput_free(sessionOutput);
        sessionOutput = NULL;
    }

    if (captureSessionOutputContainer != NULL) {
        ACaptureSessionOutputContainer_free(captureSessionOutputContainer);
        captureSessionOutputContainer = NULL;
    }

    LOGI("Close Camera\n");
}

/**
 * Session state callbacks
 */

static void onSessionActive(void* context, ACameraCaptureSession *session)
{
    LOGD("onSessionActive()");
}

static void onSessionReady(void* context, ACameraCaptureSession *session)
{
    LOGD("onSessionReady()");
}

static void onSessionClosed(void* context, ACameraCaptureSession *session)
{
    LOGD("onSessionClosed()");
}

static ACameraCaptureSession_stateCallbacks sessionStateCallbacks {
        .context = nullptr,
        .onActive = onSessionActive,
        .onReady = onSessionReady,
        .onClosed = onSessionClosed
};

static void onDisconnected(void* context, ACameraDevice* device)
{
    LOGD("onDisconnected");
}

static void onError(void* context, ACameraDevice* device, int error)
{
    LOGD("error %d", error);
}

static ACameraDevice_stateCallbacks cameraDeviceCallbacks = {
        .context = nullptr,
        .onDisconnected = onDisconnected,
        .onError = onError,
};
/**
 * Functions used to set-up the camera and draw
 * camera frames into SurfaceTexture
 */

static void initCam()
{
    cameraManager = ACameraManager_create();

    auto id = getBackFacingCamId(cameraManager);
    ACameraManager_openCamera(cameraManager, id.c_str(), &cameraDeviceCallbacks, &cameraDevice);

    printCamProps(cameraManager, id.c_str());
}

static void exitCam()
{
    if (cameraManager)
    {
        // Stop recording to SurfaceTexture and do some cleanup
        ACameraCaptureSession_stopRepeating(textureSession);
        ACameraCaptureSession_close(textureSession);
        ACaptureSessionOutputContainer_free(outputs);
        ACaptureSessionOutput_free(output);

        ACameraDevice_close(cameraDevice);
        ACameraManager_delete(cameraManager);
        cameraManager = nullptr;

#ifdef WITH_IMAGE_READER
        AImageReader_delete(imageReader);
        imageReader = nullptr;
#endif

        // Capture request for SurfaceTexture
        ANativeWindow_release(textureWindow);
        ACaptureRequest_free(request);
    }
}

/**
 * Capture callbacks
 */

void onCaptureFailed(void* context, ACameraCaptureSession* session,
                     ACaptureRequest* request, ACameraCaptureFailure* failure)
{
    LOGE("onCaptureFailed ");
}

void onCaptureSequenceCompleted(void* context, ACameraCaptureSession* session,
                                int sequenceId, int64_t frameNumber)
{}

void onCaptureSequenceAborted(void* context, ACameraCaptureSession* session,
                              int sequenceId)
{}

void onCaptureCompleted (
        void* context, ACameraCaptureSession* session,
        ACaptureRequest* request, const ACameraMetadata* result)
{
    LOGD("Capture completed");
}

static ACameraCaptureSession_captureCallbacks captureCallbacks {
        .context = nullptr,
        .onCaptureStarted = nullptr,
        .onCaptureProgressed = nullptr,
        .onCaptureCompleted = onCaptureCompleted,
        .onCaptureFailed = onCaptureFailed,
        .onCaptureSequenceCompleted = onCaptureSequenceCompleted,
        .onCaptureSequenceAborted = onCaptureSequenceAborted,
        .onCaptureBufferLost = nullptr,
};
static void initCam(JNIEnv* env, jobject surface)
{
    // Prepare surface
    textureWindow = ANativeWindow_fromSurface(env, surface);

    // Prepare request for texture target
//    ACameraDevice_createCaptureRequest(cameraDevice, TEMPLATE_PREVIEW, &request);

    // Prepare outputs for session
    ACaptureSessionOutput_create(textureWindow, &textureOutput);
    ACaptureSessionOutputContainer_create(&outputs);
    ACaptureSessionOutputContainer_add(outputs, textureOutput);

// Enable ImageReader example in CMakeLists.txt. This will additionally
// make image data available in imageCallback().
#ifdef WITH_IMAGE_READER
    imageReader = createJpegReader();
    imageWindow = createSurface(imageReader);
    ANativeWindow_acquire(imageWindow);
    ACameraOutputTarget_create(imageWindow, &imageTarget);
    ACaptureRequest_addTarget(request, imageTarget);
    ACaptureSessionOutput_create(imageWindow, &imageOutput);
    ACaptureSessionOutputContainer_add(outputs, imageOutput);
#endif

    // Prepare target surface
    ANativeWindow_acquire(textureWindow);
    ACameraOutputTarget_create(textureWindow, &textureTarget);
    ACaptureRequest_addTarget(request, textureTarget);

    // Create the session
    ACameraDevice_createCaptureSession(cameraDevice, outputs, &sessionStateCallbacks, &textureSession);

    // Start capturing continuously
    ACameraCaptureSession_setRepeatingRequest(textureSession, &captureCallbacks, 1, &request, nullptr);
}

static void initSurface(JNIEnv* env, jint texId, jobject surface)
{
    // Init shaders
    vtxShader = createShader(vertexShaderSrc, GL_VERTEX_SHADER);
    fragShader = createShader(fragmentShaderSrc, GL_FRAGMENT_SHADER);
    prog = createProgram(vtxShader, fragShader);

    // Store attribute and uniform locations
    vtxPosAttrib = glGetAttribLocation(prog, "vertexPosition");
    uvsAttrib = glGetAttribLocation(prog, "uvs");
    mvpMatrix = glGetUniformLocation(prog, "mvp");
    texMatrix = glGetUniformLocation(prog, "texMatrix");
    texSampler = glGetUniformLocation(prog, "texSampler");
    color = glGetUniformLocation(prog, "color");
    size = glGetUniformLocation(prog, "size");

    // Prepare buffers
    glGenBuffers(2, buf);

    // Set up vertices
    float vertices[] {
            // x, y, z, u, v
            -1, -1, 0, 0, 0,
            -1, 1, 0, 0, 1,
            1, 1, 0, 1, 1,
            1, -1, 0, 1, 0
    };
    glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    // Set up indices
    GLuint indices[] { 2, 1, 0, 0, 3, 2 };
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);

    // We can use the id to bind to GL_TEXTURE_EXTERNAL_OES
    textureId = texId;

    // Prepare the surfaces/targets & initialize session
//    initCam(env, surface);

}

static void drawFrame(JNIEnv* env, jfloatArray texMatArray)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glClearColor(0, 0, 0, 1);

    glUseProgram(prog);

    // Configure main transformations
    float mvp[] = {
            1.0f, 0, 0, 0,
            0, 1.0f, 0, 0,
            0, 0, 1.0f, 0,
            0, 0, 0, 1.0f
    };

    float aspect = width > height ? float(width)/float(height) : float(height)/float(width);
    if (width < height) // portrait
        ortho(mvp, -1.0f, 1.0f, -aspect, aspect, -1.0f, 1.0f);
    else // landscape
        ortho(mvp, -aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);

    glUniformMatrix4fv(mvpMatrix, 1, false, mvp);


    // Prepare texture for drawing

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Pass SurfaceTexture transformations to shader
    float* tm = env->GetFloatArrayElements(texMatArray, 0);
    glUniformMatrix4fv(texMatrix, 1, false, tm);
    env->ReleaseFloatArrayElements(texMatArray, tm, 0);

    // Set the SurfaceTexture sampler
    glUniform1i(texSampler, 0);

    // I use red color to mix with camera frames
    float c[] = { 1, 0, 0, 1 };
    glUniform4fv(color, 1, (GLfloat*)c);

    // Size of the window is used in fragment shader
    // to split the window
    float sz[2] = {0};
    sz[0] = width;
    sz[1] = height;
    glUniform2fv(size, 1, (GLfloat*)sz);

    // Set up vertices/indices and draw
    glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);

    glEnableVertexAttribArray(vtxPosAttrib);
    glVertexAttribPointer(vtxPosAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)0);

    glEnableVertexAttribArray(uvsAttrib);
    glVertexAttribPointer(uvsAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void *)(3 * sizeof(float)));

    glViewport(0, 0, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

JNIEXPORT void JNICALL Java_com_example_cppcamera_CamRenderer_glStartPreview(JNIEnv *env,
                                                                            jclass clazz,
                                                                            jint texId,
                                                                            jobject surface) {
    theNativeWindow = ANativeWindow_fromSurface(env, surface);
    initSurface(env, texId, surface);

    openCamera(TEMPLATE_PREVIEW);

    LOGI("Surface is prepared in %p.\n", surface);

    ACameraOutputTarget_create(theNativeWindow, &cameraOutputTarget);
    ACaptureRequest_addTarget(captureRequest, cameraOutputTarget);

    ACaptureSessionOutput_create(theNativeWindow, &sessionOutput);
    ACaptureSessionOutputContainer_add(captureSessionOutputContainer, sessionOutput);

    ACameraDevice_createCaptureSession(cameraDevice, captureSessionOutputContainer,
                                       &captureSessionStateCallbacks, &captureSession);

    ACameraCaptureSession_setRepeatingRequest(captureSession, NULL, 1, &captureRequest, NULL);

}

JNIEXPORT void JNICALL Java_com_example_cppcamera_MainActivity_startPreview(JNIEnv *env,
                                                                                     jclass clazz,
                                                                                     jobject surface) {
    theNativeWindow = ANativeWindow_fromSurface(env, surface);

    openCamera(TEMPLATE_PREVIEW);

    LOGI("Surface is prepared in %p.\n", surface);

    ACameraOutputTarget_create(theNativeWindow, &cameraOutputTarget);
    ACaptureRequest_addTarget(captureRequest, cameraOutputTarget);

    ACaptureSessionOutput_create(theNativeWindow, &sessionOutput);
    ACaptureSessionOutputContainer_add(captureSessionOutputContainer, sessionOutput);

    ACameraDevice_createCaptureSession(cameraDevice, captureSessionOutputContainer,
                                       &captureSessionStateCallbacks, &captureSession);

    ACameraCaptureSession_setRepeatingRequest(captureSession, NULL, 1, &captureRequest, NULL);

}

JNIEXPORT void JNICALL Java_com_example_cppcamera_MainActivity_stopPreview(JNIEnv *env,
                                                                                    jclass clazz) {
    closeCamera();
    if (theNativeWindow != NULL) {
        ANativeWindow_release(theNativeWindow);
        theNativeWindow = NULL;
    }
}

static ANativeWindow *extraViewWindow;
static ACaptureRequest *extraViewCaptureRequest;
static ACameraOutputTarget *extraViewOutputTarget;
static ACaptureSessionOutput *extraViewSessionOutput;

JNIEXPORT void JNICALL Java_com_example_cppcamera_MainActivity_startExtraView(JNIEnv *env,
                                                                                       jclass clazz,
                                                                                       jobject surface) {

    /* Assuming that camera preview has already been started */
    extraViewWindow = ANativeWindow_fromSurface(env, surface);

    LOGI("Extra view surface is prepared in %p.\n", surface);
    ACameraCaptureSession_stopRepeating(captureSession);

    ACameraDevice_createCaptureRequest(cameraDevice, TEMPLATE_STILL_CAPTURE,
                                       &extraViewCaptureRequest);

    ACameraOutputTarget_create(extraViewWindow, &extraViewOutputTarget);
    ACaptureRequest_addTarget(extraViewCaptureRequest, extraViewOutputTarget);

    ACaptureSessionOutput_create(extraViewWindow, &extraViewSessionOutput);
    ACaptureSessionOutputContainer_add(captureSessionOutputContainer,
                                       extraViewSessionOutput);

    /* Not sure why the session should be recreated.
     * Otherwise, the session state goes to ready */
    ACameraCaptureSession_close(captureSession);
    ACameraDevice_createCaptureSession(cameraDevice, captureSessionOutputContainer,
                                       &captureSessionStateCallbacks, &captureSession);

    ACaptureRequest *requests[2];
    requests[0] = captureRequest;
    requests[1] = extraViewCaptureRequest;

    ACameraCaptureSession_setRepeatingRequest(captureSession, NULL, 2, requests, NULL);
}

JNIEXPORT void JNICALL Java_com_example_cppcamera_MainActivity_stopExtraView(JNIEnv *env,
                                                                                      jclass clazz) {

    ACameraCaptureSession_stopRepeating(captureSession);

    ACaptureSessionOutputContainer_remove(captureSessionOutputContainer, extraViewSessionOutput);

    ACaptureRequest_removeTarget(extraViewCaptureRequest, extraViewOutputTarget);

    ACameraOutputTarget_free(extraViewOutputTarget);
    ACaptureSessionOutput_free(extraViewSessionOutput);
    ACaptureRequest_free(extraViewCaptureRequest);

    ACameraCaptureSession_setRepeatingRequest(captureSession, NULL, 1, &captureRequest, NULL);

    if (extraViewWindow != NULL) {
        ANativeWindow_release(extraViewWindow);
        extraViewWindow = NULL;
        LOGI("Extra view surface is released\n");

    }
}

JNIEXPORT void JNICALL Java_com_example_cppcamera_CamRenderer_onSurfaceCreated(JNIEnv *env, jclass clazz, jint texId, jobject surface)
{
    initSurface(env, texId, surface);
}

JNIEXPORT void JNICALL Java_com_example_cppcamera_CamRenderer_onSurfaceChanged(JNIEnv *env, jclass clazz, jint w, jint h)
{
    width = w;
    height = h;
}

JNIEXPORT void JNICALL Java_com_example_cppcamera_CamRenderer_onDrawFrame(JNIEnv *env, jclass clazz, jfloatArray texMatArray)
{
    drawFrame(env, texMatArray);
}