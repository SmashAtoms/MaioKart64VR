#pragma once
#include "aether/math.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace aether {
using Time = int64_t; // Nanoseconds in the runtime's XrTime domain, never wall-clock time.
enum class CameraMode { Driver, Chase };
enum class PassKind { World, FlatUi, Offscreen };
struct Error { std::string code, message; explicit operator bool() const { return !code.empty(); } };
struct Capabilities {
    std::string id, name, requiredArchive;
    bool available=true;
    std::string unavailableReason;
    std::vector<CameraMode> cameras;
    int presentationHz=30;
};
enum Button : uint32_t { Accelerate=1, Brake=2, Hop=4, Item=8, Pause=16, Confirm=32, Back=64, Recenter=128, Camera=256 };
struct InputSample {
    uint32_t held=0;
    float steering=0, vertical=0;
    bool active=false;
};
struct TickInput : InputSample { uint32_t pressed=0, released=0; };
struct StepContext { uint64_t tick=0; Time simulationTime=0; Time duration=0; };
struct ViewContext {
    uint32_t eye=0;
    Time predictedDisplayTime=0;
    Pose referenceEye{};
    Mat4 view=Mat4::Identity(), projection=Mat4::Identity();
};
struct FrameContext {
    uint64_t frame=0, tick=0;
    Time predictedDisplayTime=0;
    float alpha=0;
    CameraMode camera=CameraMode::Driver;
    std::array<ViewContext,2> views{};
};
struct RenderTarget {
    uint32_t framebuffer=0;
    int x=0,y=0,width=0,height=0;
    int64_t colorFormat=0;
    uint32_t samples=1;
};
struct Vertex { Vec3 position; std::array<float,4> color; };
class RenderDevice {
public:
    virtual ~RenderDevice()=default;
    virtual void DrawTriangles(const std::vector<Vertex>& vertices,const ViewContext&,const RenderTarget&)=0;
};
struct HostServices {
    RenderDevice& graphics;
    std::function<void(const std::string&)> log;
    std::string dataDirectory;
};
struct FramePayload { virtual ~FramePayload()=default; };
struct LeaseState { unsigned live=0; };
class FrameLease {
    std::shared_ptr<LeaseState> domain_;
    std::unique_ptr<const FramePayload> payload_;
public:
    const FrameContext context;
    FrameLease(std::shared_ptr<LeaseState> domain,std::unique_ptr<const FramePayload> payload,FrameContext ctx)
        :domain_(std::move(domain)),payload_(std::move(payload)),context(ctx) {
        if(!payload_ || domain_->live) throw std::logic_error("Frame lease already active or empty");
        ++domain_->live;
    }
    ~FrameLease(){ if(domain_) --domain_->live; }
    FrameLease(const FrameLease&)=delete;
    FrameLease& operator=(const FrameLease&)=delete;
    FrameLease(FrameLease&&)=delete;
    FrameLease& operator=(FrameLease&&)=delete;
    const FramePayload& Payload() const { return *payload_; }
};
class GameAdapter {
protected:
    std::shared_ptr<LeaseState> leases_=std::make_shared<LeaseState>();
    void RequireNoLease() const { if(leases_->live) throw std::logic_error("Mutation while stereo frame is leased"); }
public:
    virtual ~GameAdapter()=default;
    virtual Capabilities DescribeCapabilities() const=0;
    virtual Error Initialize(HostServices&,const std::string& gameData)=0;
    virtual void StepSimulation(const StepContext&,const TickInput&)=0;
    virtual std::unique_ptr<FrameLease> CaptureFrame(const FrameContext&)=0;
    virtual void RenderView(const FrameLease&,const ViewContext&,const RenderTarget&)=0;
    virtual void Suspend()=0;
    virtual void Resume()=0;
    virtual void OnGraphicsLost()=0;
    virtual Error OnGraphicsRestored()=0;
    virtual void Shutdown()=0;
};
std::unique_ptr<GameAdapter> MakeDiagnosticAdapter();
}
