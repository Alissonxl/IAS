#include "cursor_lease.hpp"
#include <cstdio>
struct Core {
    bool shown{},blocked{}; unsigned calls{};
    bool visible() const { return shown; }
    bool controls() const { return blocked; }
    void force(bool visible,bool controls) { shown=visible; blocked=controls; ++calls; }
};
int main() {
    unsigned failures=0,checks=0;
    const auto check=[&](bool value,const char* text) { ++checks; if (!value) ++failures; std::printf("%s %s\n",value?"PASS":"FAIL",text); };
    Core core; esp::CursorLease lease;
    lease.update(core,false); check(core.calls==0,"closed panel leaves game input untouched");
    lease.update(core,true); check(core.shown && core.blocked,"opening requests native MTA cursor and control suppression");
    for (int i=0;i<100;++i) lease.update(core,true);
    check(core.calls==1,"steady open panel does not repeatedly change global MTA cursor state");
    lease.update(core,false); check(!core.shown && !core.blocked && !lease.held(),"closing restores the previous cursor and controls");
    core.shown=true; core.blocked=false; lease.update(core,true); lease.update(core,false);
    check(core.shown && !core.blocked,"pre-existing cursor visibility is preserved");
    core.shown=true; core.blocked=true; lease.update(core,true); lease.update(core,false);
    check(core.shown && core.blocked,"pre-existing game menu keeps controls suspended");
    core.shown=false; core.blocked=false; lease.update(core,true); core.force(false,false); const unsigned calls=core.calls;
    lease.update(core,false); check(core.calls==calls,"another cursor owner's changed state is not overwritten on close");
    lease.update(core,true); lease.abandon(); const unsigned old=core.calls; lease.update(core,false);
    check(core.calls==old,"invalidated interface cannot receive a restoration call");
    lease.update(core,true); lease.update(core,false); check(!lease.held(),"lease can reopen and release after invalidation");
    std::printf("%u checks, %u failures. Mock core; MTA not exercised.\n",checks,failures);
    return failures?1:0;
}
