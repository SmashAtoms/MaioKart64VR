#include "aether/adapter.hpp"
#include <cmath>

namespace aether {
namespace {
struct DiagnosticFrame final:FramePayload { std::vector<Vertex> vertices; };
void Cube(std::vector<Vertex>& out,Vec3 center,float size,std::array<float,4> color) {
    const Vec3 corners[]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    const unsigned indices[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
    for(auto i:indices)out.push_back({center+corners[i]*(size*.5f),color});
}
class DiagnosticAdapter final:public GameAdapter {
    HostServices* host_=nullptr;
    float previous_=0,current_=0;
    bool paused_=false;
public:
    Capabilities DescribeCapabilities()const override{return {"diagnostic","XR camera test","",true,"",{CameraMode::Driver},30};}
    Error Initialize(HostServices& h,const std::string&)override {RequireNoLease();host_=&h;previous_=current_=0;paused_=false;return {};}
    void StepSimulation(const StepContext& step,const TickInput&)override {
        RequireNoLease();if(paused_)return;previous_=current_;current_+=static_cast<float>(step.duration/1e9);
    }
    std::unique_ptr<FrameLease> CaptureFrame(const FrameContext& ctx)override {
        RequireNoLease();auto p=std::make_unique<DiagnosticFrame>();
        const float t=previous_+(current_-previous_)*ctx.alpha;
        Cube(p->vertices,{std::sin(t)*.3f,0,-2},.25f,{.15f,.85f,.8f,1});
        // Original, meter-scale geometry surrounds the viewer to test rear/side visibility.
        for(int i=0;i<12;++i){float a=i*6.2831853f/12;Cube(p->vertices,{3*std::sin(a),-.5f,3*std::cos(a)},.2f,{.8f,.4f,.15f,1});}
        for(int x=-4;x<=4;++x)for(int z=-4;z<=4;++z)Cube(p->vertices,{static_cast<float>(x),-1.5f,static_cast<float>(z)},.025f,{.3f,.4f,.55f,1});
        // A one-meter bar and ten-centimeter graduations.
        for(int i=0;i<=10;++i)Cube(p->vertices,{-.5f+i*.1f,-.5f,-1.5f},.02f,{1,1,1,1});
        return std::make_unique<FrameLease>(leases_,std::move(p),ctx);
    }
    void RenderView(const FrameLease& lease,const ViewContext& view,const RenderTarget& target)override {
        if(!host_)throw std::logic_error("Adapter not initialized");
        if(view.predictedDisplayTime!=lease.context.predictedDisplayTime)throw std::logic_error("Mixed stereo times");
        host_->graphics.DrawTriangles(dynamic_cast<const DiagnosticFrame&>(lease.Payload()).vertices,view,target);
    }
    void Suspend()override{RequireNoLease();paused_=true;}
    void Resume()override{RequireNoLease();paused_=false;}
    void OnGraphicsLost()override{RequireNoLease();}
    Error OnGraphicsRestored()override{return {};}
    void Shutdown()override{RequireNoLease();host_=nullptr;}
};
}
std::unique_ptr<GameAdapter> MakeDiagnosticAdapter(){return std::make_unique<DiagnosticAdapter>();}
}
