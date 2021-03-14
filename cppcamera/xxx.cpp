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
#include <EGL/egl.h>
#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>


#include "log.h"
#include "gl_helper.h"
#include "cam_utils.h"

#include "messages-internal.h"
#include <android/log.h>
#include <__locale>


static ANativeWindow *theNativeWindow;
static ACameraDevice *cameraDevice;
static ACaptureRequest *captureRequest;
static ACameraOutputTarget *cameraOutputTarget;
static ACaptureSessionOutput *sessionOutput;
static ACaptureSessionOutputContainer *captureSessionOutputContainer;
static ACameraCaptureSession *captureSession;

static ACameraDevice_StateCallbacks deviceStateCallbacks;
static ACameraCaptureSession_stateCallbacks captureSessionStateCallbacks;


#ifdef WITH_IMAGE_READER
static ANativeWindow* imageWindow = nullptr;

static ACameraOutputTarget* imageTarget = nullptr;

static AImageReader* imageReader = nullptr;

static ACaptureSessionOutput* imageOutput = nullptr;
#endif

GLuint textureId = 0;
const char *shaderSrc[] = {R"(
#version 310 es

// The uniform parameters that are passed from application for every frame.
uniform float radius;

// Declare the custom data type that represents one point of a circle.
// This is vertex position and color respectively,
// that defines the interleaved data within a
// buffer that is Vertex|Color|Vertex|Color|
struct AttribData
{
        vec4 v;
        vec4 c;
};

// Declare an input/output buffer that stores data.
// This shader only writes data into the buffer.
// std430 is a standard packing layout which is preferred for SSBOs.
// Its binary layout is well defined.
// Bind the buffer to index 0. You must set the buffer binding
// in the range [0..3]. This is the minimum range approved by Khronos.
// Some platforms might support more indices.
layout(std430, binding = 0) buffer destBuffer
{
        AttribData data[];
} outBuffer;

// Declare the group size.
// This is a one-dimensional problem, so prefer a one-dimensional group layout.
layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// Declare the main program function that is executed once
// glDispatchCompute is called from the application.
void main()
{
        // Read the current global position for this thread
        uint storePos = gl_GlobalInvocationID.x;

        // Calculate the global number of threads (size) for this work dispatch.
        uint gSize = gl_WorkGroupSize.x * gl_NumWorkGroups.x;

        // Calculate an angle for the current thread
        float alpha = 2.0 * 3.14159265359 * (float(storePos) / float(gSize));

        // Calculate the vertex position based on
        // the previously calculated angle and radius.
        // This is provided by the application.
        outBuffer.data[storePos].v = vec4(sin(alpha) * radius, cos(alpha) * radius, 0.0, 1.0);

        // Assign a color for the vertex
        outBuffer.data[storePos].c = vec4(float(storePos) / float(gSize), 0.0, 1.0, 1.0);

}
)"};
static const char *vtxShader = R"(
attribute vec4 a_v4Position;
attribute vec4 a_v4FillColor;
varying vec4 v_v4FillColor;

void main()
{
    v_v4FillColor = a_v4FillColor;
    gl_Position = a_v4Position;
}
)";

static const char *frgShader = R"(
#extension GL_OES_EGL_image_external : require
varying vec4 v_v4FillColor;
uniform samplerExternalOES texSampler;
void main()
{
      gl_FragColor = texture2D(texSampler, gl_PointCoord) * v_v4FillColor;
}
)";
static void compileShader();
static GLuint prog;
static GLuint eprog;
static GLuint shader;
static GLint iLocRadius;
static GLuint gIndexBufferBinding;
static GLuint vbo;
static GLuint ssbo;
static GLuint iLocPosition;
static GLuint iLocFillColor;
static GLuint texSampler;
static void initSurface(JNIEnv* env, jint texId, jobject surface)
{
    textureId = texId;
    compileShader();
    //获取顶点坐标字段
    iLocPosition = glGetAttribLocation(eprog, "a_v4Position");
    //获取纹理坐标字段
    iLocFillColor = glGetAttribLocation(eprog, "a_v4FillColor");
    texSampler = glGetUniformLocation(prog, "texSampler");
    // Set up vertices
    float vertices[] {

            -1, -1, 0, 0, 0,1,1,1,
            -1, 1, 1, 0, 1,1,1,1,
            1, 1, 0, 1, 1,1,1,1,
            1, -1, -1, -1, 0,1,1,1
    };

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    float vertices2[] {
            -1, -1, 0, 0, 0,1,1,1,
            -1, 1, 1, 0, 1,1,1,1,
            1, 1, 0, 1, 1,1,1,1,
            1, -1, -1, -1, 0,1,1,1
    };
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(vertices2), vertices2, GL_DYNAMIC_COPY);
}

