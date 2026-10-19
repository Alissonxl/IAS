#include "separate_overlay.hpp"
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace {
int failures{};
void check(bool condition,const char* label) {
    std::printf("%s %s\n",condition?"PASS":"FAIL",label);
    if (!condition) ++failures;
}
std::vector<unsigned char> capture(IDirect3DDevice9* device) {
    esp::ComPtr<IDirect3DSurface9> buffer,copy;
    D3DSURFACE_DESC desc{};
    if (FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,buffer.put())) || FAILED(buffer->GetDesc(&desc)) ||
        FAILED(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,copy.put(),nullptr)) ||
        FAILED(device->GetRenderTargetData(buffer.get(),copy.get()))) return {};
    D3DLOCKED_RECT pixels{};
    if (FAILED(copy->LockRect(&pixels,nullptr,D3DLOCK_READONLY))) return {};
    std::vector<unsigned char> result(static_cast<std::size_t>(desc.Width)*desc.Height*4);
    for (UINT y=0;y<desc.Height;++y)
        std::memcpy(result.data()+static_cast<std::size_t>(y)*desc.Width*4,
                    static_cast<const unsigned char*>(pixels.pBits)+y*pixels.Pitch,desc.Width*4);
    copy->UnlockRect();
    return result;
}
}
int wmain(int argc,wchar_t** argv) {
    const bool ownership_only=argc==3 && std::wcscmp(argv[2],L"--ownership-only")==0;
    if (argc!=2 && !ownership_only) return 2;
    esp::Settings settings{};
    std::wstring error;
    if (!esp::load_settings(argv[1],settings,error)) return 3;
    const HINSTANCE instance=GetModuleHandleW(nullptr);
    const HWND foreground=GetForegroundWindow();
    WNDCLASSW wc{};
    wc.lpfnWndProc=DefWindowProcW;
    wc.hInstance=instance;
    wc.lpszClassName=L"MtaOverlayCaptureChecks";
    if (!RegisterClassW(&wc)) return 4;
    const HWND target=CreateWindowW(wc.lpszClassName,L"Overlay capture test",WS_OVERLAPPEDWINDOW,
                                   100,100,640,480,nullptr,nullptr,instance,nullptr);
    if (!target) return 5;
    HWND overlay_window{};
    {
        esp::ComPtr<IDirect3D9> d3d;
        *d3d.put()=Direct3DCreate9(D3D_SDK_VERSION);
        esp::ComPtr<IDirect3DDevice9> device;
        D3DPRESENT_PARAMETERS pp{};
        pp.Windowed=TRUE;
        pp.SwapEffect=D3DSWAPEFFECT_COPY;
        pp.hDeviceWindow=target;
        pp.BackBufferWidth=640;
        pp.BackBufferHeight=480;
        if (!d3d.get() || FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,target,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.put()))) return 6;
        device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff19354b,1,0);
        device->Present(nullptr,nullptr,nullptr,nullptr);
        const auto before=capture(device.get());
        check(!before.empty(),"capture game fixture backbuffer before overlay");
        esp::SeparateOverlay overlay(instance);
        esp::Frame frame{};
        frame.view[0]=.1f; frame.view[9]=-.1f;
        frame.view[12]=.5f; frame.view[13]=.5f; frame.view[14]=1;
        esp::Entity entity{};
        entity.health=50; entity.max_health=100; entity.distance=25;
        entity.name=L"Capture_Test";
        frame.entities.push_back(entity);
        check(overlay.present(target,frame,settings,error) && !overlay.window(),"hidden target never creates a visible overlay");
        ShowWindow(target,SW_SHOWNOACTIVATE);
        MSG message{};
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) DispatchMessageW(&message);
        if (!ownership_only) {
            SetForegroundWindow(target);
            check(GetForegroundWindow()==target,"fixture establishes its own foreground before testing focus preservation");
        }
        const HWND before_present_foreground=GetForegroundWindow();
        check(overlay.present(target,frame,settings,error),"render through independent D3D9Ex device");
        if (!error.empty()) std::fwprintf(stderr,L"%ls\n",error.c_str());
        check(overlay.window() && IsWindowVisible(overlay.window()),"separate overlay becomes visible after presentation");
        overlay_window=overlay.window();
        std::printf("Focus diagnostic: before=%p after=%p target=%p overlay=%p\n",
                    static_cast<void*>(before_present_foreground),static_cast<void*>(GetForegroundWindow()),
                    static_cast<void*>(target),static_cast<void*>(overlay.window()));
        if (!ownership_only) check(GetForegroundWindow()==before_present_foreground,"overlay presentation never steals foreground");
        check(GetWindow(overlay.window(),GW_OWNER)==target,"overlay is owned by the target window from creation");
        if (overlay.window()) {
            const auto style=static_cast<DWORD>(GetWindowLongPtrW(overlay.window(),GWL_EXSTYLE));
            check((style&(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE))==
                        (WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE),"layered window does not capture input or activate");
            check(SendMessageW(overlay.window(),WM_NCHITTEST,0,0)==HTTRANSPARENT,"overlay hit testing passes through");
        }
        const auto after=capture(device.get());
        check(overlay.window() && IsWindowVisible(overlay.window()) && !after.empty() && before==after,
              "game backbuffer capture remains byte-identical while overlay is visible");
        SetWindowPos(target,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        check(esp::window_above(overlay.window(),target),"promoting the game cannot cover its owned overlay before another presentation");
        bool stays_above=true;
        for (unsigned i=0;i<16;++i) {
            SetWindowPos(target,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
            stays_above=stays_above && esp::window_above(overlay.window(),target);
        }
        check(stays_above,"repeated game promotions keep overlay above without worker repositioning");
        check(overlay.present(target,frame,settings,error) && esp::window_above(overlay.window(),target),
              "overlay recovers its order without a size or position change");
        if (!ownership_only) check(GetForegroundWindow()==before_present_foreground,"restoring overlay order preserves focus");
        SetWindowPos(target,nullptr,180,140,800,600,SWP_NOZORDER|SWP_NOACTIVATE);
        check(overlay.present(target,frame,settings,error),"overlay resizes its own swapchain");
        RECT client{},bounds{};
        POINT origin{};
        GetClientRect(target,&client); ClientToScreen(target,&origin);
        if (overlay.window()) GetWindowRect(overlay.window(),&bounds);
        check(bounds.left==origin.x && bounds.top==origin.y && bounds.right-bounds.left==client.right &&
              bounds.bottom-bounds.top==client.bottom,"overlay follows target client rectangle exactly");
        ShowWindow(target,SW_MINIMIZE);
        check(overlay.present(target,frame,settings,error) && !IsWindowVisible(overlay.window()),"minimized target hides overlay");
        ShowWindow(target,SW_SHOWNOACTIVATE);
        ShowWindow(target,SW_RESTORE);
        check(overlay.present(target,frame,settings,error) && IsWindowVisible(overlay.window()),"overlay recovers after restore");
        check(overlay.present(nullptr,frame,settings,error) && !IsWindowVisible(overlay.window()),"invalid target hides overlay");
    }
    check(overlay_window && !IsWindow(overlay_window),"overlay window destroyed during ordered teardown");
    DestroyWindow(target);
    UnregisterClassW(wc.lpszClassName,instance);
    if (IsWindow(foreground)) SetForegroundWindow(foreground);
    if (ownership_only) std::printf("Ownership-only mode: foreground preservation was not tested.\n");
    return failures?1:0;
}
