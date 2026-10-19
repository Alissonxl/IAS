#include "panel.hpp"
#include <cstdio>
namespace {
unsigned checks{},failures{};
void check(bool value,const char* label) {
    ++checks; if (!value) ++failures;
    std::printf("%s %s\n",value?"PASS":"FAIL",label);
}
void click(HWND window,int x,int y) {
    RECT r{}; GetClientRect(window,&r);
    SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x*r.right/1120,y*r.bottom/690));
    SendMessageW(window,WM_LBUTTONUP,0,0);
}
int cursor_count() { const int value=ShowCursor(FALSE); ShowCursor(TRUE); return value+1; }
}
int wmain() {
    const HINSTANCE module=GetModuleHandleW(nullptr);
    HWND owner=CreateWindowExW(0,L"STATIC",L"Panel focus regression fixture",WS_OVERLAPPEDWINDOW,80,80,800,600,nullptr,nullptr,module,nullptr);
    if (!owner) return 2;
    ShowWindow(owner,SW_SHOWNOACTIVATE);
    const HWND foreground=GetForegroundWindow();
    const int cursor=cursor_count();
    RECT clip_before{}; GetClipCursor(&clip_before);
    {
        esp::Panel panel(module); esp::Settings settings{}; panel.sync(settings,true);
        std::wstring error;
        check(panel.toggle(owner,error),"panel opens without acquiring foreground input");
        check(IsWindowVisible(panel.window()),"opening does not fail when game owns cursor visibility");
        check((GetWindowLongPtrW(panel.window(),GWL_EXSTYLE)&WS_EX_NOACTIVATE)!=0,"panel always has NOACTIVATE style");
        check(GetWindow(panel.window(),GW_OWNER)==owner,"panel remains owned by game window");
        check(IsWindowEnabled(owner) && !IsIconic(owner),"opening leaves game enabled and unminimized");
        check(GetForegroundWindow()==foreground,"opening preserves foreground window");
        check(SendMessageW(panel.window(),WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE,"mouse interaction cannot activate panel");
        click(panel.window(),64,115);
        check(panel.take_changes(settings) && !settings.visual_enabled,"checkbox remains interactive without foreground activation");
        click(panel.window(),510,270);
        SendMessageW(panel.window(),WM_KEYDOWN,VK_ESCAPE,0);
        check(IsWindowVisible(panel.window()),"Escape dismisses dropdown first");
        SendMessageW(panel.window(),WM_KEYDOWN,VK_ESCAPE,1L<<30);
        check(IsWindowVisible(panel.window()),"Escape repeat does not close panel");
        SendMessageW(panel.window(),WM_KEYDOWN,VK_ESCAPE,0);
        check(!IsWindowVisible(panel.window()) && IsWindowEnabled(owner),"closing leaves game enabled");
        check(panel.toggle(owner,error),"panel reopens after closing");
        click(panel.window(),600,42);
        click(panel.window(),500,560);
        check(panel.capturing_key(),"binding capture starts before simulated application switch");
        ShowWindow(owner,SW_HIDE);
        panel.follow_target();
        if (GetAncestor(foreground,GA_ROOT)!=owner)
            check(!IsWindowVisible(panel.window()),"panel hides when target is not foreground");
        check(!panel.capturing_key(),"hiding game cancels pending binding capture");
        SendMessageW(panel.window(),WM_KEYDOWN,'P',0);
        esp::Settings captured{};
        check(!panel.take_changes(captured),"typing after switching applications cannot rebind a hidden panel");
        panel.hide();
    }
    RECT clip_after{}; GetClipCursor(&clip_after);
    check(IsWindowEnabled(owner) && !IsIconic(owner),"teardown leaves owner enabled and unminimized");
    check(GetForegroundWindow()==foreground,"teardown does not steal foreground");
    check(cursor_count()==cursor && EqualRect(&clip_before,&clip_after),"cursor counter and confinement are unchanged");
    DestroyWindow(owner);
    std::printf("%u checks, %u failures. Auxiliary window and synthetic clicks; MTA not exercised.\n",checks,failures);
    return failures?1:0;
}
