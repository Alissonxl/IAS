#include "win.hpp"

namespace {
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    if (message==WM_TIMER) { DestroyWindow(window); return 0; }
    if (message==WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window,message,wparam,lparam);
}
}
int wmain() {
    WNDCLASSW type{};
    type.hInstance=GetModuleHandleW(nullptr);
    type.lpfnWndProc=window_proc;
    type.lpszClassName=L"MtaEspIsolatedHookHost";
    if (!RegisterClassW(&type)) return 1;
    const HWND window=CreateWindowW(type.lpszClassName,L"MTA ESP isolated hook host",WS_OVERLAPPEDWINDOW,
                                    0,0,320,200,nullptr,nullptr,type.hInstance,nullptr);
    if (!window) return 1;
    if (!SetTimer(window,1,15000,nullptr)) { DestroyWindow(window); return 1; }
    MSG message{};
    int received{};
    while ((received=GetMessageW(&message,nullptr,0,0))>0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return received<0?1:0;
}
