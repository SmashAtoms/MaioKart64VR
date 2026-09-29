#pragma once
#include "gles_renderer.hpp"
#include "aether/runtime.hpp"
#include "aether/launcher.hpp"
#include <android_native_app_glue.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace aether {
class XrApp {
    android_app* app_;
    JNIEnv* env_=nullptr;
    jclass activityClass_=nullptr;
    EglContext egl_;
    GlesRenderer graphics_;
    XrInstance instance_=XR_NULL_HANDLE;
    XrSystemId system_=XR_NULL_SYSTEM_ID;
    XrSession session_=XR_NULL_HANDLE;
    XrSpace local_=XR_NULL_HANDLE,viewSpace_=XR_NULL_HANDLE;
    XrSessionState sessionState_=XR_SESSION_STATE_UNKNOWN;
    XrActionSet actions_=XR_NULL_HANDLE;
    XrAction stick_=XR_NULL_HANDLE,accelerate_=XR_NULL_HANDLE,brake_=XR_NULL_HANDLE,hop_=XR_NULL_HANDLE,item_=XR_NULL_HANDLE,menu_=XR_NULL_HANDLE,gripPose_=XR_NULL_HANDLE,haptic_=XR_NULL_HANDLE;
    std::array<XrPath,2> hands_{};
    std::array<XrSpace,2> handSpaces_{};
    struct Swapchain {
        XrSwapchain handle=XR_NULL_HANDLE;
        int width=0,height=0;
        std::vector<XrSwapchainImageOpenGLESKHR> images;
        std::vector<GLuint> fbos,depth;
    };
    std::array<Swapchain,2> swapchains_;
    int64_t format_=GL_SRGB8_ALPHA8;
    Lifecycle lifecycle_;
    Launcher launcher_;
    FixedScheduler scheduler_;
    InputQueue input_;
    std::unique_ptr<GameAdapter> adapter_;
    std::unique_ptr<HostServices> host_;
    Pose neutral_{};
    bool neutralSet_=false,recenterRequested_=false,androidResumed_=false,restartRequested_=false,exiting_=false;
    uint32_t priorUiButtons_=0;
    bool menuVisible_=true;
    Page beforePause_=Page::Home;
    int priorNavigation_=0;
    uint64_t frame_=0,submitted_=0,omitted_=0;
    Time lastPanelTime_=0;
    std::string panelSignature_,runtimeName_,gpuName_,lastError_;
    double lastCpuMs_=0;
    void Initialize();
    void Shutdown();
    void CreateActions();
    void CreateSwapchains();
    void PollEvents();
    InputSample SyncInput();
    void Frame();
    void HandleUi(InputSample);
    void Execute(UiCommand);
    void UpdatePanel(Time);
    void Pause(const std::string& reason);
    void StopHaptics();
    std::string JavaString(const char* method);
    void JavaVoid(const char* method);
    std::string Report() const;
public:
    explicit XrApp(android_app* app):app_(app){}
    void Run();
    void Command(int32_t command);
};
void Log(const std::string&);
}
