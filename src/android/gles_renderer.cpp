#include "gles_renderer.hpp"
#include <EGL/eglext.h>
#include <android/bitmap.h>
#include <stdexcept>
#include <vector>

namespace aether {
void GlCheck(const char* what){auto e=glGetError();if(e!=GL_NO_ERROR)throw std::runtime_error(std::string(what)+" GL error "+std::to_string(e));}
void EglContext::Create(){
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display==EGL_NO_DISPLAY||!eglInitialize(display,nullptr,nullptr))throw std::runtime_error("EGL display initialization failed");
    EGLint configAttributes[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT_KHR,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,0,EGL_NONE};
    EGLint count=0;
    if(!eglChooseConfig(display,configAttributes,&config,1,&count)||count!=1)throw std::runtime_error("No GLES3 EGL configuration");
    EGLint attributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,attributes);
    EGLint surfaceAttributes[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};
    surface=eglCreatePbufferSurface(display,config,surfaceAttributes);
    if(context==EGL_NO_CONTEXT||surface==EGL_NO_SURFACE||!eglMakeCurrent(display,surface,surface,context))throw std::runtime_error("EGL context creation failed");
}
void EglContext::Destroy(){if(display==EGL_NO_DISPLAY)return;eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);eglTerminate(display);display=EGL_NO_DISPLAY;context=EGL_NO_CONTEXT;surface=EGL_NO_SURFACE;}
namespace {
GLuint Shader(GLenum type,const char* source){
    GLuint s=glCreateShader(type);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){GLchar log[2048]{};glGetShaderInfoLog(s,sizeof(log),nullptr,log);glDeleteShader(s);throw std::runtime_error(log);}return s;
}
GLuint Program(const char* vertex,const char* fragment){
    GLuint vs=Shader(GL_VERTEX_SHADER,vertex),fs=0,p=0;
    try{fs=Shader(GL_FRAGMENT_SHADER,fragment);p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){GLchar log[2048]{};glGetProgramInfoLog(p,sizeof(log),nullptr,log);throw std::runtime_error(log);}}
    catch(...){glDeleteShader(vs);if(fs)glDeleteShader(fs);if(p)glDeleteProgram(p);throw;}
    glDeleteShader(vs);glDeleteShader(fs);return p;
}
const char* meshVertex=R"(#version 300 es
layout(location=0) in vec3 position;layout(location=1) in vec4 color;
uniform mat4 mvp;out vec4 tint;void main(){tint=color;gl_Position=mvp*vec4(position,1.0);})";
const char* meshFragment=R"(#version 300 es
precision mediump float;in vec4 tint;uniform bool encodeGamma;out vec4 frag;
void main(){vec3 c=tint.rgb;frag=vec4(encodeGamma?pow(c,vec3(1.0/2.2)):c,tint.a);})";
const char* panelVertex=R"(#version 300 es
layout(location=0) in vec3 position;layout(location=1) in vec2 uv;
uniform mat4 mvp;out vec2 coord;void main(){coord=uv;gl_Position=mvp*vec4(position,1.0);})";
const char* panelFragment=R"(#version 300 es
precision mediump float;in vec2 coord;uniform sampler2D panel;uniform bool encodeGamma;out vec4 frag;
void main(){vec4 c=texture(panel,coord);frag=vec4(encodeGamma?pow(c.rgb,vec3(1.0/2.2)):c.rgb,c.a);})";
}
void GlesRenderer::Create(bool srgb){
    encodeGamma_=!srgb;meshProgram_=Program(meshVertex,meshFragment);panelProgram_=Program(panelVertex,panelFragment);
    glGenVertexArrays(1,&vao_);glGenBuffers(1,&vbo_);glGenTextures(1,&panelTexture_);
    glBindTexture(GL_TEXTURE_2D,panelTexture_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);GlCheck("Renderer creation");
}
void GlesRenderer::Destroy(){if(meshProgram_)glDeleteProgram(meshProgram_);if(panelProgram_)glDeleteProgram(panelProgram_);if(vbo_)glDeleteBuffers(1,&vbo_);if(vao_)glDeleteVertexArrays(1,&vao_);if(panelTexture_)glDeleteTextures(1,&panelTexture_);meshProgram_=panelProgram_=vbo_=vao_=panelTexture_=0;}
void GlesRenderer::BeginView(const RenderTarget& t){
    glBindFramebuffer(GL_FRAMEBUFFER,t.framebuffer);glViewport(t.x,t.y,t.width,t.height);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(.015f,.025f,.05f,1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}
void GlesRenderer::DrawTriangles(const std::vector<Vertex>& vertices,const ViewContext& view,const RenderTarget& t){
    glBindFramebuffer(GL_FRAMEBUFFER,t.framebuffer);glViewport(t.x,t.y,t.width,t.height);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
    glUseProgram(meshProgram_);auto mvp=view.projection*view.view;glUniformMatrix4fv(glGetUniformLocation(meshProgram_,"mvp"),1,GL_FALSE,mvp.m.data());glUniform1i(glGetUniformLocation(meshProgram_,"encodeGamma"),encodeGamma_);
    glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,vbo_);glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices.size()*sizeof(Vertex)),vertices.data(),GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,color)));
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices.size()));GlCheck("Diagnostic draw");
}
void GlesRenderer::UploadPanel(JNIEnv* env,jobject bitmap){
    AndroidBitmapInfo info{};if(AndroidBitmap_getInfo(env,bitmap,&info)!=ANDROID_BITMAP_RESULT_SUCCESS||info.format!=ANDROID_BITMAP_FORMAT_RGBA_8888)throw std::runtime_error("Invalid panel bitmap");
    void* pixels=nullptr;if(AndroidBitmap_lockPixels(env,bitmap,&pixels)!=ANDROID_BITMAP_RESULT_SUCCESS)throw std::runtime_error("Panel bitmap unavailable");
    glBindTexture(GL_TEXTURE_2D,panelTexture_);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glPixelStorei(GL_UNPACK_ROW_LENGTH,info.stride/4);
    glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,info.width,info.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);AndroidBitmap_unlockPixels(env,bitmap);GlCheck("Panel upload");
}
void GlesRenderer::DrawPanel(const ViewContext& view,const RenderTarget& t){
    // Engine UI lives in recentered reference space, never in a game's world space.
    const float vertices[]={-.65f,-.65f,-1.8f,0,1, .65f,-.65f,-1.8f,1,1, .65f,.65f,-1.8f,1,0,
                           -.65f,-.65f,-1.8f,0,1, .65f,.65f,-1.8f,1,0, -.65f,.65f,-1.8f,0,0};
    glBindFramebuffer(GL_FRAMEBUFFER,t.framebuffer);glViewport(t.x,t.y,t.width,t.height);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
    glUseProgram(panelProgram_);auto mvp=view.projection*view.view;glUniformMatrix4fv(glGetUniformLocation(panelProgram_,"mvp"),1,GL_FALSE,mvp.m.data());glUniform1i(glGetUniformLocation(panelProgram_,"encodeGamma"),encodeGamma_);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,panelTexture_);glUniform1i(glGetUniformLocation(panelProgram_,"panel"),0);
    glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,vbo_);glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),nullptr);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),reinterpret_cast<void*>(3*sizeof(float)));
    glDrawArrays(GL_TRIANGLES,0,6);glDepthMask(GL_TRUE);GlCheck("Panel draw");
}
}
