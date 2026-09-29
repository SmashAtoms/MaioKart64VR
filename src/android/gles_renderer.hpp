#pragma once
#include "aether/adapter.hpp"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <string>

namespace aether {
class EglContext {
public:
    EGLDisplay display=EGL_NO_DISPLAY;
    EGLConfig config=nullptr;
    EGLContext context=EGL_NO_CONTEXT;
    EGLSurface surface=EGL_NO_SURFACE;
    void Create();
    void Destroy();
    ~EglContext(){Destroy();}
};
class GlesRenderer final:public RenderDevice {
    GLuint meshProgram_=0,panelProgram_=0,vao_=0,vbo_=0,panelTexture_=0;
    bool encodeGamma_=false;
public:
    void Create(bool srgbTarget);
    void Destroy();
    void BeginView(const RenderTarget&);
    void DrawTriangles(const std::vector<Vertex>&,const ViewContext&,const RenderTarget&)override;
    void UploadPanel(JNIEnv*,jobject bitmap);
    void DrawPanel(const ViewContext&,const RenderTarget&);
};
void GlCheck(const char* operation);
}
