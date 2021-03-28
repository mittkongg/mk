#include <assert.h>
#include <jni.h>
#include <pthread.h>
#include <android/native_window_jni.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <EGL/egl.h>
#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>
#include <android/log.h>
#define  LOG_TAG    "native-camera2-jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
GLuint ssboId = -1;
EGLContext ctx = NULL;
EGLContext dis = NULL;
static const char shaderSrc[] = "#version 320 es\n"
                                "#extension GL_OES_EGL_image_external_essl3 : require\n"
                                "#extension GL_OES_EGL_image_external: enable\n"
                  "precision mediump float;\n"
                  "layout(local_size_x = 8, local_size_y = 8) in;\n"
                  "uniform samplerExternalOES in_data;\n"
                  "layout(std430) buffer;\n"
                  "layout(binding = 1) buffer Input { float elements[]; } out_data;\n"
                  "void main() {\n"
                  "ivec2 gid = ivec2(gl_GlobalInvocationID.xy);\n"
                  "if (gid.x > 256 || gid.y > 256) return;\n"
                  "vec2 uv = vec2(gl_GlobalInvocationID.xy) / 256.0;\n"
                  "vec4 pixel = texture (in_data, uv);\n"
                  "int idx = 3 * (gid.y * 256 + gid.x);\n"
                  "out_data.elements[idx + 0] = 1.0;\n"
                  "out_data.elements[idx + 1] = pixel.y;\n"
                  "out_data.elements[idx + 2] = pixel.z;\n"
                  "}\n";

static const char gComputeShader[] =
        "#version 320 es\n"
        "layout(local_size_x = 8) in;\n"
        "layout(binding = 0) readonly buffer Input0 {\n"
        "    float data[];\n"
        "} input0;\n"
        "layout(binding = 1) readonly buffer Input1 {\n"
        "    float data[];\n"
        "} input1;\n"
        "layout(binding = 2) writeonly buffer Output {\n"
        "    float data[];\n"
        "} output0;\n"
        "void main()\n"
        "{\n"
        "    uint idx = gl_GlobalInvocationID.x;\n"
        "    float f = input0.data[idx] + input1.data[idx];"
        "    output0.data[idx] = f;\n"
        "}\n";

