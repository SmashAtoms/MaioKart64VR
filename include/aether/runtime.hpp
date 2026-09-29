#pragma once
#include "aether/adapter.hpp"
#include <array>
#include <deque>
#include <map>
#include <set>

namespace aether {
class InputQueue {
    InputSample current_{};
    struct Edge { uint32_t pressed,released; };
    std::deque<Edge> edges_;
public:
    void Sample(InputSample input);
    TickInput Consume();
    void Clear();
};
struct Schedule { std::vector<StepContext> steps; float alpha=0; uint64_t tick=0; bool discontinuity=false; };
class FixedScheduler {
    Time last_=0,accumulator_=0,simulation_=0;
    int hz_;
    uint64_t tick_=0;
    bool based_=false;
public:
    explicit FixedScheduler(int hz=30);
    void Rebase(Time displayTime);
    Schedule Advance(Time displayTime);
};
enum class SessionPhase { Idle, Ready, Running, Stopping, Lost, Exiting };
class Lifecycle {
public:
    SessionPhase phase=SessionPhase::Idle;
    bool focused=false, tracking=false, controllers=false, androidResumed=false;
    bool paused=true;
    bool MayRunFrames() const { return phase==SessionPhase::Running; }
    bool MayRender(bool shouldRender) const { return MayRunFrames()&&shouldRender&&tracking; }
    bool MayResume() const { return MayRunFrames()&&focused&&tracking&&controllers&&androidResumed; }
    void Observe(bool focus,bool tracked,bool inputActive,bool resumed);
    bool Resume();
};
// Per-eye sidecars for replay. Never write through display-list completion pointers.
class ReplayState {
    uint64_t frame_=0;
    std::array<std::set<uintptr_t>,2> copies_;
public:
    void Begin(uint64_t frame);
    bool CopyOnce(unsigned eye,uintptr_t commandIdentity);
};
class TargetRouter {
    RenderTarget final_{};
    std::map<std::pair<unsigned,int>,RenderTarget> effects_;
    unsigned eye_=0;
public:
    void BeginView(unsigned eye,RenderTarget finalTarget);
    void RegisterEffect(unsigned eye,int logicalId,RenderTarget target);
    const RenderTarget& Resolve(int logicalId) const;
    const RenderTarget& Reset() const { return final_; }
    void GraphicsLost() { effects_.clear();final_={}; }
};
}
