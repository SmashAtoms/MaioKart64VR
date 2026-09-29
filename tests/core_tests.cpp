#include "aether/runtime.hpp"
#include "aether/launcher.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <type_traits>

using namespace aether;
static int assertions=0;
void Check(bool value,const char* what){++assertions;if(!value)throw std::runtime_error(what);}
void Near(float a,float b,const char* what){Check(std::abs(a-b)<.0001f,what);}
template<class F>void Throws(F f,const char* what){bool caught=false;try{f();}catch(const std::exception&){caught=true;}Check(caught,what);}
class Recorder:public RenderDevice {
public:
    std::vector<Vertex> first;
    int draws=0;
    void DrawTriangles(const std::vector<Vertex>& v,const ViewContext&,const RenderTarget& t)override {
        Check(t.framebuffer!=0,"external target retained");
        if(draws==0)first=v;else{
            Check(v.size()==first.size(),"same stereo geometry count");
            for(size_t i=0;i<v.size();++i){Near(v[i].position.x,first[i].position.x,"stereo X identical");Near(v[i].position.y,first[i].position.y,"stereo Y identical");Near(v[i].position.z,first[i].position.z,"stereo Z identical");}
        }++draws;
    }
};
void TestTiming(){
    FixedScheduler s(30);Check(s.Advance(1000000000).steps.empty(),"initial frame has no forced step");
    Check(s.Advance(1013888889).steps.empty(),"72 Hz frame may have zero steps");
    auto a=s.Advance(1034000000);Check(a.steps.size()==1,"30 Hz due update");
    auto b=s.Advance(1134000000);Check(b.steps.size()==3,"multiple steps after slow frame");
    Check(b.alpha>=0&&b.alpha<1,"interpolation bounded");
    s.Rebase(9000000000);Check(s.Advance(9010000000).steps.empty(),"resume has no catch-up");
    Check(s.Advance(9500000000).discontinuity,"large gap requires recovery");
    Check(s.Advance(9400000000).discontinuity,"backwards clock requires recovery");
    FixedScheduler exact(30);exact.Advance(0);size_t ticks=0;
    for(int i=1;i<=7200;++i)ticks+=exact.Advance(static_cast<Time>(i)*1000000000/72).steps.size();
    Check(ticks==3000,"100 seconds has exactly 3000 updates without rounding drift");
}
void TestInput(){
    InputQueue q;
    q.Sample({Accelerate,.4f,0,true});q.Sample({0,.5f,0,true});
    auto i=q.Consume();Check(i.pressed==Accelerate&&i.released==Accelerate,"short press survives unticked frames");Near(i.steering,.5f,"latest analog state");
    Check(q.Consume().pressed==0,"edge consumed exactly once");
    q.Sample({Item,0,0,true});q.Sample({0,0,0,true});q.Sample({Item,0,0,true});
    Check(q.Consume().pressed==Item,"first repeated press");Check(q.Consume().pressed==Item,"second repeated press retained");
    q.Sample({Item,0,0,false});Check(q.Consume().held==0,"focus loss clears held controls");Check(q.Consume().pressed==0,"focus loss clears stale edges");
}
void TestCamera(){
    Pose neutral{{},{10,2,4}};Pose eye{{},{10.032f,2,3.8f}};
    auto m=ComposeEye({{}, {100,20,30}},neutral,eye,Mat4::Identity(),100);
    Near(m.m[12],103.2f,"IPD scaled exactly once");Near(m.m[14],10,"head translation applied once");
    Vec3 origin=Transform(RigidInverse(m),{m.m[12],m.m[13],m.m[14]});Near(origin.x,0,"view is inverse eye");Near(origin.z,0,"view is inverse eye Z");
    const float h=std::sqrt(.5f);Pose turned{{0,h,0,h},{0,1,0}};
    auto recentered=ComposeEye({},NeutralHeading(turned),turned,Mat4::Identity(),1);
    Near(recentered.m[0],1,"recenter neutralizes heading");Near(recentered.m[12],0,"recenter neutralizes translation");
    auto reflect=Mat4::Identity();reflect.m[10]=-1;
    auto converted=ComposeEye({}, {}, {{},{0,0,-1}},reflect,2);Near(converted.m[14],2,"coordinate handedness conversion");
    auto projection=Projection(-.7f,.8f,.75f,-.65f,.05f,100);
    Check(projection.m[0]>0&&projection.m[5]>0&&projection.m[8]!=0,"asymmetric eye projection");
    Throws([]{Projection(0,0,0,0,1,0);},"invalid projection rejected");
}
void TestReplay(){
    TargetRouter router;RenderTarget left{17,0,0,1024,1024},right{29,0,0,1024,1024};
    router.RegisterEffect(0,1,{31,0,0,256,256});router.RegisterEffect(1,1,{32,0,0,256,256});
    router.BeginView(0,left);Check(router.Resolve(0).framebuffer==17&&router.Reset().framebuffer==17,"main/reset routed externally");Check(router.Resolve(1).framebuffer==31,"left effect target");
    router.BeginView(1,right);Check(router.Reset().framebuffer==29&&router.Resolve(1).framebuffer==32,"right effect isolated");
    Throws([&]{router.Resolve(999);},"unknown effect fails explicitly");
    ReplayState replay;bool callerFlag=false;replay.Begin(1);
    auto id=reinterpret_cast<uintptr_t>(&callerFlag);
    Check(replay.CopyOnce(0,id),"left copy executes");Check(!replay.CopyOnce(0,id),"left copy once");Check(replay.CopyOnce(1,id),"right copy independent");Check(!callerFlag,"caller flag immutable");
    replay.Begin(2);Check(replay.CopyOnce(0,id),"new application frame resets sidecar");
    router.GraphicsLost();Throws([&]{router.Resolve(1);},"graphics loss drops stale effect handles");
}
void TestLease(){
    static_assert(!std::is_copy_constructible<FrameLease>::value);
    Recorder recorder;HostServices host{recorder,[](const std::string&){},""};auto adapter=MakeDiagnosticAdapter();
    Check(!adapter->Initialize(host,""),"diagnostic initializes without data");
    adapter->StepSimulation({1,0,33333333},{});
    FrameContext context;context.alpha=.5f;context.predictedDisplayTime=12345;
    auto frame=adapter->CaptureFrame(context);
    Throws([&]{adapter->StepSimulation({},{});},"no simulation while leased");
    Throws([&]{adapter->Shutdown();},"no shutdown while leased");
    Throws([&]{adapter->CaptureFrame(context);},"no pool recycling while leased");
    ViewContext view;view.predictedDisplayTime=12345;
    adapter->RenderView(*frame,view,{17,0,0,1024,1024});view.eye=1;adapter->RenderView(*frame,view,{29,0,0,1024,1024});
    Check(recorder.draws==2,"both eyes replayed");
    view.predictedDisplayTime=999;Throws([&]{adapter->RenderView(*frame,view,{17,0,0,10,10});},"mixed eye timing rejected");
    frame.reset();adapter->Shutdown();Check(!adapter->Initialize(host,""),"repeat initialization");adapter->Shutdown();
}
void TestLifecycleAndMenu(){
    Lifecycle l;Check(!l.MayRunFrames(),"idle does not call frame loop");l.phase=SessionPhase::Running;
    l.Observe(true,true,true,true);Check(l.Resume(),"explicit resume");l.Observe(false,true,true,true);Check(l.paused,"focus loss pauses");
    Check(l.MayRunFrames(),"unfocused session maintains frame loop");Check(!l.MayRender(false),"shouldRender false omits draw");
    l.Observe(true,true,true,true);Check(l.paused,"focus return never auto resumes");
    l.Resume();l.Observe(true,false,true,true);Check(l.paused&&!l.MayRender(true),"invalid tracking omits world");
    l.Observe(true,true,false,true);Check(!l.Resume(),"controller loss requires recovery");
    l.phase=SessionPhase::Stopping;Check(!l.MayRunFrames(),"stop ends frame calls");
    Launcher ui;Check(ui.GetPanel().rows.size()==5,"no-data launcher available");ui.Activate();Check(ui.CurrentPage()==Page::Games,"game catalog independent of initialization");
    Check(ui.Activate()==UiCommand::Import,"missing data imports before game init");
    ui.SetGameState(true,false,"Not linked");Check(ui.Activate()!=UiCommand::Launch,"unavailable port not falsely launched");
    ui.SetGameState(true,true);Check(ui.Activate()==UiCommand::Launch,"supported imported game launches");
    ui.Open(Page::Settings);Check(ui.settings.camera==CameraMode::Driver,"driver is default");Check(ui.Activate()==UiCommand::SaveSettings&&ui.settings.camera==CameraMode::Chase,"camera toggle persists");
}
int main(){try{TestTiming();TestInput();TestCamera();TestReplay();TestLease();TestLifecycleAndMenu();std::cout<<"PASS: "<<assertions<<" contract assertions\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
