#include "xr_app.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace aether {
namespace {
struct XrFailure:std::runtime_error {XrResult result;XrFailure(XrResult r,const char* op):std::runtime_error(std::string(op)+" failed ("+std::to_string(r)+")"),result(r){}};
void Check(XrResult r,const char* op){if(XR_FAILED(r))throw XrFailure(r,op);}
Pose Convert(const XrPosef& p){return {{p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w},{p.position.x,p.position.y,p.position.z}};}
XrPosef IdentityPose(){XrPosef p{};p.orientation.w=1;return p;}
template<size_t N>void Name(char(&out)[N],const char* name){std::strncpy(out,name,N-1);out[N-1]=0;}
using Clock=std::chrono::steady_clock;
}
std::string XrApp::JavaString(const char* name){
    auto method=env_->GetMethodID(activityClass_,name,"()Ljava/lang/String;");
    auto result=static_cast<jstring>(env_->CallObjectMethod(app_->activity->clazz,method));
    if(env_->ExceptionCheck()){env_->ExceptionClear();throw std::runtime_error(std::string("Android bridge: ")+name);}
    if(!result)return {};
    const char* chars=env_->GetStringUTFChars(result,nullptr);std::string value=chars?chars:"";
    if(chars)env_->ReleaseStringUTFChars(result,chars);env_->DeleteLocalRef(result);return value;
}
void XrApp::JavaVoid(const char* name){auto m=env_->GetMethodID(activityClass_,name,"()V");env_->CallVoidMethod(app_->activity->clazz,m);if(env_->ExceptionCheck()){env_->ExceptionClear();throw std::runtime_error(std::string("Android bridge: ")+name);}}
void XrApp::Initialize(){
    PFN_xrInitializeLoaderKHR initializeLoader=nullptr;
    Check(xrGetInstanceProcAddr(XR_NULL_HANDLE,"xrInitializeLoaderKHR",reinterpret_cast<PFN_xrVoidFunction*>(&initializeLoader)),"Find Android loader initialization");
    XrLoaderInitInfoAndroidKHR loaderInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};loaderInfo.applicationVM=app_->activity->vm;loaderInfo.applicationContext=app_->activity->clazz;
    Check(initializeLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInfo)),"Initialize Android OpenXR loader");
    uint32_t count=0;Check(xrEnumerateInstanceExtensionProperties(nullptr,0,&count,nullptr),"List extensions");
    std::vector<XrExtensionProperties> available(count,{XR_TYPE_EXTENSION_PROPERTIES});Check(xrEnumerateInstanceExtensionProperties(nullptr,count,&count,available.data()),"Read extensions");
    auto has=[&](const char* name){return std::any_of(available.begin(),available.end(),[&](auto& e){return std::strcmp(e.extensionName,name)==0;});};
    std::vector<const char*> extensions{XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
    for(auto e:extensions)if(!has(e))throw std::runtime_error(std::string("Runtime missing ")+e);
    bool refresh=has(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);if(refresh)extensions.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
    XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};androidInfo.applicationVM=app_->activity->vm;androidInfo.applicationActivity=app_->activity->clazz;
    XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};info.next=&androidInfo;Name(info.applicationInfo.applicationName,"Aether64 XR");Name(info.applicationInfo.engineName,"Aether64");info.applicationInfo.applicationVersion=1;info.applicationInfo.engineVersion=1;info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);info.enabledExtensionCount=extensions.size();info.enabledExtensionNames=extensions.data();
    Check(xrCreateInstance(&info,&instance_),"Create instance");
    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};Check(xrGetInstanceProperties(instance_,&properties),"Runtime properties");runtimeName_=properties.runtimeName;
    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};systemInfo.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;Check(xrGetSystem(instance_,&systemInfo,&system_),"Find headset");
    PFN_xrGetOpenGLESGraphicsRequirementsKHR requirements=nullptr;Check(xrGetInstanceProcAddr(instance_,"xrGetOpenGLESGraphicsRequirementsKHR",reinterpret_cast<PFN_xrVoidFunction*>(&requirements)),"GLES requirements function");
    XrGraphicsRequirementsOpenGLESKHR limits{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};Check(requirements(instance_,system_,&limits),"GLES requirements");
    egl_.Create();GLint major=0,minor=0;glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
    auto version=XR_MAKE_VERSION(major,minor,0);if(version<limits.minApiVersionSupported||version>limits.maxApiVersionSupported)throw std::runtime_error("GLES context is outside runtime requirements");
    const auto* gpu=glGetString(GL_RENDERER);gpuName_=gpu?reinterpret_cast<const char*>(gpu):"Unknown GLES device";
    XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};binding.display=egl_.display;binding.config=egl_.config;binding.context=egl_.context;
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};sessionInfo.next=&binding;sessionInfo.systemId=system_;Check(xrCreateSession(instance_,&sessionInfo,&session_),"Create session");
    XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};spaceInfo.poseInReferenceSpace=IdentityPose();spaceInfo.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;Check(xrCreateReferenceSpace(session_,&spaceInfo,&local_),"Create LOCAL space");spaceInfo.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;Check(xrCreateReferenceSpace(session_,&spaceInfo,&viewSpace_),"Create VIEW space");
    CreateActions();CreateSwapchains();graphics_.Create(format_==GL_SRGB8_ALPHA8);
    host_=std::make_unique<HostServices>(HostServices{graphics_,Log,JavaString("dataDirectory")});
    auto cameraMethod=env_->GetMethodID(activityClass_,"loadCamera","()I");launcher_.settings.camera=env_->CallIntMethod(app_->activity->clazz,cameraMethod)==1?CameraMode::Chase:CameraMode::Driver;
    launcher_.settings.diagnostics=env_->CallBooleanMethod(app_->activity->clazz,env_->GetMethodID(activityClass_,"loadDiagnostics","()Z"));
    launcher_.SetGameState(env_->CallBooleanMethod(app_->activity->clazz,env_->GetMethodID(activityClass_,"hasGameData","()Z")),false,"Game archive detected. The Mario Kart 64 renderer adapter is not included in this diagnostic build.");
    if(refresh){
        PFN_xrEnumerateDisplayRefreshRatesFB enumerate=nullptr;PFN_xrRequestDisplayRefreshRateFB request=nullptr;
        Check(xrGetInstanceProcAddr(instance_,"xrEnumerateDisplayRefreshRatesFB",reinterpret_cast<PFN_xrVoidFunction*>(&enumerate)),"Enumerate refresh function");
        Check(xrGetInstanceProcAddr(instance_,"xrRequestDisplayRefreshRateFB",reinterpret_cast<PFN_xrVoidFunction*>(&request)),"Request refresh function");
        uint32_t n=0;Check(enumerate(session_,0,&n,nullptr),"Refresh count");std::vector<float> rates(n);Check(enumerate(session_,n,&n,rates.data()),"Refresh rates");
        if(std::find(rates.begin(),rates.end(),72.f)!=rates.end()){auto r=request(session_,72);Log("72 Hz request result: "+std::to_string(r));}
    }
    neutralSet_=false;panelSignature_.clear();lastPanelTime_=0;restartRequested_=false;lifecycle_={};
    Log("Initialized "+runtimeName_+" / "+gpuName_);
}
void XrApp::CreateActions(){
    Check(xrStringToPath(instance_,"/user/hand/left",&hands_[0]),"Left hand path");Check(xrStringToPath(instance_,"/user/hand/right",&hands_[1]),"Right hand path");
    XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};Name(setInfo.actionSetName,"aether_input");Name(setInfo.localizedActionSetName,"Aether64 controls");Check(xrCreateActionSet(instance_,&setInfo,&actions_),"Create action set");
    auto create=[&](XrAction& action,const char* name,XrActionType type,bool hands=false){XrActionCreateInfo i{XR_TYPE_ACTION_CREATE_INFO};i.actionType=type;Name(i.actionName,name);Name(i.localizedActionName,name);if(hands){i.countSubactionPaths=2;i.subactionPaths=hands_.data();}Check(xrCreateAction(actions_,&i,&action),"Create action");};
    create(stick_,"steering",XR_ACTION_TYPE_VECTOR2F_INPUT);create(accelerate_,"accelerate",XR_ACTION_TYPE_BOOLEAN_INPUT);create(brake_,"brake",XR_ACTION_TYPE_BOOLEAN_INPUT);create(hop_,"hop",XR_ACTION_TYPE_FLOAT_INPUT);create(item_,"item",XR_ACTION_TYPE_FLOAT_INPUT);create(menu_,"menu",XR_ACTION_TYPE_BOOLEAN_INPUT);create(gripPose_,"hand_pose",XR_ACTION_TYPE_POSE_INPUT,true);create(haptic_,"haptic",XR_ACTION_TYPE_VIBRATION_OUTPUT,true);
    std::vector<XrActionSuggestedBinding> bindings;
    auto bind=[&](XrAction action,const char* path){XrPath p;Check(xrStringToPath(instance_,path,&p),"Binding path");bindings.push_back({action,p});};
    bind(stick_,"/user/hand/left/input/thumbstick");bind(accelerate_,"/user/hand/right/input/a/click");bind(brake_,"/user/hand/right/input/b/click");bind(hop_,"/user/hand/right/input/trigger/value");bind(item_,"/user/hand/left/input/trigger/value");bind(menu_,"/user/hand/left/input/menu/click");
    bind(gripPose_,"/user/hand/left/input/grip/pose");bind(gripPose_,"/user/hand/right/input/grip/pose");bind(haptic_,"/user/hand/left/output/haptic");bind(haptic_,"/user/hand/right/output/haptic");
    XrPath profile;Check(xrStringToPath(instance_,"/interaction_profiles/oculus/touch_controller",&profile),"Touch profile");
    XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};suggested.interactionProfile=profile;suggested.countSuggestedBindings=bindings.size();suggested.suggestedBindings=bindings.data();Check(xrSuggestInteractionProfileBindings(instance_,&suggested),"Suggest Touch bindings");
    XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&actions_;Check(xrAttachSessionActionSets(session_,&attach),"Attach action set");
    for(int i=0;i<2;++i){XrActionSpaceCreateInfo space{XR_TYPE_ACTION_SPACE_CREATE_INFO};space.action=gripPose_;space.subactionPath=hands_[i];space.poseInActionSpace=IdentityPose();Check(xrCreateActionSpace(session_,&space,&handSpaces_[i]),"Create hand space");}
}
void XrApp::CreateSwapchains(){
    uint32_t count=0;Check(xrEnumerateViewConfigurationViews(instance_,system_,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"Stereo view count");if(count!=2)throw std::runtime_error("This build requires two stereo views");
    std::array<XrViewConfigurationView,2> configs{{{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}}};Check(xrEnumerateViewConfigurationViews(instance_,system_,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,configs.data()),"Stereo configuration");
    Check(xrEnumerateSwapchainFormats(session_,0,&count,nullptr),"Swapchain format count");std::vector<int64_t> formats(count);Check(xrEnumerateSwapchainFormats(session_,count,&count,formats.data()),"Swapchain formats");
    format_=std::find(formats.begin(),formats.end(),GL_SRGB8_ALPHA8)!=formats.end()?GL_SRGB8_ALPHA8:GL_RGBA8;
    if(std::find(formats.begin(),formats.end(),format_)==formats.end())throw std::runtime_error("No supported RGBA swapchain format");
    for(int eye=0;eye<2;++eye){
        auto& s=swapchains_[eye];s.width=configs[eye].recommendedImageRectWidth;s.height=configs[eye].recommendedImageRectHeight;
        XrSwapchainCreateInfo i{XR_TYPE_SWAPCHAIN_CREATE_INFO};i.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;i.format=format_;i.sampleCount=1;i.width=s.width;i.height=s.height;i.faceCount=1;i.arraySize=1;i.mipCount=1;
        Check(xrCreateSwapchain(session_,&i,&s.handle),"Create eye swapchain");Check(xrEnumerateSwapchainImages(s.handle,0,&count,nullptr),"Image count");s.images.resize(count,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});Check(xrEnumerateSwapchainImages(s.handle,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(s.images.data())),"Swapchain images");
        s.fbos.resize(count);s.depth.resize(count);glGenFramebuffers(count,s.fbos.data());glGenRenderbuffers(count,s.depth.data());
        for(uint32_t image=0;image<count;++image){glBindFramebuffer(GL_FRAMEBUFFER,s.fbos[image]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,s.images[image].image,0);glBindRenderbuffer(GL_RENDERBUFFER,s.depth[image]);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,s.width,s.height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,s.depth[image]);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Incomplete eye framebuffer");}
    }GlCheck("Swapchain framebuffer setup");
}
void XrApp::PollEvents(){
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while(true){auto r=xrPollEvent(instance_,&event);if(r==XR_EVENT_UNAVAILABLE)break;Check(r,"Poll events");
        if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){
            auto& change=*reinterpret_cast<XrEventDataSessionStateChanged*>(&event);sessionState_=change.state;
            Log("Session state "+std::to_string(sessionState_));
            if(sessionState_==XR_SESSION_STATE_READY){XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;Check(xrBeginSession(session_,&begin),"Begin session");lifecycle_.phase=SessionPhase::Running;}
            if(sessionState_!=XR_SESSION_STATE_FOCUSED)Pause("Session interrupted. Select Resume when tracking and controllers return.");
            if(sessionState_==XR_SESSION_STATE_STOPPING){lifecycle_.phase=SessionPhase::Stopping;Check(xrEndSession(session_),"End session");lifecycle_.phase=SessionPhase::Idle;}
            if(sessionState_==XR_SESSION_STATE_LOSS_PENDING){lifecycle_.phase=SessionPhase::Lost;restartRequested_=true;}
            if(sessionState_==XR_SESSION_STATE_EXITING){lifecycle_.phase=SessionPhase::Exiting;exiting_=true;}
        }else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING){restartRequested_=true;lifecycle_.phase=SessionPhase::Lost;}
        else if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING){recenterRequested_=true;Pause("Reference space changed. Recenter and resume.");}
        event={XR_TYPE_EVENT_DATA_BUFFER};
    }
}
InputSample XrApp::SyncInput(){
    XrActiveActionSet set{actions_,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&set;auto result=xrSyncActions(session_,&sync);
    if(result==XR_SESSION_NOT_FOCUSED)return {};Check(result,"Synchronize input");
    auto boolean=[&](XrAction action){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;XrActionStateBoolean s{XR_TYPE_ACTION_STATE_BOOLEAN};Check(xrGetActionStateBoolean(session_,&get,&s),"Boolean input");return s.isActive&&s.currentState;};
    auto analog=[&](XrAction action){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;XrActionStateFloat s{XR_TYPE_ACTION_STATE_FLOAT};Check(xrGetActionStateFloat(session_,&get,&s),"Trigger input");return s.isActive?s.currentState:0.f;};
    InputSample sample;sample.active=true;
    for(auto hand:hands_){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=gripPose_;get.subactionPath=hand;XrActionStatePose pose{XR_TYPE_ACTION_STATE_POSE};Check(xrGetActionStatePose(session_,&get,&pose),"Hand input activity");sample.active=sample.active&&pose.isActive;}
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=stick_;XrActionStateVector2f stick{XR_TYPE_ACTION_STATE_VECTOR2F};Check(xrGetActionStateVector2f(session_,&get,&stick),"Steering input");
    sample.steering=stick.isActive?stick.currentState.x:0;sample.vertical=stick.isActive?stick.currentState.y:0;
    if(boolean(accelerate_))sample.held|=Accelerate|Confirm;if(boolean(brake_))sample.held|=Brake|Back;
    if(analog(hop_)>.5f)sample.held|=Hop;if(analog(item_)>.5f)sample.held|=Item;if(boolean(menu_))sample.held|=Button::Pause;
    return sample;
}
void XrApp::StopHaptics(){if(session_==XR_NULL_HANDLE||haptic_==XR_NULL_HANDLE)return;for(auto hand:hands_){XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};info.action=haptic_;info.subactionPath=hand;xrStopHapticFeedback(session_,&info);}}
void XrApp::Pause(const std::string& reason){
    if(adapter_&&!lifecycle_.paused){adapter_->Suspend();beforePause_=launcher_.CurrentPage();launcher_.Open(Page::Pause);menuVisible_=true;launcher_.message=reason;}
    lifecycle_.paused=true;input_.Clear();StopHaptics();
}
void XrApp::HandleUi(InputSample sample){
    if(!sample.active){priorUiButtons_=0;priorNavigation_=0;return;}
    uint32_t pressed=sample.held&~priorUiButtons_;priorUiButtons_=sample.held;
    if(pressed&Button::Pause){Pause("Game paused.");return;}
    if(!menuVisible_)return;
    int direction=sample.vertical>.65f?-1:sample.vertical<-.65f?1:0;
    if(direction&&direction!=priorNavigation_)launcher_.Navigate(direction);priorNavigation_=direction;
    if(pressed&Confirm)Execute(launcher_.Activate());else if(pressed&Back)Execute(launcher_.Back());
}
void XrApp::Execute(UiCommand command){
    switch(command){
    case UiCommand::Import:Pause("Importing game data.");JavaVoid("requestImport");break;
    case UiCommand::StartTest:
        if(adapter_){adapter_->Shutdown();adapter_.reset();}
        adapter_=MakeDiagnosticAdapter();{auto error=adapter_->Initialize(*host_,"");if(error)throw std::runtime_error(error.message);}scheduler_=FixedScheduler(30);input_.Clear();lifecycle_.Resume();break;
    case UiCommand::Launch:launcher_.message="The game adapter is not available in this build.";break;
    case UiCommand::Resume:
        if(lifecycle_.Resume()){if(adapter_)adapter_->Resume();launcher_.Open(beforePause_);input_.Clear();}else launcher_.message="Resume needs focus, valid tracking, and both Touch controllers.";break;
    case UiCommand::ExitGame:
        if(adapter_){adapter_->Shutdown();adapter_.reset();}input_.Clear();lifecycle_.paused=true;menuVisible_=true;launcher_.Open(Page::Home);launcher_.message="Choose a game or an XR test.";break;
    case UiCommand::Recenter:recenterRequested_=true;break;
    case UiCommand::SaveSettings:{auto method=env_->GetMethodID(activityClass_,"saveSettings","(IZ)V");env_->CallVoidMethod(app_->activity->clazz,method,launcher_.settings.camera==CameraMode::Chase?1:0,launcher_.settings.diagnostics);break;}
    case UiCommand::ExportDiagnostics:{auto report=env_->NewStringUTF(Report().c_str());env_->CallVoidMethod(app_->activity->clazz,env_->GetMethodID(activityClass_,"exportReport","(Ljava/lang/String;)V"),report);env_->DeleteLocalRef(report);break;}
    case UiCommand::None:break;
    }
}
std::string XrApp::Report()const {
    std::ostringstream s;s<<"Aether64 V1\nRuntime: "<<runtimeName_<<"\nGPU: "<<gpuName_<<"\nEye size: "<<swapchains_[0].width<<" x "<<swapchains_[0].height<<"\nSubmitted: "<<submitted_<<"  Omitted: "<<omitted_<<"\nCPU work last frame: "<<std::fixed<<std::setprecision(2)<<lastCpuMs_<<" ms\nGPU time / compositor missed frames: not measured\n72 Hz requested when supported; fresh-frame target is unverified.\nLast error: "<<lastError_;return s.str();
}
void XrApp::UpdatePanel(Time time){
    if(time-lastPanelTime_<250000000&&!panelSignature_.empty())return;
    lastPanelTime_=time;launcher_.diagnostics=Report();auto panel=launcher_.GetPanel();
    std::string signature=panel.title+std::to_string(panel.selected)+panel.message;for(auto& r:panel.rows)signature+=r;
    if(signature==panelSignature_)return;
    auto title=env_->NewStringUTF(panel.title.c_str()),message=env_->NewStringUTF(panel.message.c_str());
    auto stringClass=env_->FindClass("java/lang/String");auto rows=env_->NewObjectArray(panel.rows.size(),stringClass,nullptr);
    for(size_t i=0;i<panel.rows.size();++i){auto row=env_->NewStringUTF(panel.rows[i].c_str());env_->SetObjectArrayElement(rows,i,row);env_->DeleteLocalRef(row);}
    auto method=env_->GetMethodID(activityClass_,"renderPanel","(Ljava/lang/String;[Ljava/lang/String;ILjava/lang/String;)Landroid/graphics/Bitmap;");
    auto bitmap=env_->CallObjectMethod(app_->activity->clazz,method,title,rows,panel.selected,message);
    env_->DeleteLocalRef(title);env_->DeleteLocalRef(message);env_->DeleteLocalRef(rows);env_->DeleteLocalRef(stringClass);
    if(env_->ExceptionCheck()||!bitmap){env_->ExceptionClear();throw std::runtime_error("Android panel rendering failed");}
    graphics_.UploadPanel(env_,bitmap);env_->DeleteLocalRef(bitmap);panelSignature_=std::move(signature);
}
void XrApp::Frame(){
    XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState state{XR_TYPE_FRAME_STATE};Check(xrWaitFrame(session_,&wait,&state),"Wait frame");
    XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};Check(xrBeginFrame(session_,&begin),"Begin frame");
    const auto cpuStart=Clock::now();bool ended=false;std::array<bool,2> acquired{};std::array<uint32_t,2> indices{};
    try{
        InputSample sample=SyncInput();
        XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=state.predictedDisplayTime;locate.space=local_;
        std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};XrViewState viewState{XR_TYPE_VIEW_STATE};uint32_t viewCount=0;Check(xrLocateViews(session_,&locate,&viewState,2,&viewCount,views.data()),"Locate views");
        constexpr auto valid=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT;
        bool tracked=viewCount==2&&(viewState.viewStateFlags&valid)==valid;
        bool wasPaused=lifecycle_.paused;
        lifecycle_.Observe(sessionState_==XR_SESSION_STATE_FOCUSED,tracked,sample.active,androidResumed_);
        if(!wasPaused&&lifecycle_.paused){lifecycle_.paused=false;Pause("Tracking, focus, or controller input interrupted. Select Resume after recovery.");}
        if(tracked&&(!neutralSet_||recenterRequested_)){
            XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};Check(xrLocateSpace(viewSpace_,local_,state.predictedDisplayTime,&head),"Locate neutral head pose");
            constexpr auto headValid=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if((head.locationFlags&headValid)==headValid){neutral_=NeutralHeading(Convert(head.pose));neutralSet_=true;recenterRequested_=false;}
        }
        HandleUi(sample);
        auto importMessage=JavaString("pollImportResult");if(!importMessage.empty()){launcher_.message=importMessage;launcher_.SetGameState(env_->CallBooleanMethod(app_->activity->clazz,env_->GetMethodID(activityClass_,"hasGameData","()Z")),false,"Game archive detected. The Mario Kart 64 renderer adapter is not included in this diagnostic build.");panelSignature_.clear();}
        std::ostringstream inputText;inputText<<"Touch: "<<(sample.active?"active":"inactive")<<"\nSteering: "<<std::fixed<<std::setprecision(2)<<sample.steering<<"  Y: "<<sample.vertical<<"\nA accelerate: "<<bool(sample.held&Accelerate)<<"  B brake: "<<bool(sample.held&Brake)<<"\nRight trigger hop: "<<bool(sample.held&Hop)<<"  Left trigger item: "<<bool(sample.held&Item);launcher_.inputStatus=inputText.str();
        FrameContext context;context.frame=++frame_;context.predictedDisplayTime=state.predictedDisplayTime;context.camera=launcher_.settings.camera;
        if(lifecycle_.paused||!adapter_){scheduler_.Rebase(state.predictedDisplayTime);input_.Clear();}
        else{
            input_.Sample(sample);auto schedule=scheduler_.Advance(state.predictedDisplayTime);context.tick=schedule.tick;context.alpha=schedule.alpha;
            if(schedule.discontinuity)Pause("Frame timing was interrupted. Select Resume to continue.");
            else for(auto& step:schedule.steps)adapter_->StepSimulation(step,input_.Consume());
        }
        std::array<XrCompositionLayerProjectionView,2> projectionViews{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
        XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};projection.space=local_;projection.viewCount=2;projection.views=projectionViews.data();
        bool render=lifecycle_.MayRender(state.shouldRender)&&neutralSet_;
        if(render){
            for(unsigned eye=0;eye<2;++eye){auto& v=context.views[eye];v.eye=eye;v.predictedDisplayTime=state.predictedDisplayTime;v.referenceEye=Convert(views[eye].pose);v.view=RigidInverse(ComposeEye({},neutral_,v.referenceEye,Mat4::Identity(),1));v.projection=Projection(views[eye].fov.angleLeft,views[eye].fov.angleRight,views[eye].fov.angleUp,views[eye].fov.angleDown,.05f,100);}
            if(menuVisible_)UpdatePanel(state.predictedDisplayTime);
            auto lease=adapter_?adapter_->CaptureFrame(context):nullptr;
            for(unsigned eye=0;eye<2;++eye){
                auto& s=swapchains_[eye];XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};Check(xrAcquireSwapchainImage(s.handle,&acquire,&indices[eye]),"Acquire eye image");acquired[eye]=true;
                XrSwapchainImageWaitInfo imageWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};imageWait.timeout=XR_INFINITE_DURATION;Check(xrWaitSwapchainImage(s.handle,&imageWait),"Wait eye image");
                RenderTarget target{s.fbos.at(indices[eye]),0,0,s.width,s.height,format_,1};graphics_.BeginView(target);
                if(adapter_)adapter_->RenderView(*lease,context.views[eye],target);
                if(menuVisible_)graphics_.DrawPanel(context.views[eye],target);
                glFlush();XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};Check(xrReleaseSwapchainImage(s.handle,&release),"Release eye image");acquired[eye]=false;
                projectionViews[eye].pose=views[eye].pose;projectionViews[eye].fov=views[eye].fov;projectionViews[eye].subImage.swapchain=s.handle;projectionViews[eye].subImage.imageRect={{0,0},{s.width,s.height}};
            }
            // Lease destroyed after both draws. OpenXR retains ownership of released images.
            ++submitted_;
        }else ++omitted_;
        const XrCompositionLayerBaseHeader* layers[]={reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection)};
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=state.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;end.layerCount=render?1:0;end.layers=render?layers:nullptr;
        lastCpuMs_=std::chrono::duration<double,std::milli>(Clock::now()-cpuStart).count();ended=true;Check(xrEndFrame(session_,&end),"End frame");
    }catch(...){
        for(unsigned eye=0;eye<2;++eye)if(acquired[eye]){XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};xrReleaseSwapchainImage(swapchains_[eye].handle,&release);}
        if(!ended){XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=state.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;xrEndFrame(session_,&end);}throw;
    }
}
void XrApp::Shutdown(){
    if(adapter_){adapter_->Suspend();adapter_->OnGraphicsLost();adapter_->Shutdown();adapter_.reset();}host_.reset();StopHaptics();
    if(egl_.context!=EGL_NO_CONTEXT){graphics_.Destroy();for(auto& s:swapchains_){if(!s.fbos.empty())glDeleteFramebuffers(s.fbos.size(),s.fbos.data());if(!s.depth.empty())glDeleteRenderbuffers(s.depth.size(),s.depth.data());}}
    for(auto& s:swapchains_){if(s.handle)xrDestroySwapchain(s.handle);s={};}
    for(auto& hand:handSpaces_){if(hand)xrDestroySpace(hand);hand=XR_NULL_HANDLE;}
    if(viewSpace_)xrDestroySpace(viewSpace_);if(local_)xrDestroySpace(local_);viewSpace_=local_=XR_NULL_HANDLE;
    if(session_)xrDestroySession(session_);session_=XR_NULL_HANDLE;
    if(actions_)xrDestroyActionSet(actions_);actions_=XR_NULL_HANDLE;stick_=accelerate_=brake_=hop_=item_=menu_=gripPose_=haptic_=XR_NULL_HANDLE;
    if(instance_)xrDestroyInstance(instance_);instance_=XR_NULL_HANDLE;
    egl_.Destroy();lifecycle_={};input_.Clear();priorUiButtons_=0;priorNavigation_=0;
}
void XrApp::Command(int32_t command){
    if(command==APP_CMD_RESUME)androidResumed_=true;
    if(command==APP_CMD_PAUSE||command==APP_CMD_STOP){androidResumed_=false;Pause("Application paused. Select Resume after returning.");}
    if(command==APP_CMD_DESTROY)exiting_=true;
}
void XrApp::Run(){
    app_->activity->vm->AttachCurrentThread(&env_,nullptr);activityClass_=static_cast<jclass>(env_->NewGlobalRef(env_->GetObjectClass(app_->activity->clazz)));
    try{
        Initialize();unsigned recoveryAttempts=0;
        while(!app_->destroyRequested&&!exiting_){
            int events=0;android_poll_source* source=nullptr;
            while(ALooper_pollOnce(lifecycle_.MayRunFrames()?0:100,nullptr,&events,reinterpret_cast<void**>(&source))>=0){if(source)source->process(app_,source);if(app_->destroyRequested||exiting_)break;}
            if(app_->destroyRequested||exiting_)break;
            PollEvents();
            if(restartRequested_){if(++recoveryAttempts>2)throw std::runtime_error("OpenXR session recovery failed repeatedly");Shutdown();Initialize();launcher_.Open(Page::Home);launcher_.message="XR session recovered. Restart the test or game from the launcher.";continue;}
            if(lifecycle_.MayRunFrames()){
                try{Frame();recoveryAttempts=0;}
                catch(const XrFailure& e){lastError_=e.what();Log(lastError_);if(e.result==XR_ERROR_SESSION_LOST||e.result==XR_ERROR_INSTANCE_LOST||e.result==XR_ERROR_GRAPHICS_DEVICE_INVALID){restartRequested_=true;lifecycle_.phase=SessionPhase::Lost;}else throw;}
            }
        }
    }catch(const std::exception& e){lastError_=e.what();Log(lastError_);auto message=env_->NewStringUTF(lastError_.c_str());env_->CallVoidMethod(app_->activity->clazz,env_->GetMethodID(activityClass_,"showError","(Ljava/lang/String;)V"),message);env_->DeleteLocalRef(message);}
    Shutdown();env_->DeleteGlobalRef(activityClass_);app_->activity->vm->DetachCurrentThread();
}
}
