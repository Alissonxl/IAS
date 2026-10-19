#pragma once
namespace esp {
// Own only the temporary cursor state; preserve an existing menu's state on release.
class CursorLease {
public:
    template<class Core> void update(Core& core,bool wanted) {
        if (wanted && !held_) {
            visible_=core.visible(); controls_=core.controls();
            core.force(true,true); held_=true;
        } else if (!wanted && held_) {
            if (core.visible() && core.controls()) core.force(visible_,controls_);
            held_=false;
        }
    }
    void abandon() noexcept { held_=false; }
    bool held() const noexcept { return held_; }
private:
    bool held_{},visible_{},controls_{};
};
}
