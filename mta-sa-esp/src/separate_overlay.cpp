#include "separate_overlay.hpp"

namespace esp {
namespace {
constexpr wchar_t overlay_class[]=L"MtaEspSeparateOverlay";
constexpr COLORREF transparent_key=RGB(0,0,0);
constexpr DWORD clear_color=0xff000000;
constexpr LONG maximum_dimension=16384;

bool failed(const wchar_t* operation,HRESULT result,std::wstring& error) {
    error=std::wstring(operation)+L"; HRESULT="+std::to_wstring(static_cast<unsigned long>(result));
    return false;
}
}
SeparateOverlay::~SeparateOverlay() {
    release();
}
void SeparateOverlay::release() noexcept {
    hide();
    renderer_.reset();
    device_.reset();
    d3d_.reset();
    if (window_) DestroyWindow(window_);
    if (atom_) UnregisterClassW(overlay_class,instance_);
    window_=nullptr;
    atom_=0;
    parameters_={};
    position_={};
}
LRESULT CALLBACK SeparateOverlay::window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    if (message==WM_NCHITTEST) return HTTRANSPARENT;
    if (message==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message==WM_ERASEBKGND) return 1;
    return DefWindowProcW(window,message,wparam,lparam);
}
bool SeparateOverlay::create(HWND owner,std::wstring& error) {
    if (window_ && (!IsWindow(window_) || GetWindow(window_,GW_OWNER)!=owner)) release();
    if (window_ && d3d_.get()) return true;
    if (window_ || atom_) release();
    WNDCLASSW wc{};
    wc.lpfnWndProc=window_proc;
    wc.hInstance=instance_;
    wc.lpszClassName=overlay_class;
    atom_=RegisterClassW(&wc);
    if (!atom_) { error=L"RegisterClassW do overlay: "+std::to_wstring(GetLastError()); return false; }
    window_=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
                            overlay_class,L"MTA ESP overlay",WS_POPUP,0,0,1,1,owner,nullptr,instance_,nullptr);
    if (!window_) { error=L"CreateWindowExW do overlay: "+std::to_wstring(GetLastError()); return false; }
    if (!SetLayeredWindowAttributes(window_,transparent_key,255,LWA_COLORKEY)) {
        error=L"SetLayeredWindowAttributes: "+std::to_wstring(GetLastError()); return false;
    }
    // A dedicated 9Ex factory does not enter the game's Direct3DCreate9 proxy setup.
    const HRESULT factory_result=Direct3DCreate9Ex(D3D_SDK_VERSION,d3d_.put());
    if (FAILED(factory_result)) return failed(L"Direct3DCreate9Ex do overlay",factory_result,error);
    return true;
}
void SeparateOverlay::hide() noexcept {
    if (window_) ShowWindow(window_,SW_HIDE);
}
void SeparateOverlay::pump() noexcept {
    if (!window_) return;
    MSG message{};
    while (PeekMessageW(&message,window_,0,0,PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}
bool SeparateOverlay::resize(UINT width,UINT height,std::wstring& error) {
    if (device_.get()) {
        const HRESULT cooperative=device_->CheckDeviceState(window_);
        if (cooperative==S_PRESENT_MODE_CHANGED) parameters_.BackBufferWidth=0;
        else if (cooperative==D3DERR_DEVICELOST) { hide(); return true; }
        else if (FAILED(cooperative)) return failed(L"CheckDeviceState do overlay",cooperative,error);
        if (parameters_.BackBufferWidth==width && parameters_.BackBufferHeight==height) return true;
        renderer_.reset();
        auto next=parameters_;
        next.BackBufferWidth=width;
        next.BackBufferHeight=height;
        const HRESULT reset=device_->ResetEx(&next,nullptr);
        if (reset==D3DERR_DEVICELOST) { hide(); return true; }
        if (FAILED(reset)) return failed(L"Reset do overlay",reset,error);
        parameters_=next;
        return true;
    }
    parameters_={};
    parameters_.Windowed=TRUE;
    parameters_.hDeviceWindow=window_;
    parameters_.SwapEffect=D3DSWAPEFFECT_DISCARD;
    parameters_.BackBufferFormat=D3DFMT_A8R8G8B8;
    parameters_.BackBufferWidth=width;
    parameters_.BackBufferHeight=height;
    parameters_.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    const HRESULT result=d3d_->CreateDeviceEx(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window_,
                         D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE|D3DCREATE_NOWINDOWCHANGES,
                         &parameters_,nullptr,device_.put());
    if (FAILED(result)) return failed(L"CreateDevice do overlay",result,error);
    return true;
}
bool SeparateOverlay::present(HWND target,const Frame& frame,const Settings& settings,std::wstring& error,const ViewMatrix* camera,const AimVisual* aim) {
    error.clear();
    // The runtime owns foreground policy; this presenter validates window geometry.
    if (!IsWindow(target) || !IsWindowVisible(target) || IsIconic(target)) {
        hide(); return true;
    }
    RECT client{};
    POINT origin{};
    if (!GetClientRect(target,&client) || !ClientToScreen(target,&origin)) {
        hide(); error=L"Nao foi possivel consultar a area cliente do jogo"; return false;
    }
    const LONG width=client.right-client.left,height=client.bottom-client.top;
    if (width<=0 || height<=0 || width>maximum_dimension || height>maximum_dimension) { hide(); return true; }
    const HWND owner=GetAncestor(target,GA_ROOT);
    DWORD owner_pid{};
    if (!owner || !GetWindowThreadProcessId(owner,&owner_pid) || owner_pid!=GetCurrentProcessId()) {
        hide(); error=L"Janela proprietaria do overlay indisponivel ou de outro processo"; return false;
    }
    if (!create(owner,error)) { release(); return false; }
    pump();
    // Keep the window hidden until its first complete frame has been presented.
    if (position_.left!=origin.x || position_.top!=origin.y || position_.right!=width || position_.bottom!=height) {
        if (!SetWindowPos(window_,HWND_TOPMOST,origin.x,origin.y,width,height,SWP_NOACTIVATE)) {
            hide(); error=L"SetWindowPos do overlay: "+std::to_wstring(GetLastError()); return false;
        }
        position_={origin.x,origin.y,width,height};
    }
    if (!resize(static_cast<UINT>(width),static_cast<UINT>(height),error)) { hide(); return false; }
    if (!window_above(window_,target) && !SetWindowPos(window_,HWND_TOPMOST,0,0,0,0,
                                                    SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)) {
        hide(); error=L"Restaurar ordem do overlay: "+std::to_wstring(GetLastError()); return false;
    }
    HRESULT result=device_->Clear(0,nullptr,D3DCLEAR_TARGET,clear_color,1,0);
    if (SUCCEEDED(result)) result=device_->BeginScene();
    if (FAILED(result)) {
        hide();
        return result==D3DERR_DEVICELOST || failed(L"BeginScene/Clear do overlay",result,error);
    }
    bool drawn=false;
    try { drawn=renderer_.draw(device_.get(),frame,settings,DeviceOwnership::Exclusive,camera,aim); }
    catch (...) { device_->EndScene(); hide(); throw; }
    result=device_->EndScene();
    if (!drawn || FAILED(result)) {
        hide();
        if (device_->CheckDeviceState(window_)==D3DERR_DEVICELOST) return true;
        return failed(L"Desenho do overlay",FAILED(result)?result:E_FAIL,error);
    }
    result=device_->PresentEx(nullptr,nullptr,nullptr,nullptr,0);
    if (result==D3DERR_DEVICELOST) { hide(); return true; }
    if (FAILED(result)) { hide(); return failed(L"Present do overlay",result,error); }
    ShowWindow(window_,SW_SHOWNOACTIVATE);
    return true;
}
}
