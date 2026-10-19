#pragma once
#include "game.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace esp {
struct AimStep { AimVisual visual; long dx{},dy{}; bool active{}; };
class AimController {
    struct Sample { Vec3 position{},velocity{}; std::uint64_t time{}; };
    struct Candidate { std::uint64_t id{}; Vec3 world{}; Vec2 screen{}; float score{}; };
public:
    void suspend() noexcept { latched_=armed_=settled_=false; locked_=0; carry_x_=carry_y_=0; }
    AimStep update(const Frame& frame,const ViewMatrix& view,const Settings& settings,float width,float height,
                   std::uint64_t pose_time,std::uint64_t now,bool key,bool next_key,bool permitted,Vec2 origin={.5f,.5f},bool tracking_available=true) {
        const auto& aim=settings.aim;
        AimStep result;
        result.visual.center=origin;
        const bool origin_valid=std::isfinite(origin.x) && std::isfinite(origin.y) &&
            origin.x>0 && origin.x<1 && origin.y>0 && origin.y<1;
        result.visual.center_valid=origin_valid;
        if (!configured_ || last_bone_!=aim.bone) {
            samples_.clear(); locked_=0; settled_=false; carry_x_=carry_y_=0;
        }
        if (configured_ && (last_key_!=aim.key || last_mode_!=aim.mode)) {
            suspend(); was_key_=key; was_next_=next_key;
        }
        configured_=true; last_bone_=aim.bone; last_key_=aim.key; last_mode_=aim.mode;
        const bool rising=key && !was_key_,next_rising=next_key && !was_next_;
        was_key_=key; was_next_=next_key;
        if (!aim.enabled || !permitted || !origin_valid || !std::isfinite(width) || !std::isfinite(height) || width<=0 || height<=0 ||
            now<pose_time || now-pose_time>settings.frame_timeout_ms) {
            suspend(); last_tick_=now; return result;
        }
        if (!key) armed_=true;
        if (aim.mode==1 && rising && armed_) latched_=!latched_;
        result.active=armed_ && (aim.mode==1?latched_:key);
        if (aim.mode==2 && !tracking_available) {
            result.active=false; settled_=false; locked_=0; carry_x_=carry_y_=0; last_tick_=now;
            return result;
        }
        constexpr std::array<std::size_t,3> bones{0,2,4};
        auto& candidates=candidates_;
        candidates.clear();
        candidates.reserve(frame.aim_entities.size());
        for (const auto& entity:frame.aim_entities) {
            if (!entity.identity || !entity.streamed || !entity.aim_eligible || !entity.health_known || entity.health<=0 ||
                (entity.is_player?!aim.players:!aim.peds) || (!aim.in_car && (!entity.vehicle_known || entity.in_vehicle)) ||
                (aim.visible_check && entity.visibility!=Visibility::Visible) || entity.distance>settings.max_distance) continue;
            const auto bone=bones[std::min<std::uint32_t>(aim.bone,2)];
            if (!entity.skeleton.valid[bone]) continue;
            Vec3 world=entity.skeleton.positions[bone];
            if (!finite(world)) continue;
            auto& sample=samples_[entity.identity];
            if (sample.time!=pose_time) {
                Vec3 velocity{};
                if (sample.time && pose_time>sample.time && pose_time-sample.time<=250) {
                    const float dt=static_cast<float>(pose_time-sample.time)*.001f;
                    velocity={(world.x-sample.position.x)/dt,(world.y-sample.position.y)/dt,(world.z-sample.position.z)/dt};
                    if (!finite(velocity) || std::hypot(velocity.x,velocity.y,velocity.z)>120) velocity={};
                }
                sample={world,velocity,pose_time};
            }
            const float seconds=aim.prediction_ms*.001f;
            world.x+=sample.velocity.x*seconds; world.y+=sample.velocity.y*seconds; world.z+=sample.velocity.z*seconds;
            Vec2 screen{};
            if (!project(world,view,width,height,settings.near_clip,screen) || screen.x<0 || screen.x>width || screen.y<0 || screen.y>height) continue;
            const float score=std::hypot(screen.x-width*origin.x,screen.y-height*origin.y);
            if (score>aim.fov) continue;
            candidates.push_back({entity.identity,world,screen,score});
        }
        if (samples_.size()>4096) samples_.clear();
        if (candidates.empty()) { locked_=0; settled_=false; carry_x_=carry_y_=0; last_tick_=now; return result; }
        const auto closer=[](const Candidate& a,const Candidate& b) {
            return a.score==b.score?a.id<b.id:a.score<b.score;
        };
        // Ordinary frames only need the nearest candidate; ordering is needed for cycling.
        if (next_rising) std::sort(candidates.begin(),candidates.end(),closer);
        std::size_t selected=static_cast<std::size_t>(std::min_element(candidates.begin(),candidates.end(),closer)-candidates.begin());
        const auto old=std::find_if(candidates.begin(),candidates.end(),[&](const Candidate& c){return c.id==locked_;});
        if (old!=candidates.end() && ((aim.lock && result.active) || next_rising)) {
            selected=static_cast<std::size_t>(old-candidates.begin());
            if (next_rising) selected=(selected+1)%candidates.size();
        }
        const auto& target=candidates[selected];
        if (locked_!=target.id) { settled_=false; carry_x_=carry_y_=0; }
        locked_=target.id; result.visual={target.world,true,origin,true};
        const auto elapsed=now>=last_tick_?now-last_tick_:0;
        if (!result.active) { settled_=false; carry_x_=carry_y_=0; last_tick_=now; return result; }
        if (aim.mode==2) {
            const float threshold=settled_?aim.tap_resume:aim.tap_tolerance;
            if (target.score<=threshold) {
                settled_=true; result.visual.aligned=true;
                carry_x_=carry_y_=0; last_tick_=now; return result;
            }
            settled_=false;
        }
        if (last_tick_ && elapsed<8) return result;
        const float dt=last_tick_?std::clamp(static_cast<float>(elapsed)*.001f,.001f,.05f):.016f;
        last_tick_=now;
        float response=std::max(aim.smooth,1.0f)*.01f;
        if (aim.mode==2) response*=target.score>8?1/aim.tap_boost:2.0f;
        const float alpha=1-std::exp(-dt/response);
        const auto step=[&](float delta,float& carry) {
            const float amount=std::clamp(delta*alpha*aim.input_gain+carry,-aim.max_step,aim.max_step);
            const long whole=static_cast<long>(std::trunc(amount)); carry=amount-static_cast<float>(whole); return whole;
        };
        result.dx=step(target.screen.x-width*origin.x,carry_x_);
        result.dy=step(target.screen.y-height*origin.y,carry_y_);
        return result;
    }
private:
    std::unordered_map<std::uint64_t,Sample> samples_;
    std::vector<Candidate> candidates_;
    std::uint64_t locked_{},last_tick_{};
    bool was_key_{},was_next_{},latched_{},armed_{};
    bool configured_{};
    bool settled_{};
    std::uint32_t last_bone_{},last_key_{},last_mode_{};
    float carry_x_{},carry_y_{};
};
}
