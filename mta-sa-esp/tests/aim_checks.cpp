#include "aim.hpp"
#include "crosshair.hpp"
#include <cstdio>
#include <limits>
namespace {
unsigned checks{},failures{};
void check(bool value,const char* text) { ++checks; if (!value) ++failures; std::printf("%s %s\n",value?"PASS":"FAIL",text); }
esp::Entity entity(std::uint64_t id,float x) {
    esp::Entity e; e.identity=id; e.position={x,.5f,1}; e.health=100; e.distance=10;
    e.aim_eligible=e.vehicle_known=true; e.visibility=esp::Visibility::Visible;
    for (std::size_t i:{0U,2U,4U}) { e.skeleton.valid[i]=true; e.skeleton.positions[i]={x,.5f,1}; }
    return e;
}
}
int main() {
    esp::Settings s{}; s.max_distance=1000; s.near_clip=.01f; s.aim.enabled=true; s.aim.fov=25;
    esp::ViewMatrix view{}; view[0]=view[5]=view[14]=1;
    esp::Frame f; f.aim_entities={entity(1,.6f),entity(2,.7f)};
    esp::AimController c;
    auto r=c.update(f,view,s,100,100,1000,1000,false,false,true);
    check(r.visual.selected && r.visual.world.x==.6f && !r.active,"nearest eligible bone is highlighted before activation");
    r=c.update(f,view,s,100,100,1000,1016,true,false,true);
    check(r.active && r.dx>0 && r.dy==0,"held key produces only movement toward selected point");
    check(r.dx<=static_cast<long>(s.aim.max_step),"movement is bounded per update");
    f.aim_entities[0]=entity(1,.72f); f.aim_entities[1]=entity(2,.52f);
    r=c.update(f,view,s,100,100,1032,1032,true,false,true);
    check(r.visual.world.x==.72f,"lock preserves the same live target when another becomes closer");
    r=c.update(f,view,s,100,100,1048,1048,true,true,true);
    check(r.visual.world.x==.52f,"switch-key edge cycles to another eligible target");
    f.aim_entities[1].health=0;
    r=c.update(f,view,s,100,100,1064,1064,true,false,true);
    check(r.visual.world.x==.72f,"dead locked target is discarded immediately");
    r=c.update(f,view,s,100,100,1064,1080,true,false,false);
    check(!r.active && !r.dx && !r.visual.selected,"panel and focus gate suppress both movement and target state");
    r=c.update(f,view,s,100,100,1064,1096,true,false,true);
    check(!r.active,"a held menu-click cannot start aim immediately after closing");
    c.update(f,view,s,100,100,1100,1100,false,false,true);
    r=c.update(f,view,s,100,100,1116,1116,true,false,true);
    check(r.active,"releasing and pressing again arms aim after closing panel");
    r=c.update(f,view,s,100,100,1000,2000,true,false,true);
    check(!r.active && !r.visual.selected,"expired pose data cannot drive mouse input");
    auto candidate=[&](esp::Entity e) { esp::Frame scene; scene.aim_entities.push_back(e); esp::AimController controller; return controller.update(scene,view,s,100,100,1000,1000,false,false,true); };
    auto e=entity(10,.6f); e.visibility=esp::Visibility::Unknown;
    check(!candidate(e).visual.selected,"visibility check rejects unknown visibility");
    e.visibility=esp::Visibility::Occluded;
    check(!candidate(e).visual.selected,"visibility check rejects occluded targets");
    s.aim.visible_check=false; check(candidate(e).visual.selected,"disabling visibility check accepts known streamed occluded target");
    s.aim.visible_check=true; e=entity(10,.6f); e.in_vehicle=true;
    check(!candidate(e).visual.selected,"vehicle filter rejects an occupied ped");
    s.aim.in_car=true; check(candidate(e).visual.selected,"vehicle option permits an occupied ped");
    s.aim.in_car=false; e=entity(10,.6f); e.vehicle_known=false;
    check(!candidate(e).visual.selected,"unknown vehicle state fails closed when filtering vehicles");
    e=entity(10,.6f); e.is_player=false;
    check(!candidate(e).visual.selected,"NPCs are independently disabled by default");
    s.aim.peds=true; check(candidate(e).visual.selected,"NPC switch enables eligible NPC targets");
    s.aim.players=false; check(!candidate(entity(11,.6f)).visual.selected,"player switch excludes players independently");
    s.aim.players=true; e=entity(10,.6f); e.aim_eligible=false;
    check(!candidate(e).visual.selected,"unverified dimension metadata cannot drive aim");
    e=entity(10,.6f); e.streamed=false; check(!candidate(e).visual.selected,"cached unstreamed records cannot be aimed at");
    e=entity(10,.6f); e.skeleton.valid[0]=false;
    check(!candidate(e).visual.selected,"missing selected bone does not use a fabricated position");
    s.aim.bone=1; check(candidate(e).visual.selected,"chest selection uses its own validated bone");
    s.aim.bone=2; check(candidate(e).visual.selected,"pelvis selection uses its own validated bone");
    s.aim.bone=0; e=entity(10,.95f); check(!candidate(e).visual.selected,"targets outside configured pixel FOV are rejected");
    s.aim.mode=1; esp::AimController toggle;
    toggle.update(f,view,s,100,100,1000,1000,false,false,true);
    r=toggle.update(f,view,s,100,100,1016,1016,true,false,true); check(r.active,"toggle mode starts on key edge");
    r=toggle.update(f,view,s,100,100,1032,1032,true,false,true); check(r.active,"holding toggle key does not toggle repeatedly");
    toggle.update(f,view,s,100,100,1048,1048,false,false,true);
    r=toggle.update(f,view,s,100,100,1064,1064,true,false,true); check(!r.active,"next toggle key edge stops aim");
    s.aim.mode=0; s.aim.prediction_ms=100; esp::AimController prediction;
    f.aim_entities={entity(20,.6f)}; prediction.update(f,view,s,100,100,1000,1000,false,false,true);
    f.aim_entities={entity(20,.62f)}; r=prediction.update(f,view,s,100,100,1100,1100,false,false,true);
    check(std::abs(r.visual.world.x-.64f)<.001f,"prediction uses observed velocity and explicit milliseconds");
    s.aim.bone=1;
    f.aim_entities[0].skeleton.positions[2]={.55f,.5f,1};
    r=prediction.update(f,view,s,100,100,1120,1120,false,false,true);
    check(r.visual.selected && std::abs(r.visual.world.x-.55f)<.001f,
          "changing bone starts a fresh velocity history instead of predicting a head-to-chest jump");
    s.aim.bone=0; s.aim.mode=1; s.aim.key=VK_LBUTTON;
    esp::AimController reconfigured;
    reconfigured.update(f,view,s,100,100,2000,2000,false,false,true);
    r=reconfigured.update(f,view,s,100,100,2016,2016,true,false,true);
    check(r.active,"toggle is active before testing a mode change");
    s.aim.mode=0;
    r=reconfigured.update(f,view,s,100,100,2032,2032,true,false,true);
    check(!r.active && !r.dx,"changing activation mode requires release rather than carrying an active latch");
    reconfigured.update(f,view,s,100,100,2048,2048,false,false,true);
    s.aim.key=VK_RBUTTON;
    r=reconfigured.update(f,view,s,100,100,2064,2064,true,false,true);
    check(!r.active && !r.dx,"rebinding to an already held key cannot start mouse movement");
    s.aim.lock=false; f.aim_entities={entity(90,.7f),entity(91,.52f),entity(92,.6f)};
    esp::AimController nearest;
    r=nearest.update(f,view,s,100,100,3000,3000,false,false,true);
    check(r.visual.selected && r.visual.world.x==.52f,"linear selection finds nearest candidate regardless of input order");
    s.aim.enabled=true; s.aim.fov=3; s.aim.mode=0; s.aim.prediction_ms=0;
    esp::Frame offset_frame;
    auto crosshair_target=entity(400,.7f); crosshair_target.skeleton.positions[0]={.7f,.3f,1};
    offset_frame.aim_entities={entity(401,.5f),crosshair_target};
    esp::AimController offset_controller;
    r=offset_controller.update(offset_frame,view,s,100,100,4000,4000,false,false,true,{.7f,.3f});
    check(r.visual.selected && r.visual.world.x==.7f && r.visual.world.y==.3f,
          "FOV candidates are measured around actual HUD crosshair instead of window center");
    r=offset_controller.update(offset_frame,view,s,100,100,4016,4016,true,false,true,{.7f,.3f});
    check(r.active && !r.dx && !r.dy && r.visual.center.x==.7f && r.visual.center.y==.3f,
          "a bone already on the offset crosshair produces zero correction");
    r=offset_controller.update(offset_frame,view,s,1920,1080,4032,4032,true,false,true,{.7f,.3f});
    check(r.visual.selected && !r.dx && !r.dy,"normalized crosshair stays aligned across resolution changes");
    r=offset_controller.update(offset_frame,view,s,100,100,4048,4048,true,false,true,{0,.5f});
    check(!r.active && !r.visual.center_valid,"invalid HUD crosshair cannot drive input");
    {
        std::array<unsigned char,30> probe{};
        const std::array<unsigned char,30> shape{0xdb,0x05,0,0,0,0,0x8b,0xcf,0xd8,0x0d,0,0,0,0,
            0xd9,0x5c,0x24,0x14,0xdb,0x05,0,0,0,0,0xd8,0x0d,0,0,0,0};
        probe=shape;
        float x=.53f,y=.4f; std::uint8_t active=1;
        std::array<unsigned char,0x238*3> modes{};
        esp::AimSettings layout;
        const auto address=[](const void* p){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));};
        layout.crosshair_x=address(&x); layout.crosshair_y=address(&y); layout.active_camera=address(&active);
        layout.camera_modes=address(modes.data()); layout.camera_stride=0x238; layout.crosshair_probe=address(probe.data());
        std::memcpy(probe.data()+10,&layout.crosshair_x,4); std::memcpy(probe.data()+26,&layout.crosshair_y,4);
        const auto mode=[&](std::uint16_t value){std::memcpy(modes.data()+active*layout.camera_stride,&value,2);};
        mode(53); auto anchor=esp::read_crosshair(0,layout);
        check(anchor.valid && anchor.ratio.x==x && anchor.ratio.y==y,"shoulder camera reads HUD multipliers from active camera slot");
        mode(55); check(esp::read_crosshair(0,layout).ratio.x==x,"vehicle aiming preserves HUD anchor");
        mode(65); check(esp::read_crosshair(0,layout).ratio.y==y,"attached aiming preserves HUD anchor");
        mode(7); anchor=esp::read_crosshair(0,layout);
        check(anchor.valid && anchor.ratio.x==.5f && anchor.ratio.y==.5f,"sniper camera uses centered scope instead of shoulder offset");
        mode(53); y=std::numeric_limits<float>::quiet_NaN();
        check(!esp::read_crosshair(0,layout).valid,"nonfinite crosshair multiplier is rejected"); y=.4f;
        probe[10]^=1; check(!esp::read_crosshair(0,layout).valid,"HUD operand mismatch fails closed"); probe[10]^=1;
        active=3; check(!esp::read_crosshair(0,layout).valid,"invalid active camera slot is rejected");
    }
    {
        auto tap_settings=s; tap_settings.aim.mode=2; tap_settings.aim.enabled=true;
        tap_settings.aim.fov=70; tap_settings.aim.smooth=2; tap_settings.aim.prediction_ms=0;
        auto ordinary=tap_settings; ordinary.aim.mode=0;
        esp::Frame scene; scene.aim_entities={entity(600,.7f)};
        esp::AimController tap,normal;
        tap.update(scene,view,tap_settings,100,100,5000,5000,false,false,true);
        normal.update(scene,view,ordinary,100,100,5000,5000,false,false,true);
        const auto fast=tap.update(scene,view,tap_settings,100,100,5016,5016,true,false,true);
        const auto slow=normal.update(scene,view,ordinary,100,100,5016,5016,true,false,true);
        check(fast.active && fast.dx>slow.dx && fast.dx<=static_cast<long>(tap_settings.aim.max_step),
              "manual Tap acquires a distant bone faster without removing the movement limit");
        scene.aim_entities={entity(600,.505f)};
        auto settled=tap.update(scene,view,tap_settings,100,100,5032,5032,true,false,true);
        check(settled.visual.aligned && !settled.dx && !settled.dy,"Tap stops correction inside alignment tolerance");
        scene.aim_entities={entity(600,.511f)};
        settled=tap.update(scene,view,tap_settings,100,100,5048,5048,true,false,true);
        check(settled.visual.aligned && !settled.dx,"Tap hysteresis ignores small jitter around an aligned bone");
        scene.aim_entities={entity(600,.54f)};
        settled=tap.update(scene,view,tap_settings,100,100,5064,5064,true,false,true);
        check(!settled.visual.aligned && settled.dx>0,"Tap resumes correction when target leaves alignment band");
        settled=tap.update(scene,view,tap_settings,100,100,5080,5080,true,false,true,{.5f,.5f},false);
        check(!settled.active && !settled.dx && !settled.visual.selected,"manual weapon stow pauses Tap camera movement");
        settled=tap.update(scene,view,tap_settings,100,100,5096,5096,true,false,true,{.5f,.5f},true);
        check(settled.active,"manual return to aiming resumes Tap without requiring an extra key press");
        tap.update(scene,view,tap_settings,100,100,5112,5112,true,false,false);
        settled=tap.update(scene,view,tap_settings,100,100,5128,5128,true,false,true);
        check(!settled.active && !settled.dx,"opening a menu still requires releasing the activation key before Tap resumes");
    }
    s.aim.enabled=false; check(!candidate(entity(22,.6f)).visual.selected,"disabled master produces no target or movement");
    std::printf("%u checks, %u failures. Synthetic poses; no input sent.\n",checks,failures);
    return failures?1:0;
}