static void drawFrame(JNIEnv* env, jfloatArray texMatArray)
{
 //   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
 //   glClearColor(1, 0, 0, 1);

//    glUseProgram(prog);
//    iLocRadius = glGetUniformLocation(prog, "radius");
//    // Set the radius uniform.
//    glUniform1f(iLocRadius, (float)1.0f);
//    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
//    glDispatchCompute(4, 1, 1);
//    glMemoryBarrier( GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT );
//    //glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
//    float *qtr = (float *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 32, GL_MAP_READ_BIT );
//    LOGD("addr %p %f, %f, %f, %f", qtr, (float)qtr[0], qtr[1], qtr[2], qtr[3]);
//
//    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

    glUseProgram(eprog);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(texSampler, 0);
    glBindBuffer( GL_ARRAY_BUFFER,  ssbo);
    glEnableVertexAttribArray(iLocPosition);
    glEnableVertexAttribArray(iLocFillColor);
    glVertexAttribPointer(iLocPosition, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)0);
    glVertexAttribPointer(iLocFillColor, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(4 * sizeof(float)));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 32);
    glDisableVertexAttribArray(iLocPosition);
    glDisableVertexAttribArray(iLocFillColor);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

static void compileShader() {

    prog = glCreateProgram();
    shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, shaderSrc, NULL);
    glCompileShader(shader);

    int rvalue;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &rvalue);

    int logLen = 0;
    char  errlog[1024] = {0};
    if (!rvalue) {
        glGetShaderInfoLog(shader, strlen(shaderSrc[0]), &logLen, errlog);
        LOGD("shader failed Compiler log:\n%s\n", errlog);
    } else {
        LOGD("shader succ Compiler");
    }

    glAttachShader(prog, shader);
    glLinkProgram(prog);

    glGetProgramiv(prog, GL_LINK_STATUS, &rvalue);

    if (!rvalue)
    {
        glGetProgramInfoLog(prog, strlen(shaderSrc[0]), &logLen, errlog);
        LOGD("shader failed: Linker log: %d\n %s", logLen, errlog);
    } else {
        LOGD("shader succ linker");
    }

    GLuint vtx = createShader(vtxShader, GL_VERTEX_SHADER);
    GLuint frag = createShader(frgShader, GL_FRAGMENT_SHADER);
    eprog = createProgram(vtx, frag);
}


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



extern "C" JNIEXPORT void JNICALL Java_com_example_cppcamera_CamRenderer_glStartPreview(JNIEnv *env,
                                                                            jclass clazz,
                                                                            jint texId,
                                                                            jobject ctx,
                                                                            jobject surface) {
    theNativeWindow = ANativeWindow_fromSurface(env, surface);
    initSurface(env, texId, surface);

    openCamera(TEMPLATE_PREVIEW);

    LOGI("Surface is prepared in %p.\n", surface);
    LOGI("Context is prepared in %p, current c++ is %p.\n", ctx, eglGetCurrentContext());

    ACameraOutputTarget_create(theNativeWindow, &cameraOutputTarget);
    ACaptureRequest_addTarget(captureRequest, cameraOutputTarget);

    ACaptureSessionOutput_create(theNativeWindow, &sessionOutput);
    ACaptureSessionOutputContainer_add(captureSessionOutputContainer, sessionOutput);

    ACameraDevice_createCaptureSession(cameraDevice, captureSessionOutputContainer,
                                       &captureSessionStateCallbacks, &captureSession);

    ACameraCaptureSession_setRepeatingRequest(captureSession, NULL, 1, &captureRequest, NULL);

}


extern "C" JNIEXPORT void JNICALL Java_com_example_cppcamera_MainActivity_stopPreview(JNIEnv *env,
                                                                                    jclass clazz) {
    closeCamera();
    if (theNativeWindow != NULL) {
        ANativeWindow_release(theNativeWindow);
        theNativeWindow = NULL;
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_example_cppcamera_CamRenderer_onSurfaceCreated(JNIEnv *env, jclass clazz, jint texId, jobject surface)
{

    initSurface(env, texId, surface);
}

extern "C" JNIEXPORT void JNICALL Java_com_example_cppcamera_CamRenderer_onSurfaceChanged(JNIEnv *env, jclass clazz, jint w, jint h)
{

}

extern "C" JNIEXPORT void JNICALL Java_com_example_cppcamera_CamRenderer_onDrawFrame(JNIEnv *env, jclass clazz, jfloatArray texMatArray)
{
    drawFrame(env, texMatArray);
}