#define CHECK() \
{\
    GLenum err = glGetError(); \
    if (err != GL_NO_ERROR) \
    {\
        printf("glGetError returns %d\n", err); \
    }\
}
void init(EGLContext sharedContext) {
    EGLint numConfigs;
    EGLint width;
    EGLint height;
    EGLint major;//Main version number
    EGLint minor;//Minor version number

    const EGLint attibutes[] = {
            EGL_BUFFER_SIZE, 32,
            EGL_ALPHA_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, //Specify the rendering API version 2
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_NONE
    };
    EGLDisplay display = NULL;
    if ((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay() returned error %d", eglGetError());
        return;
    }
    if (!eglInitialize(display, &major, &minor)) {
        LOGE("eglInitialize() returned error %d", eglGetError());
        return;
    }
//Only take a config here
    EGLConfig config;
    if (!eglChooseConfig(display, attibutes, &config, 1, &numConfigs)) {
        LOGE("eglChooseConfig() returned error %d", eglGetError());

        return;
    }

    EGLint eglContextAttribute[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
    };

    EGLContext context;
    if (!(context = eglCreateContext(display, config, ctx, eglContextAttribute))) {
        LOGE("eglCreateContext() returned error %d", eglGetError());
        return;
    }

    if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
        return;
    }
    LOGE("INIT EGL OK");

}
GLuint loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf =  new char[infoLen];
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n",
                            shaderType, buf);
                    delete(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

GLuint createComputeProgram(const char* pComputeSource) {
    GLuint computeShader = loadShader(GL_COMPUTE_SHADER, pComputeSource);
    if (!computeShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, computeShader);
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = new char[bufLength];
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE( "Could not link program:\n%s\n", buf);
                    delete(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

void createSSBO(int cx, int cy) {

    glGenBuffers(1, &ssboId);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboId);

    if (ssboId == 0) {
        LOGE("cannot create SSBO.");
    }


    int PIXEL_SIZE = 3;
    int FLOAT_BYTE_SIZE = 4;
    int ssboSize = cx * cy * PIXEL_SIZE * FLOAT_BYTE_SIZE;


    glBufferData(GL_SHADER_STORAGE_BUFFER, ssboSize, NULL, GL_STREAM_COPY);

    int ssboSizeCreated;
    glGetBufferParameteriv (GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &ssboSizeCreated);
    if (ssboSizeCreated != ssboSize) {
        LOGE("cannot create SSBO with needed size.");
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);  // unbind output ssbo
    LOGE("createSSBO ssbo id: %u", ssboId);

}

int createShader(int img_cx, int img_cy) {
    char* shaderSrc = "#version 310 es\n"
                      "#extension GL_OES_EGL_image_external: enable\n"
                      "precision mediump float;\n"
                      "layout(local_size_x = 8, local_size_y = 8) in;\n"
                      "uniform samplerExternalOES in_data;\n"
                      "layout(std430) buffer;\n"
                      "layout(binding = 1) buffer Input { float elements[]; } out_data;\n"
                      "void main() {\n"
                      "ivec2 gid = ivec2(gl_GlobalInvocationID.xy);\n"
                      "if (gid.x > 256 || gid.y > 256) return;\n"
                      "vec2 uv = vec2(gl_GlobalInvocationID.xy) / 256.0;\n"
                      "vec4 pixel = texture (in_data, uv);\n"
                      "int idx = 3 * (gid.y * 256 + gid.x);\n"
                      "out_data.elements[idx + 0] = pixel.x;\n"
                      "out_data.elements[idx + 1] = pixel.y;\n"
                      "out_data.elements[idx + 2] = pixel.z;\n"
                      "}\n";


    GLuint shader;
    GLint compiled;
    LOGE("shader\n %s", shaderSrc);
    // Create the shader object
    shader = glCreateShader(GL_COMPUTE_SHADER);

    // Load the shader source
    int length = 1;
    glShaderSource(shader, 1, &shaderSrc, &length);

    // Compile the shader
    glCompileShader(shader);
    // Check the compile status
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

        if(infoLen > 1)
        {
            char* infoLog = new char[infoLen];
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            LOGE("Error compiling shader:\n%s\n", infoLog);
            delete(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    int rvalue;
    glGetProgramiv(program, GL_LINK_STATUS, &rvalue);

    if (!rvalue)

    {
        char log[200] = {0};
        glGetProgramInfoLog(program, 200, &length, log);

        LOGE("Error: Linker log:\n%s\n", log);

        return false;

    }
    return program;
}

void setupSSBufferObject(GLuint& ssbo, GLuint index, float* pIn, GLuint count)
{
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

    glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(float), pIn, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, ssbo);
}


static void readSsbo(int texId, int ssboId) {

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboId);

    float *data = (float *)
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 256*256*3*4, GL_MAP_READ_BIT);
    if (data != NULL) {
        LOGE("ssbo data: %f %f %f %f %f %f", data[0], data[1], data[2], data[3], data[4], data[5]);
/*        for (int i = 0; i < 256 * 256; i++) {
            if (data[i] != 0) {
                LOGE("%d %d", i, data[i]);
            }

        }
*/
    } else {
//        LOGE("empty texId %d ,  ssbo id %d", texId, ssboId);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

}
static void writeSsbo(int texId) {

    static bool isInit = false;
    static int proId = -1;
    if (!isInit) {
        createSSBO(256, 256);
//        proId = createShader(256, 256);
        proId = createComputeProgram(shaderSrc);
        isInit = true;
    }


    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texId);
    glUniform1i(glGetUniformLocation(proId, "in_data"), 0);
    setupSSBufferObject(ssboId, 1, NULL, 256*256*3);
    glUseProgram(proId);
    glDispatchCompute(4, 1, 1);  // smaller work group sizes for lower end GPU.
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    readSsbo(texId, ssboId);



}

extern "C"
JNIEXPORT void JNICALL Java_com_hrl_c_MainActivity_writeSsbo(JNIEnv *env,
                                                             jclass clazz,
                                                             jfloatArray texMatrix,
                                                             jint texId)
{
    writeSsbo(texId);
}

extern "C"
JNIEXPORT void JNICALL Java_com_hrl_c_MainActivity_passContext(JNIEnv *env,
                                                             jclass clazz,
                                                             jlong eglDisplay,
                                                             jlong eglContext)
{
    dis = (EGLDisplay)eglDisplay;
    ctx = (EGLContext)eglContext;
}

extern "C"
JNIEXPORT void JNICALL Java_com_hrl_c_MainActivity_noEnvDo(JNIEnv *env,
                                                               jclass clazz, jint texId)
{
    init(ctx);
    writeSsbo(texId);
}