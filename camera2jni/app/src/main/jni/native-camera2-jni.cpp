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
#include <string>
#include <vector>
#include <jni.h>
#include <pthread.h>
#include <android/native_window_jni.h>
#include "messages-internal.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3ext.h>
#include <GLES3/gl31.h>
static const char *vtxShader = R"(

precision mediump float;
vec2 v_TexCoordinate;
void main()
{
    v_TexCoordinate = vec2(0.5, 0.5);
}
)";

static const char *frgShader = R"(

#extension GL_OES_EGL_image_external_essl3 : require
#extension GL_OES_EGL_image_external : require
precision mediump float;
vec2 v_TexCoordinate;
vec4 fragmentColor;
layout(binding = 1) buffer Output { float elements[]; } out_data;
uniform samplerExternalOES cameraTexture;
void main()
{
    fragmentColor = texture(cameraTexture, v_TexCoordinate);
    out_data.elements[0] = 0.5;
    out_data.elements[1] = 0.5;
    out_data.elements[2] = 0.5;
}
)";
GLuint createShader(const char *src, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> logStr(maxLength);

        glGetShaderInfoLog(shader, maxLength, &maxLength, logStr.data());
        LOGE("Could not compile shader %s - %s", src, logStr.data());
    }

    return shader;
}

GLuint createProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertexShader);
    glAttachShader(prog, fragmentShader);
    glLinkProgram(prog);

    GLint isLinked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
        LOGE("Could not link program");

    return prog;
}
static int testShader(int w, int h) {
    GLuint vtx = createShader(vtxShader, GL_VERTEX_SHADER);
    GLuint frag = createShader(frgShader, GL_FRAGMENT_SHADER);
    GLuint prog = createProgram(vtx, frag);
    return prog;
}

int createShader(int img_cx, int img_cy) {
    std::string shaderSrc =
            "   #version 310 es\n"
            "   #extension GL_OES_EGL_image_external_essl3: enable\n"
            "   precision mediump float;\n"
            "   layout(local_size_x = 8, local_size_y = 8) in;\n"
            "   layout(binding = 0) uniform samplerExternalOES in_data; \n"
            "   layout(std430) buffer;\n"
            "   layout(binding = 1) buffer Input { float elements[]; } out_data;\n"
            "   void main() {\n"
            "     ivec2 gid = ivec2(gl_GlobalInvocationID.xy);\n"
            "     if (gid.x >= " + std::to_string(img_cx) + " || gid.y >= " + std::to_string(img_cy) + ") return;\n"
            "     vec2 uv = vec2(gl_GlobalInvocationID.xy) / " + std::to_string(img_cx) + ".0;\n"
            "     vec4 pixel = texture (in_data, uv);\n"
            "     int idx = 3 * (gid.y * " + std::to_string(img_cx) + " + gid.x);\n"
            "     out_data.elements[idx + 0] = pixel.x;\n"
            "     out_data.elements[idx + 1] = pixel.y;\n"
            "     out_data.elements[idx + 2] = pixel.z;\n"
            "   }";


    GLuint shader;
    GLint compiled;
    LOGE("shader\n %s", shaderSrc.c_str());
    // Create the shader object
    shader = glCreateShader(GL_COMPUTE_SHADER);
    if(shader == 0)
        return shader;
    const char *src[] = {shaderSrc.c_str()};
    // Load the shader source
    int length = 1;
    glShaderSource(shader, 1, src, &length);

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
            char* infoLog = (char*)malloc(sizeof(char) * infoLen);
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            LOGE("Error compiling shader:\n%s\n", infoLog);
            free(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    return program;
}


static void readSsbo(int texId, int ssboId) {
//   glBindTexture(GL_TEXTURE_EXTERNAL_OES, texId);
   glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboId);
//    glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, ssboId);
    unsigned char *data = (unsigned char *)
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 256*256*3, GL_MAP_READ_BIT);
    if (data != NULL) {
        LOGE("ssbo data: %d %d %d %d %d %d", data[0], data[1], data[2], data[3], data[4], data[5]);
        for (int i = 0; i < 256 * 256; i++) {
            if (data[i] != 0) {
                LOGE("%d %d", i, data[i]);
            }

        }

    } else {
        LOGE("empty texId %d ,  ssbo id %d", texId, ssboId);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
//    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
}
static void writeSsbo(int texId, int ssboId) {

//    int proId = createShader(256, 256);
//    GLuint bufId;
//    glGenBuffers(1, &bufId);
//    glBufferData(GL_SHADER_STORAGE_BUFFER, 256*256*3, NULL, GL_STREAM_COPY);
//    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

//    int proId = testShader(256, 256);
    int proId = createShader(256, 256);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texId);
//    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboId);
//    int test[] = {1,2,3,4,5};
//    glBufferData(GL_SHADER_STORAGE_BUFFER, 20, test, GL_DYNAMIC_COPY);
//    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
////    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, ssboId, 0, 255*255*4);
//    glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, ssboId);
//    glUseProgram(proId);
//    glDispatchCompute(4, 1, 1);  // smaller work group sizes for lower end GPU.
//    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
//    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    readSsbo(texId, ssboId);
//    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    GLuint FramebufferName;
    glGenFramebuffers(1, &FramebufferName);
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_EXTERNAL_OES, texId, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LOGE("%s", "Error: Could not setup frame buffer.");
    }

    unsigned char* dataf = new unsigned char[256 * 256 * 4];
    glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, dataf);
    if (dataf) {
        LOGE("pixel data: %d %d %d %d %d %d", dataf[0], dataf[1], dataf[2], dataf[3], dataf[4], dataf[5]);
    }
//    glBindTexture(GL_TEXTURE_2D, g_TexturePointer);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glDeleteFramebuffers(1, &FramebufferName);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    delete[] dataf;
//    glBindTexture(GL_TEXTURE_2D, 0);


    glDeleteProgram(proId);

}
extern "C"
JNIEXPORT void JNICALL Java_com_example_camera2jni_Camera2BasicFragment_readSsbo(JNIEnv *env,
                                                                                     jclass clazz,
                                                                                     jint texId,
                                                                                     jint ssboId)
{
    readSsbo(texId, ssboId);
}

extern "C"
JNIEXPORT void JNICALL Java_com_example_camera2jni_Camera2BasicFragment_writeSsbo(JNIEnv *env,
                                                                                 jclass clazz,
                                                                                 jint texId,
                                                                                 jint ssboId)
{
    writeSsbo(texId, ssboId);
}

