#include "panel.hpp"
#include <algorithm>
#include <cstdio>
#include <cwchar>

namespace {
int failures{};
void check(bool condition,const char* label) {
    std::printf("%s %s\n",condition?"PASS":"FAIL",label);
    if (!condition) ++failures;
}
void click(HWND window,int x,int y) {
    RECT area{}; GetClientRect(window,&area);
    const LPARAM point=MAKELPARAM(x*area.right/1120,y*area.bottom/690);
    SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point);
    SendMessageW(window,WM_LBUTTONUP,0,point);
}
bool bitmap(HWND window,const wchar_t* path,std::uint64_t* cursor_digest=nullptr) {
    RECT area{}; GetClientRect(window,&area);
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=area.right;
    info.bmiHeader.biHeight=-area.bottom; info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr);
    if (!dc) return false;
    void* pixels{};
    HBITMAP image=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if (!image) { DeleteDC(dc); return false; }
    const auto previous=SelectObject(dc,image);
    const BOOL printed=PrintWindow(window,dc,PW_CLIENTONLY);
    GdiFlush();
    const auto* words=static_cast<const std::uint32_t*>(pixels);
    const int pixel_count=area.right*area.bottom;
    if (cursor_digest) {
        *cursor_digest=1469598103934665603ULL;
        const int left=330*area.right/1120,top=600*area.bottom/690;
        for (int y=top;y<std::min(top+40,static_cast<int>(area.bottom));++y)
            for (int x=left;x<std::min(left+40,static_cast<int>(area.right));++x)
                *cursor_digest=(*cursor_digest^(words[y*area.right+x]&0xffffff))*1099511628211ULL;
    }
    const bool rendered=std::count_if(words,words+pixel_count,[](std::uint32_t value){return (value&0xffffff)!=0;})>pixel_count/2;
    BITMAPFILEHEADER header{};
    header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);
    const DWORD bytes=static_cast<DWORD>(area.right*area.bottom*4);
    header.bfSize=header.bfOffBits+bytes;
    esp::Handle file(CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    DWORD written{};
    const bool ok=printed && rendered && file.valid() && WriteFile(file.get(),&header,sizeof(header),&written,nullptr) && written==sizeof(header) &&
        WriteFile(file.get(),&info.bmiHeader,sizeof(info.bmiHeader),&written,nullptr) && written==sizeof(info.bmiHeader) &&
        WriteFile(file.get(),pixels,bytes,&written,nullptr) && written==bytes;
    SelectObject(dc,previous); DeleteObject(image); DeleteDC(dc);
    return ok;
}
}
int wmain(int argc,wchar_t** argv) {
    const bool cursor_only=argc==4 && std::wcscmp(argv[3],L"--cursor-only")==0;
    if (argc!=3 && !cursor_only) return 2;
    esp::Settings original{},edited{};
    std::wstring error;
    if (!esp::load_settings(argv[1],original,error)) return 3;
    const HWND foreground=GetForegroundWindow();
    HWND panel_window{};
    {
        esp::Panel panel(GetModuleHandleW(nullptr));
        panel.sync(original,true);
        check(original.panel_key==VK_INSERT,"INSERT opens the panel by default");
        check(panel.toggle(nullptr,error) && panel.window(),"custom panel created");
        panel_window=panel.window();
        panel.pump();
        if (cursor_only) {
            RECT area{}; GetClientRect(panel.window(),&area);
            const LPARAM cursor_point=MAKELPARAM(330*area.right/1120,600*area.bottom/690);
            std::uint64_t before{},with_cursor{},cleared{};
            SendMessageW(panel.window(),WM_MOUSELEAVE,0,0);
            check(bitmap(panel.window(),argv[2],&before),"capture panel before cursor");
            SendMessageW(panel.window(),WM_MOUSEMOVE,0,cursor_point);
            check(bitmap(panel.window(),argv[2],&with_cursor) && with_cursor!=before,
                  "front cursor is composited above the cached panel pixels");
            std::uint64_t repeated{},moved{};
            check(bitmap(panel.window(),argv[2],&repeated) && repeated==with_cursor,
                  "repeated painting preserves the cursor without alternating frames");
            SendMessageW(panel.window(),WM_MOUSEMOVE,0,MAKELPARAM(100*area.right/1120,100*area.bottom/690));
            check(bitmap(panel.window(),argv[2],&moved) && moved==before,
                  "moving cursor restores its previous pixels from clean panel cache");
            check(SendMessageW(panel.window(),WM_SETCURSOR,0,MAKELPARAM(HTCLIENT,WM_MOUSEMOVE))==TRUE && !GetCursor(),
                  "native cursor is suppressed locally without changing global visibility count");
            SendMessageW(panel.window(),WM_MOUSELEAVE,0,0);
            check(bitmap(panel.window(),argv[2],&cleared) && cleared==before,"leaving panel leaves no cursor trails in its pixels");
            check(!panel.take_changes(edited),"cursor painting leaves ESP options unchanged");
            panel.hide();
            std::printf("Cursor-only mode: foreground preservation was not tested.\n");
            return failures?1:0;
        }
        check(GetForegroundWindow()==foreground,"opening the panel preserves foreground focus");
        check((GetWindowLongPtrW(panel.window(),GWL_EXSTYLE)&WS_EX_NOACTIVATE)!=0 &&
              SendMessageW(panel.window(),WM_MOUSEACTIVATE,0,MAKELPARAM(HTCLIENT,WM_LBUTTONDOWN))==MA_NOACTIVATE,
              "mouse interaction cannot activate the panel");
        check(bitmap(panel.window(),argv[2]),"render dark panel with live preview");
        check(panel.toggle(nullptr,error) && !IsWindowVisible(panel.window()) && !panel.take_changes(edited),
              "closing via shortcut leaves the ESP enable state unchanged");
        check(panel.toggle(nullptr,error) && !panel.take_changes(edited),"opening via shortcut does not toggle ESP");
        panel.hide();
        click(panel.window(),90,248);
        check(panel.take_changes(edited) && edited.names!=original.names,"name checkbox changes runtime settings");
        check(!panel.take_enabled_change(),"ordinary visual edits never request re-enabling a failed renderer");
        click(panel.window(),90,118);
        check(panel.take_changes(edited) && !edited.visual_enabled && panel.take_enabled_change() && !panel.take_enabled_change(),
              "master checkbox emits an explicit single enable-state change");
        click(panel.window(),684,460);
        check(panel.take_changes(edited) && edited.max_distance==3000,"distance slider upper bound");
        click(panel.window(),416,460);
        check(panel.take_changes(edited) && edited.max_distance==1,"distance slider lower bound");
        click(panel.window(),463,460);
        check(panel.take_changes(edited) && edited.max_distance>400 && edited.max_distance<600,"distance slider changes distance limit");
        const float selected_distance=edited.max_distance;
        click(panel.window(),510,270); click(panel.window(),510,381);
        check(panel.take_changes(edited) && edited.drawing_type==esp::DrawingType::Occluded,"drawing filter selects occluded entities");
        click(panel.window(),510,360); click(panel.window(),510,434);
        check(panel.take_changes(edited) && edited.box_style==esp::BoxStyle::Corners,"box style selects corners");
        check(panel.toggle(nullptr,error),"color editor opens in the existing nonactivating panel");
        panel.pump();
        click(panel.window(),270,246);
        const std::wstring color_path=std::wstring(argv[2])+L".colors.bmp";
        check(bitmap(panel.window(),color_path.c_str()),"render internal palette and RGB controls");
        click(panel.window(),500,288);
        check(panel.take_changes(edited) && edited.name_colors.visible==0xffff943d &&
              edited.name_colors.occluded==original.name_colors.occluded,"palette edits only the chosen visibility color");
        click(panel.window(),470,380);
        check(panel.take_changes(edited) && edited.name_colors.visible==0xff00943d,"RGB slider changes only the red channel");
        click(panel.window(),470,420);
        check(panel.take_changes(edited) && edited.name_colors.visible==0xff00003d,"RGB slider supports exact channel endpoints");
        click(panel.window(),736,236);
        check(!panel.take_enabled_change() && !edited.visual_enabled,"colors and dropdowns cannot re-enable ESP");
        check(GetForegroundWindow()==foreground,"custom controls and color picker preserve focus");
        click(panel.window(),980,648);
        check(panel.take_save() && !panel.take_save(),"save request consumed exactly once");
        const std::wstring profile_path=argv[1];
        const auto directory=profile_path.substr(0,profile_path.find_last_of(L"\\/")+1);
        wchar_t temporary[MAX_PATH]{};
        if (GetTempFileNameW(directory.c_str(),L"epc",0,temporary) && CopyFileW(argv[1],temporary,FALSE)) {
            edited.armor_bar=false;
            edited.snaplines=true;
            edited.name_colors={0xff123456,0xff654321};
            const bool saved=esp::save_visual_settings(temporary,edited,error);
            check(saved,"atomic visual profile save");
            if (!saved) std::fwprintf(stderr,L"SAVE ERROR: %ls\n",error.c_str());
            esp::Settings reloaded{};
            check(esp::load_settings(temporary,reloaded,error) && reloaded.max_distance==selected_distance && reloaded.box_style==esp::BoxStyle::Corners &&
                  reloaded.drawing_type==esp::DrawingType::Occluded && !reloaded.armor_bar && reloaded.snaplines &&
                  reloaded.name_colors.visible==0xff123456 && reloaded.name_colors.occluded==0xff654321,"saved controls and both colors round-trip");
            check(reloaded.sha256==original.sha256 && reloaded.layout.device==original.layout.device &&
                  reloaded.mta.client_sha256==original.mta.client_sha256 && reloaded.visibility.line_clear==original.visibility.line_clear,
                  "visual save preserves fingerprints and engine layout");
            edited.max_distance=4000;
            check(!esp::save_visual_settings(temporary,edited,error) && esp::load_settings(temporary,reloaded,error) &&
                  reloaded.max_distance==selected_distance,"invalid profile save leaves the previous valid file intact");
            DeleteFileW(temporary);
        } else check(false,"create profile save fixture");
        SendMessageW(panel.window(),WM_CLOSE,0,0);
        check(!IsWindowVisible(panel.window()),"closing panel hides it without stopping runtime");
        check(GetForegroundWindow()==foreground,"closing the panel does not change foreground focus");
        HWND hidden_target=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"Panel target fixture",WS_POPUP,
                                          0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        check(hidden_target && panel.toggle(hidden_target,error),"panel can bind to a target window");
        panel.follow_target();
        check(!IsWindowVisible(panel.window()) && GetForegroundWindow()==foreground,
              "panel hides for an invisible or inactive target without taking focus");
        panel.hide();
        if (hidden_target) DestroyWindow(hidden_target);
        const bool reopened=panel.toggle(foreground,error);
        if (!reopened) std::fwprintf(stderr,L"REOPEN ERROR: %ls\n",error.c_str());
        check(reopened,"panel reopens over the foreground target");
        panel.follow_target();
        check(IsWindowVisible(panel.window()) && GetForegroundWindow()==foreground,
              "panel remains visible over a focused target without activation");
        panel.hide();
    }
    check(panel_window && !IsWindow(panel_window),"panel window destroyed on teardown");
    return failures?1:0;
}
