// Include the implementation to exercise the private callback with isolated
// engine memory and COM tables. No DLL is loaded into a game process.
#include "../src/runtime.cpp"
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
unsigned failures{},checks{},forwards{};
HRESULT WINAPI forwarded(IDirect3DDevice9*) { ++forwards; return S_OK; }
void check(bool condition,const char* name) {
    ++checks;
    std::printf("%s %s\n",condition?"PASS":"FAIL",name);
    if (!condition) ++failures;
}
template<class T> void put(std::uintptr_t address,const T& value) {
    std::memcpy(reinterpret_cast<void*>(address),&value,sizeof(value));
}
struct Chain { std::uintptr_t* table; HWND target; };
struct Device { std::uintptr_t* table; Chain* chain; };
ULONG WINAPI release(IDirect3DSwapChain9*) { return 1; }
HRESULT WINAPI parameters(IDirect3DSwapChain9* self,D3DPRESENT_PARAMETERS* result) {
    *result={};
    result->Windowed=TRUE;
    result->hDeviceWindow=reinterpret_cast<Chain*>(self)->target;
    return S_OK;
}
HRESULT WINAPI swap_chain(IDirect3DDevice9* self,UINT index,IDirect3DSwapChain9** result) {
    if (index) return D3DERR_INVALIDCALL;
    *result=reinterpret_cast<IDirect3DSwapChain9*>(reinterpret_cast<Device*>(self)->chain);
    return S_OK;
}
}
int wmain() {
    std::array<std::uintptr_t,119> device_table{};
    std::array<std::uintptr_t,10> chain_table{};
    device_table[14]=std::bit_cast<std::uintptr_t>(&swap_chain);
    chain_table[2]=std::bit_cast<std::uintptr_t>(&release);
    chain_table[9]=std::bit_cast<std::uintptr_t>(&parameters);
    Chain chain_a{chain_table.data(),reinterpret_cast<HWND>(123)},chain_b{chain_table.data(),reinterpret_cast<HWND>(456)};
    Device a{device_table.data(),&chain_a},b{device_table.data(),&chain_b};
    auto* first=reinterpret_cast<IDirect3DDevice9*>(&a);
    (void)b;
    std::array<std::uint8_t,2048> memory{};
    const auto base=reinterpret_cast<std::uintptr_t>(memory.data());
    esp::Settings settings{};
    settings.layout={0,8,4,16,0,4,8,128,4,0,4,0,16,20,24,28};
    settings.max_distance=3000;
    settings.collect_interval_ms=1000; settings.frame_timeout_ms=2000;
    settings.players_only=true;
    settings.separate_overlay=true;
    const auto objects=base+512,flags=base+256,pool=base+128;
    put(base,reinterpret_cast<std::uintptr_t>(first));
    put(base+4,objects);
    put(base+8,pool);
    put(pool,objects); put(pool+4,flags); put(pool+8,std::int32_t{2});
    put(objects+4,esp::Vec3{0,0,0});
    put(objects+128+4,esp::Vec3{10,0,5});
    put(objects+128+16,std::uintptr_t{1});
    put(objects+128+20,100.0f); put(objects+128+24,100.0f);
    esp::Runtime state;
    state.base=base;
    auto snapshot=std::make_shared<const esp::Settings>(settings);
    state.settings.store(snapshot);
    esp::runtime.store(&state);
    state.original_end=forwarded;
    esp::on_end_scene(first);
    check(state.latest && state.latest->frame && state.latest->frame->entities.size()==1,
          "real EndScene callback collects a player in the isolated fixture");
    const auto earlier=state.latest;
    const auto samples=state.collections.load();
    put(base+16+12*sizeof(float),.75f);
    put(objects+128+4,esp::Vec3{20,0,5});
    esp::on_end_scene(first);
    check(state.latest!=earlier && state.latest->frame==earlier->frame && state.collections==samples,
          "a camera-only frame reuses player data without a second collection");
    check(state.latest->view[12]==.75f && earlier->view[12]==0,
          "camera changes immediately without mutating a snapshot in use by presenter");
    check(state.latest->collected_at==earlier->collected_at && state.latest->timestamp>=state.latest->collected_at,
          "publication and pose collection keep separate clocks");
    state.settings.store(std::make_shared<const esp::Settings>(settings));
    esp::on_end_scene(first);
    check(state.collections==samples+1 && state.latest->frame->entities[0].position.x==20,
          "configuration invalidation immediately refreshes pose data");
    put(base+16,std::numeric_limits<float>::quiet_NaN());
    esp::on_end_scene(first);
    check(!state.latest && state.enabled,"invalid camera removes the frame without silently disabling ESP");
    put(base+16,0.0f);
    esp::on_end_scene(first);
    check(state.latest && state.latest->frame->entities.size()==1,"valid camera resumes publication");
    state.enabled.store(false);
    esp::on_end_scene(first);
    check(!state.latest && forwards==6,"disabled option clears frame while original EndScene always executes");
    esp::runtime.store(nullptr);
    std::printf("%u checks, %u failures. Synthetic memory and COM; no game process used.\n",checks,failures);
    return failures?1:0;
}
