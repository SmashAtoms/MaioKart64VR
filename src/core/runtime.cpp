#include "aether/runtime.hpp"
#include <algorithm>
#include <cmath>

namespace aether {
void InputQueue::Sample(InputSample input) {
    if(!input.active){Clear();return;}
    input.steering=std::isfinite(input.steering)?std::clamp(input.steering,-1.f,1.f):0;
    input.vertical=std::isfinite(input.vertical)?std::clamp(input.vertical,-1.f,1.f):0;
    const uint32_t pressed=input.held&~current_.held,released=current_.held&~input.held;
    if(pressed||released) edges_.push_back({pressed,released});
    current_=input;
}
TickInput InputQueue::Consume() {
    TickInput out;static_cast<InputSample&>(out)=current_;
    // Coalesce release after a short press, but preserve a second press for the next tick.
    while(!edges_.empty()) {
        auto edge=edges_.front();
        if((out.pressed&edge.pressed)||(out.released&edge.released)) break;
        out.pressed|=edge.pressed;out.released|=edge.released;edges_.pop_front();
    }
    return out;
}
void InputQueue::Clear(){current_={};edges_.clear();}
FixedScheduler::FixedScheduler(int hz):hz_(hz){if(hz<1||hz>1000)throw std::invalid_argument("Invalid simulation rate");}
void FixedScheduler::Rebase(Time t){last_=t;accumulator_=0;based_=true;}
Schedule FixedScheduler::Advance(Time t) {
    Schedule out;out.tick=tick_;
    if(!based_){Rebase(t);return out;}
    Time elapsed=t-last_;last_=t;
    // A suspension/clock jump is a recovery event, never silently simulate background time.
    if(elapsed<0||elapsed>250000000){accumulator_=0;out.discontinuity=true;return out;}
    // Accumulate nanoseconds multiplied by Hz, avoiding drift from rounded 1/30 s.
    accumulator_+=elapsed*hz_;
    while(accumulator_>=1000000000) {
        accumulator_-=1000000000;
        Time next=static_cast<Time>((tick_+1)*1000000000ULL/static_cast<uint64_t>(hz_));
        out.steps.push_back({++tick_,simulation_,next-simulation_});simulation_=next;
    }
    out.tick=tick_;out.alpha=static_cast<float>(accumulator_/1000000000.0);
    return out;
}
void Lifecycle::Observe(bool f,bool t,bool c,bool r){
    focused=f;tracking=t;controllers=c;androidResumed=r;
    if(!MayResume()) paused=true;
}
bool Lifecycle::Resume(){if(!MayResume())return false;paused=false;return true;}
void ReplayState::Begin(uint64_t f){if(frame_!=f){frame_=f;for(auto& c:copies_)c.clear();}}
bool ReplayState::CopyOnce(unsigned eye,uintptr_t id){if(eye>1)throw std::out_of_range("Eye");return copies_[eye].insert(id).second;}
void TargetRouter::BeginView(unsigned eye,RenderTarget target){
    if(eye>1||target.width<=0||target.height<=0||target.samples==0)throw std::invalid_argument("Invalid target");
    eye_=eye;final_=target;
}
void TargetRouter::RegisterEffect(unsigned eye,int id,RenderTarget target){
    if(eye>1||id<=0)throw std::invalid_argument("Invalid effect identity");effects_[{eye,id}]=target;
}
const RenderTarget& TargetRouter::Resolve(int id)const {
    if(id==0)return final_;
    auto it=effects_.find({eye_,id});if(it==effects_.end())throw std::out_of_range("Unregistered eye effect");
    return it->second;
}
}
