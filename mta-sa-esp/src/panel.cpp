#include "panel.hpp"
#include "aim_options.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>

namespace esp {
namespace {
using namespace Gdiplus;
constexpr wchar_t panel_class[]=L"MtaEspVisualPanel";
constexpr int width=1120,height=690;
constexpr std::uint32_t paper=0xff10161f,card=0xff141c27,border=0xff293545;
constexpr std::uint32_t ink=0xffeef3fc,muted=0xff96a7bc,blue=0xff0095ff;
constexpr std::array<int,10> row_y{96,236,274,312,350,388,566,452,490,528};
constexpr std::array<std::uint32_t,12> palette{
    0xffa7ef20,0xffff943d,0xff00dbee,0xff0095ff,0xfff0e847,0xff25df76,
    0xffffffff,0xffa9b6c8,0xffff5268,0xffbf83ff,0xfff394c9,0xff202938};
bool hit(int x,int y,int left,int top,int w,int h) {
    return x>=left && x<left+w && y>=top && y<top+h;
}
int hover_region(int x,int y,int popup,bool color_editor) {
    if (color_editor) return hit(x,y,724,222,26,26)?1:0;
    if (popup) {
        const int count=popup==1?3:2,top=popup==1?296:386;
        for (int i=0;i<count;++i) if (hit(x,y,420,top+i*34,260,34)) return i+1;
        return 0;
    }
    if (hit(x,y,430,24,110,36)) return 100;
    if (hit(x,y,550,24,110,36)) return 101;
    if (hit(x,y,1060,24,34,34)) return 1;
    if (hit(x,y,24,92,1072,52)) return 2;
    if (hit(x,y,420,252,260,40)) return 3;
    if (hit(x,y,420,342,260,40)) return 4;
    if (hit(x,y,880,628,216,42)) return 5;
    if (hit(x,y,414,406,272,72)) return 6;
    for (std::size_t i=1;i<row_y.size();++i) {
        if (hit(x,y,40,row_y[i],195,30)) return static_cast<int>(10+i);
        for (int col=0;col<2;++col) if (hit(x,y,248+col*64,row_y[i]+3,48,24))
            return static_cast<int>(30+i*2)+col;
    }
    return 0;
}
void rounded(Graphics& g,float x,float y,float w,float h,float radius,
             std::uint32_t fill,std::uint32_t stroke=0) {
    GraphicsPath path;
    const float d=radius*2;
    path.AddArc(x,y,d,d,180,90); path.AddArc(x+w-d,y,d,d,270,90);
    path.AddArc(x+w-d,y+h-d,d,d,0,90); path.AddArc(x,y+h-d,d,d,90,90); path.CloseFigure();
    SolidBrush brush{Color(fill)}; g.FillPath(&brush,&path);
    if (stroke) { Pen pen{Color(stroke),1}; g.DrawPath(&pen,&path); }
}
void line(Graphics& g,float x,float y,float right,float bottom,std::uint32_t color,float thickness=1) {
    Pen pen{Color(color),thickness};
    pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
    g.DrawLine(&pen,x,y,right,bottom);
}
void text(Graphics& g,float x,float y,float w,float h,const wchar_t* value,float size=15,
          std::uint32_t color=ink,bool bold=false,StringAlignment align=StringAlignmentNear) {
    Font font(L"Segoe UI",size,bold?FontStyleBold:FontStyleRegular,UnitPixel);
    SolidBrush brush{Color(color)}; StringFormat format;
    format.SetAlignment(align); format.SetLineAlignment(StringAlignmentCenter);
    format.SetFormatFlags(StringFormatFlagsNoWrap); format.SetTrimming(StringTrimmingEllipsisCharacter);
    g.DrawString(value,-1,&font,RectF(x,y,w,h),&format,&brush);
}
void dot(Graphics& g,float x,float y,float r,std::uint32_t color) {
    SolidBrush brush{Color(color)}; g.FillEllipse(&brush,x-r,y-r,r*2,r*2);
}
void check_box(Graphics& g,int x,int y,bool checked,bool hover) {
    rounded(g,static_cast<float>(x),static_cast<float>(y),23,23,5,
            checked?blue:(hover?0xff26394e:0xff1b2635),checked?0xff38b2ff:border);
    if (checked) {
        line(g,static_cast<float>(x+6),static_cast<float>(y+12),static_cast<float>(x+10),static_cast<float>(y+16),ink,2);
        line(g,static_cast<float>(x+10),static_cast<float>(y+16),static_cast<float>(x+18),static_cast<float>(y+7),ink,2);
    }
}
std::uint32_t channel_color(std::uint32_t color,int channel,int value) {
    const unsigned shift=static_cast<unsigned>((2-channel)*8);
    return (color&~(0xffU<<shift))|(static_cast<std::uint32_t>(value)<<shift)|0xff000000U;
}
int slider_value(int x,int left,int span,int maximum) {
    return static_cast<int>(std::lround(std::clamp(static_cast<double>(x-left)/span,0.0,1.0)*maximum));
}
std::wstring shortcut(std::uint32_t key) {
    if (key==VK_INSERT) return L"INSERT";
    wchar_t name[32]{};
    const UINT scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC);
    const LONG flags=static_cast<LONG>((scan<<16)|((key==VK_DELETE || key==VK_HOME || key==VK_END)?1U<<24:0));
    return GetKeyNameTextW(flags,name,32)>0?name:L"PAINEL";
}
}
#include "panel_aim.inl"
#include "panel_usability.inl"
Panel::~Panel() { release(); }
void Panel::release() noexcept {
    hide();
    positioned_=false;
    if (window_) DestroyWindow(window_);
    window_=nullptr;
    if (atom_) UnregisterClassW(panel_class,instance_);
    atom_=0;
    delete buffer_; buffer_=nullptr; content_dirty_=true;
    delete composition_; composition_=nullptr;
    if (graphics_token_) Gdiplus::GdiplusShutdown(graphics_token_);
    graphics_token_=0; selected_color_=nullptr; open_=false;
}
void Panel::invalidate() noexcept { content_dirty_=true; if (window_) InvalidateRect(window_,nullptr,FALSE); }
void Panel::cursor_at(int x,int y,bool visible) noexcept {
    if (cursor_visible_==visible && (!visible || (cursor_.x==x && cursor_.y==y))) return;
    // Cursor movement only recomposes the cached interface, never rebuilds its controls.
    if (window_ && cursor_visible_) {
        RECT old{cursor_.x-4,cursor_.y-4,cursor_.x+28,cursor_.y+37};
        InvalidateRect(window_,&old,FALSE);
    }
    cursor_={x,y}; cursor_visible_=visible;
    if (window_ && visible) {
        RECT next{x-4,y-4,x+28,y+37}; InvalidateRect(window_,&next,FALSE);
    }
}

bool Panel::create(std::wstring& error) {
    if (IsWindow(window_)) return true;
    release();
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&graphics_token_,&input,nullptr)!=Gdiplus::Ok) {
        error=L"Inicializar desenho do painel falhou"; graphics_token_=0; return false;
    }
    WNDCLASSW wc{};
    wc.hInstance=instance_; wc.lpfnWndProc=window_proc; wc.lpszClassName=panel_class;
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    atom_=RegisterClassW(&wc);
    if (!atom_) { error=L"Registrar painel: "+std::to_wstring(GetLastError()); release(); return false; }
    MONITORINFO monitor{sizeof(monitor),{},{},0};
    GetMonitorInfoW(MonitorFromWindow(target_,MONITOR_DEFAULTTONEAREST),&monitor);
    const int available_w=monitor.rcWork.right-monitor.rcWork.left;
    const int available_h=monitor.rcWork.bottom-monitor.rcWork.top;
    scale_=std::min({1.0f,static_cast<float>(available_w-32)/width,static_cast<float>(available_h-32)/height});
    if (scale_<=0) scale_=1;
    window_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,panel_class,L"MTA:SA | Visual",
                           WS_POPUP,0,0,static_cast<int>(width*scale_),static_cast<int>(height*scale_),
                           interactive_?target_:nullptr,nullptr,instance_,this);
    if (!window_) { error=L"Criar painel: "+std::to_wstring(GetLastError()); release(); return false; }
    HRGN shape=CreateRoundRectRgn(0,0,static_cast<int>(width*scale_)+1,static_cast<int>(height*scale_)+1,22,22);
    if (shape && !SetWindowRgn(window_,shape,FALSE)) DeleteObject(shape);
    return true;
}
void Panel::sync(const Settings& settings,bool enabled) {
    values_=settings; values_.visual_enabled=enabled; invalidate();
}
bool Panel::toggle(HWND target,std::wstring& error) {
    error.clear();
    if (open_) { hide(); return true; }
    const HWND next_target=IsWindow(target)?GetAncestor(target,GA_ROOT):nullptr;
    DWORD target_pid{};
    if (next_target) GetWindowThreadProcessId(next_target,&target_pid);
    const bool next_interactive=target_pid==GetCurrentProcessId() && IsWindowVisible(next_target) && !IsIconic(next_target);
    if (window_ && (interactive_!=next_interactive || (next_interactive && target_!=next_target))) release();
    target_=next_target; interactive_=next_interactive;
    if (!create(error)) return false;
    RECT bounds{};
    MONITORINFO monitor{sizeof(monitor),{},{},0};
    GetMonitorInfoW(MonitorFromWindow(target_,MONITOR_DEFAULTTONEAREST),&monitor);
    if (!target_ || !GetWindowRect(target_,&bounds)) bounds=monitor.rcWork;
    const float previous_scale=scale_;
    scale_=std::min({1.0f,static_cast<float>(monitor.rcWork.right-monitor.rcWork.left-32)/width,
                    static_cast<float>(monitor.rcWork.bottom-monitor.rcWork.top-32)/height});
    if (scale_<=0) scale_=1;
    const int w=static_cast<int>(width*scale_),h=static_cast<int>(height*scale_);
    RECT previous{}; const bool remembered=positioned_ && GetWindowRect(window_,&previous);
    const int x=std::clamp(remembered?static_cast<int>(previous.left):static_cast<int>(bounds.left+(bounds.right-bounds.left-w)/2),
                           static_cast<int>(monitor.rcWork.left),std::max(static_cast<int>(monitor.rcWork.left),static_cast<int>(monitor.rcWork.right)-w));
    const int y=std::clamp(remembered?static_cast<int>(previous.top):static_cast<int>(bounds.top+(bounds.bottom-bounds.top-h)/2),
                           static_cast<int>(monitor.rcWork.top),std::max(static_cast<int>(monitor.rcWork.top),static_cast<int>(monitor.rcWork.bottom)-h));
    SetWindowPos(window_,HWND_TOPMOST,x,y,w,h,SWP_NOACTIVATE);
    positioned_=true;
    save_shortcut_down_=(GetAsyncKeyState(VK_CONTROL)&0x8000) && (GetAsyncKeyState('S')&0x8000);
    if (previous_scale!=scale_) {
        HRGN shape=CreateRoundRectRgn(0,0,w+1,h+1,22,22);
        if (shape && !SetWindowRgn(window_,shape,FALSE)) DeleteObject(shape);
    }
    open_=true; popup_=0; selected_color_=nullptr;
    ShowWindow(window_,SW_SHOWNOACTIVATE);
    invalidate();
    return true;
}
void Panel::finish_drag() noexcept {
    dragging_=false; slider_=-1;
    if (GetCapture()==window_) ReleaseCapture();
}
void Panel::hide() noexcept {
    cursor_at(0,0,false);
    key_capture_=0;
    open_=false; popup_=0; selected_color_=nullptr; finish_drag();
    if (window_) ShowWindow(window_,SW_HIDE);
}
void Panel::follow_target() noexcept {
    if (!open_ || !window_ || !target_) return;
    const HWND foreground=GetAncestor(GetForegroundWindow(),GA_ROOT);
    const bool show=IsWindow(target_) && IsWindowVisible(target_) && !IsIconic(target_) &&
                    foreground==target_;
    if (!show) {
        const bool changed=key_capture_ || popup_ || selected_color_;
        key_capture_=0; popup_=0; selected_color_=nullptr;
        finish_drag(); cursor_at(0,0,false);
        if (changed) invalidate();
    }
    if (show!=static_cast<bool>(IsWindowVisible(window_))) {
        ShowWindow(window_,show?SW_SHOWNOACTIVATE:SW_HIDE);
    }
}
void Panel::above(HWND overlay) noexcept {
    if (window_ && IsWindowVisible(window_) &&
        ((IsWindowVisible(overlay) && !window_above(window_,overlay)) ||
         (IsWindowVisible(target_) && !window_above(window_,target_))))
        SetWindowPos(window_,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
}
void Panel::pump() noexcept {
    follow_target();
    const bool capture_at_start=key_capture_!=0;
    MSG message{};
    while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    if (open_ && window_ && IsWindowVisible(window_)) {
        POINT point{}; RECT client{};
        if (GetCursorPos(&point) && ScreenToClient(window_,&point) && GetClientRect(window_,&client))
            cursor_at(point.x,point.y,PtInRect(&client,point)!=FALSE);
        else cursor_at(0,0,false);
    }
    if (key_capture_) {
        for (int key=1;key<255;++key) {
            const bool down=(GetAsyncKeyState(key)&0x8000)!=0;
            const bool rising=down && !key_previous_[static_cast<std::size_t>(key)];
            key_previous_[static_cast<std::size_t>(key)]=down;
            if (!rising) continue;
            accept_key(static_cast<std::uint32_t>(key));
            if (!key_capture_) break;
        }
    }
    const bool save_chord=open_ && target_ && IsWindowVisible(window_) && !capture_at_start && !key_capture_ &&
        GetForegroundWindow()==GetAncestor(target_,GA_ROOT) && (GetAsyncKeyState(VK_CONTROL)&0x8000) && (GetAsyncKeyState('S')&0x8000);
    if (save_chord && !save_shortcut_down_) save_=true;
    save_shortcut_down_=save_chord;
    // Release a drag even if its final button event was lost while switching windows.
    if ((dragging_ || slider_>=0) && window_) {
        if (!(GetAsyncKeyState(VK_LBUTTON)&0x8000)) finish_drag();
        else {
            POINT point{};
            if (GetCursorPos(&point) && ScreenToClient(window_,&point))
                move(static_cast<int>(point.x/scale_),static_cast<int>(point.y/scale_));
        }
    }
}
bool Panel::take_changes(Settings& settings) {
    if (!std::exchange(dirty_,false)) return false;
    settings=values_; return true;
}
void Panel::status(std::wstring value) { if (value!=status_) { status_=std::move(value); invalidate(); } }
void Panel::changed() { dirty_=true; unsaved_=true; status_=L"Altera\u00e7\u00f5es aplicadas. Ctrl+S salva o perfil."; invalidate(); }
void Panel::slide(int x) {
    if (slider_==13) {
        const float value=std::round((.01f+4.99f*std::clamp(static_cast<float>(x-80)/260,0.0f,1.0f))*100)/100;
        if (values_.aim.input_gain!=value) { values_.aim.input_gain=value; changed(); }
    } else if (slider_>=10 && slider_<=12) {
        const float fraction=std::clamp(static_cast<float>(x-420)/260,0.0f,1.0f);
        float* value=slider_==10?&values_.aim.fov:slider_==11?&values_.aim.smooth:&values_.aim.prediction_ms;
        const float next=slider_==10?1+599*fraction:slider_==11?1+29*fraction:200*fraction;
        const float rounded_value=std::round(next*10)/10;
        if (*value!=rounded_value) { *value=rounded_value; changed(); }
    } else if (slider_==0) {
        const float value=static_cast<float>(1+slider_value(x,420,260,2999));
        if (values_.max_distance!=value) { values_.max_distance=value; changed(); }
    } else if (selected_color_ && slider_>=1 && slider_<=3) {
        const auto value=channel_color(*selected_color_,slider_-1,slider_value(x,474,236,255));
        if (value!=*selected_color_) { *selected_color_=value; changed(); }
    }
}
void Panel::move(int x,int y) {
    if (dragging_) {
        POINT point{}; RECT bounds{};
        if (GetCursorPos(&point) && GetWindowRect(window_,&bounds)) {
            MONITORINFO monitor{sizeof(monitor),{},{},0};
            GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST),&monitor);
            const int w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;
            const int left=std::clamp(static_cast<int>(window_origin_.x+point.x-drag_origin_.x),
                static_cast<int>(monitor.rcWork.left),std::max(static_cast<int>(monitor.rcWork.left),static_cast<int>(monitor.rcWork.right)-w));
            const int top=std::clamp(static_cast<int>(window_origin_.y+point.y-drag_origin_.y),
                static_cast<int>(monitor.rcWork.top),std::max(static_cast<int>(monitor.rcWork.top),static_cast<int>(monitor.rcWork.bottom)-h));
            SetWindowPos(window_,nullptr,left,top,0,0,SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);
        }
    } else if (slider_>=0) slide(x);
    const bool hover_changed=tab_==1 && !selected_color_?
        aim_hover_region(hover_x_,hover_y_,popup_)!=aim_hover_region(x,y,popup_):
        hover_region(hover_x_,hover_y_,popup_,selected_color_!=nullptr)!=hover_region(x,y,popup_,selected_color_!=nullptr);
    hover_x_=x; hover_y_=y;
    if (hover_changed) invalidate();
}
void Panel::click(int x,int y) {
    if (hit(x,y,1060,24,34,34)) { hide(); return; }
    if (hit(x,y,430,24,110,36) || hit(x,y,550,24,110,36)) {
        finish_drag(); popup_=0; selected_color_=nullptr; key_capture_=0;
        tab_=x<550?0:1; invalidate(); return;
    }
    if (hit(x,y,880,628,216,42)) { save_=true; return; }
    if (selected_color_) {
        if (hit(x,y,724,222,26,26)) { selected_color_=nullptr; invalidate(); return; }
        for (int i=0;i<12;++i) if (hit(x,y,438+(i%6)*50,276+(i/6)*38,38,26)) {
            *selected_color_=palette[static_cast<std::size_t>(i)]; changed(); return;
        }
        for (int i=0;i<3;++i) if (hit(x,y,468,366+i*40,248,26)) {
            slider_=i+1; SetCapture(window_); slide(x); return;
        }
        if (hit(x,y,414,210,344,304)) return;
        selected_color_=nullptr; invalidate(); return;
    }
    if (tab_==1) { click_aim(x,y); return; }
    if (popup_ && tab_==0) {
        const int count=popup_==1?3:2,top=popup_==1?296:386;
        for (int i=0;i<count;++i) if (hit(x,y,420,top+i*34,260,34)) {
            if (popup_==1) values_.drawing_type=static_cast<DrawingType>(i);
            else values_.box_style=static_cast<BoxStyle>(i);
            popup_=0; changed(); return;
        }
        popup_=0; invalidate(); return;
    }
    if (hit(x,y,1060,24,34,34)) { hide(); return; }
    if (hit(x,y,24,92,1072,52)) {
        values_.visual_enabled=!values_.visual_enabled; enabled_change_=true; changed(); return;
    }
    for (std::size_t i=1;i<visual_toggles.size();++i) if (hit(x,y,40,row_y[i],195,30)) {
        auto member=visual_toggles[i].member; values_.*member=!(values_.*member); changed(); return;
    }
    for (std::size_t i=0;i<visual_pairs.size();++i) for (int col=0;col<2;++col) {
        if (hit(x,y,248+col*64,row_y[i+1]+3,48,24)) {
            auto& pair=values_.*(visual_pairs[i].member);
            selected_color_=col?&pair.occluded:&pair.visible; invalidate(); return;
        }
    }
    if (hit(x,y,248,row_y[7]+3,48,24)) { selected_color_=&values_.health_color; invalidate(); return; }
    if (hit(x,y,248,row_y[8]+3,48,24)) { selected_color_=&values_.armor_color; invalidate(); return; }
    if (hit(x,y,420,252,260,40)) { popup_=1; invalidate(); return; }
    if (hit(x,y,420,342,260,40)) { popup_=2; invalidate(); return; }
    if (hit(x,y,414,444,272,32)) { slider_=0; SetCapture(window_); slide(x); return; }
    if (hit(x,y,880,628,216,42)) { save_=true; return; }
    if (hit(x,y,20,12,1028,66)) {
        dragging_=true; GetCursorPos(&drag_origin_);
        RECT bounds{}; GetWindowRect(window_,&bounds); window_origin_={bounds.left,bounds.top};
        SetCapture(window_);
    }
}
void Panel::paint(HDC dc) {
    using namespace Gdiplus;
    RECT client{}; GetClientRect(window_,&client);
    if (client.right<=0 || client.bottom<=0) return;
    if (!buffer_ || buffer_->GetWidth()!=static_cast<UINT>(client.right) || buffer_->GetHeight()!=static_cast<UINT>(client.bottom)) {
        delete buffer_; buffer_=nullptr;
        buffer_=new Bitmap(client.right,client.bottom,PixelFormat32bppPARGB);
        content_dirty_=true;
    }
    if (content_dirty_) {
    Graphics g(buffer_);
    g.Clear(Color(paper)); g.ScaleTransform(scale_,scale_);
    g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    const auto hovered=[&](int x,int y,int w,int h) { return hit(hover_x_,hover_y_,x,y,w,h); };
    rounded(g,0.5f,0.5f,width-1.0f,height-1.0f,12,paper,border);
    // Eye mark and restrained header.
    Pen eye(Color(blue),2.5f);
    g.DrawArc(&eye,30.0f,26.0f,34.0f,34.0f,205.0f,130.0f);
    g.DrawArc(&eye,30.0f,8.0f,34.0f,34.0f,25.0f,130.0f);
    dot(g,47,34,5,blue);
    text(g,80,18,330,30,L"CONTROLE",25,ink,true);
    text(g,80,50,450,22,L"Configura\u00e7\u00f5es de exibi\u00e7\u00e3o",13,muted);
    rounded(g,830,28,74,28,6,card,border);
    const auto key=shortcut(values_.panel_key); text(g,830,28,74,28,key.c_str(),12,ink,false,StringAlignmentCenter);
    text(g,916,28,136,28,L"Abrir / fechar",13,muted);
    if (hovered(1060,24,34,34)) rounded(g,1060,24,34,34,6,0xff263547);
    line(g,1070,34,1084,48,muted,1.6f); line(g,1084,34,1070,48,muted,1.6f);
    rounded(g,426,20,240,44,9,0xff0b111a,border);
    for (int i=0;i<2;++i) {
        const float x=430.0f+i*120;
        if (tab_==i) rounded(g,x,24,110,36,7,0xff143c5c,0xff226597);
        else if (hovered(static_cast<int>(x),24,110,36)) rounded(g,x,24,110,36,7,0xff1b2b3b);
        text(g,x,24,110,36,i==0?L"Visual":L"Mira",14,tab_==i?0xff80d7ff:muted,true,StringAlignmentCenter);
    }
    line(g,1,82,1119,82,border);
    constexpr std::array<const wchar_t*,3> drawings{L"Todos",L"Vis\u00edveis",L"Ocultos"};
    constexpr std::array<const wchar_t*,2> styles{L"Padr\u00e3o",L"Cantos"};
    if (tab_==1) paint_aim(g);
    else {
    rounded(g,24,94,1072,50,8,hovered(24,92,1072,52)?0xff16334d:0xff102538,0xff16466d);
    check_box(g,42,107,values_.visual_enabled,hovered(24,92,1072,52));
    text(g,78,98,104,40,L"Ativado",17,ink,true);
    text(g,190,98,600,40,L"Controla a exibi\u00e7\u00e3o do ESP",13,muted);
    dot(g,1068,119,6,values_.visual_enabled?blue:muted);
    rounded(g,24,158,356,450,10,card,border);
    rounded(g,394,158,312,450,10,card,border);
    rounded(g,720,158,376,450,10,card,border);
    text(g,42,172,220,32,L"Elementos",20,ink,true);
    text(g,420,172,240,32,L"Ajustes",20,ink,true);
    text(g,740,172,240,32,L"Pr\u00e9via",20,ink,true);
    text(g,240,208,64,24,L"Vis\u00edvel",12,muted,false,StringAlignmentCenter);
    text(g,304,208,64,24,L"Oculto",12,muted,false,StringAlignmentCenter);
    constexpr std::array<const wchar_t*,10> labels{
        L"Ativado",L"Nome",L"Dist\u00e2ncia",L"Caixa",L"Snapline",L"Esqueleto",
        L"Todas as dimens\u00f5es",L"Vida",L"Colete",L"Valores HP / AP"};
    for (std::size_t i=1;i<visual_toggles.size();++i) {
        const int y=row_y[i]; const bool hover=hovered(40,y,195,30);
        if (hover) rounded(g,34,static_cast<float>(y-2),204,34,6,0xff1b2b3c);
        check_box(g,44,y+3,values_.*(visual_toggles[i].member),hover);
        text(g,80,static_cast<float>(y),160,30,labels[i],14);
    }
    line(g,40,435,364,435,border);
    const auto swatch=[&](int x,int y,std::uint32_t color) {
        rounded(g,static_cast<float>(x),static_cast<float>(y),48,24,5,color,hovered(x,y,48,24)?ink:0xff3a485b);
    };
    for (std::size_t i=0;i<visual_pairs.size();++i) {
        const auto& pair=values_.*(visual_pairs[i].member);
        swatch(248,row_y[i+1]+3,pair.visible); swatch(312,row_y[i+1]+3,pair.occluded);
    }
    swatch(248,row_y[7]+3,values_.health_color); swatch(248,row_y[8]+3,values_.armor_color);
    text(g,420,220,260,24,L"Tipo de desenho",14);
    text(g,420,310,260,24,L"Estilo da caixa",14);
    const auto dropdown=[&](int y,const wchar_t* value,bool expanded) {
        rounded(g,420,static_cast<float>(y),260,40,6,hovered(420,y,260,40)?0xff213247:0xff172332,expanded?blue:border);
        text(g,434,static_cast<float>(y),220,40,value,14);
        line(g,654,static_cast<float>(y+17),659,static_cast<float>(y+22),muted,1.6f);
        line(g,659,static_cast<float>(y+22),664,static_cast<float>(y+17),muted,1.6f);
    };
    dropdown(252,drawings[std::min<std::size_t>(static_cast<std::size_t>(values_.drawing_type),2)],popup_==1);
    dropdown(342,styles[std::min<std::size_t>(static_cast<std::size_t>(values_.box_style),1)],popup_==2);
    text(g,420,408,160,30,L"Limite de dist\u00e2ncia",14);
    wchar_t distance[32]{};
    std::swprintf(distance,32,L"%.0f m",static_cast<double>(values_.max_distance));
    rounded(g,590,406,90,32,6,0xff172332,border);
    text(g,590,406,90,32,distance,13,ink,false,StringAlignmentCenter);
    const float thumb=420+260*std::clamp((values_.max_distance-1)/2999,0.0f,1.0f);
    line(g,420,460,680,460,border,6); line(g,420,460,thumb,460,blue,6);
    dot(g,thumb,460,slider_==0?9:7,0xff76c6ff);
    rounded(g,410,500,280,90,7,0xff101923,border);
    text(g,426,508,250,24,L"Cores dos elementos",13,ink,true);
    dot(g,434,546,5,0xffa7ef20); text(g,448,534,226,24,L"Vis\u00edvel",12,muted);
    dot(g,434,572,5,0xffff943d); text(g,448,560,226,24,L"Atr\u00e1s de obst\u00e1culos",12,muted);
    // The preview is derived from the same settings as the live renderer.
    rounded(g,738,216,340,374,7,0xff101720,0xff24313f);
    for (int x=754;x<1078;x+=24) line(g,static_cast<float>(x),218,static_cast<float>(x),588,0xff19232f);
    for (int y=238;y<588;y+=24) line(g,740,static_cast<float>(y),1076,static_cast<float>(y),0xff19232f);
    const bool on=values_.visual_enabled;
    const auto preview_color=[&](std::uint32_t value) { return on?value:0xff3b485aU; };
    const bool occluded=values_.drawing_type==DrawingType::Occluded;
    const auto color=[&](ColorPair pair) { return preview_color(occluded?pair.occluded:pair.visible); };
    if (values_.names) text(g,866,232,174,28,L"Jogador",16,color(values_.name_colors),true,StringAlignmentCenter);
    // Neutral silhouette, with the health and armor columns adjacent on its left.
    line(g,952,324,952,426,0xff384553,38);
    line(g,930,338,900,426,0xff384553,17); line(g,974,338,1004,426,0xff384553,17);
    line(g,939,426,925,524,0xff384553,19); line(g,965,426,979,524,0xff384553,19);
    dot(g,952,300,19,0xff384553);
    if (values_.boxes) {
        const auto c=color(values_.box_colors);
        if (values_.box_style==BoxStyle::Default) {
            line(g,880,276,1024,276,c,2); line(g,880,276,880,544,c,2);
            line(g,1024,276,1024,544,c,2); line(g,880,544,1024,544,c,2);
        } else for (const int x:{880,1024}) for (const int y:{276,544}) {
            line(g,static_cast<float>(x),static_cast<float>(y),static_cast<float>(x+(x==880?24:-24)),static_cast<float>(y),c,3);
            line(g,static_cast<float>(x),static_cast<float>(y),static_cast<float>(x),static_cast<float>(y+(y==276?24:-24)),c,3);
        }
    }
    if (values_.skeleton) {
        const std::array<PointF,14> joints{{{952,300},{952,330},{927,337},{915,380},{900,426},
            {977,337},{989,380},{1004,426},{936,418},{930,474},{925,524},{968,418},{973,474},{979,524}}};
        constexpr std::array<std::array<int,2>,13> edges{{{0,1},{1,2},{2,3},{3,4},{1,5},{5,6},{6,7},{1,8},{8,9},{9,10},{8,11},{11,12},{12,13}}};
        for (const auto& edge:edges) line(g,joints[edge[0]].X,joints[edge[0]].Y,joints[edge[1]].X,joints[edge[1]].Y,color(values_.skeleton_colors),2);
        for (const auto& joint:joints) dot(g,joint.X,joint.Y,3,color(values_.skeleton_colors));
    }
    if (values_.snaplines) line(g,906,588,952,544,color(values_.snapline_colors),1.4f);
    const auto bar=[&](int x,float amount,std::uint32_t c,const wchar_t* value) {
        rounded(g,static_cast<float>(x),308,13,232,4,0xff10161f,border);
        rounded(g,static_cast<float>(x+2),310+228*(1-amount),9,228*amount,3,preview_color(c));
        if (values_.labels) text(g,static_cast<float>(x-9),542,32,22,value,12,preview_color(ink),false,StringAlignmentCenter);
    };
    if (values_.health_bar) bar(804,1,values_.health_color,L"100");
    if (values_.armor_bar) bar(values_.health_bar?826:804,0.75f,values_.armor_color,L"75");
    if (values_.distance_label) text(g,884,553,138,24,L"84 m",13,color(values_.distance_colors),false,StringAlignmentCenter);
    if (!on) {
        rounded(g,850,386,204,42,7,0xff1a2635,border);
        text(g,850,386,204,42,L"ESP desativado",15,muted,true,StringAlignmentCenter);
    }
    }
    line(g,1,618,1119,618,border);
    dot(g,36,649,5,unsaved_?0xffffb35c:blue);
    const auto footer=footer_text(); text(g,52,630,810,38,footer.c_str(),12,unsaved_?0xffffc47c:muted);
    rounded(g,880,628,216,42,8,hovered(880,628,216,42)?0xff29a7ff:blue,0xff3eb1ff);
    text(g,880,628,216,42,unsaved_?L"Salvar alteracoes":L"Salvar perfil",15,0xffffffff,true,StringAlignmentCenter);
    if (popup_ && tab_==0) {
        const int count=popup_==1?3:2,top=popup_==1?296:386;
        rounded(g,420,static_cast<float>(top),260,static_cast<float>(count*34),6,0xff202e40,0xff3d536d);
        for (int i=0;i<count;++i) {
            if (hovered(420,top+i*34,260,34)) rounded(g,423,static_cast<float>(top+i*34+2),254,30,4,0xff244c71);
            text(g,434,static_cast<float>(top+i*34),234,34,popup_==1?drawings[static_cast<std::size_t>(i)]:styles[static_cast<std::size_t>(i)],14);
        }
    }
    if (selected_color_) {
        rounded(g,414,210,344,304,10,0xff1b293a,0xff49637e);
        text(g,438,224,260,28,L"Cor do elemento",18,ink,true);
        text(g,724,222,26,26,L"\u00d7",22,muted,false,StringAlignmentCenter);
        for (std::size_t i=0;i<palette.size();++i) rounded(g,static_cast<float>(438+(i%6)*50),
            static_cast<float>(276+(i/6)*38),38,26,5,palette[i],*selected_color_==palette[i]?ink:0xff4c5c72);
        constexpr std::array<const wchar_t*,3> channels{L"R",L"G",L"B"};
        constexpr std::array<std::uint32_t,3> colors{0xffff6378,0xff51db93,0xff55acff};
        for (int i=0;i<3;++i) {
            const int value=static_cast<int>((*selected_color_>>((2-i)*8))&255);
            const float y=380.0f+i*40;
            text(g,438,y-14,28,28,channels[static_cast<std::size_t>(i)],13,colors[static_cast<std::size_t>(i)],true);
            line(g,474,y,710,y,border,5); const float sx=474+236*static_cast<float>(value)/255;
            line(g,474,y,sx,y,colors[static_cast<std::size_t>(i)],5); dot(g,sx,y,6,ink);
            wchar_t number[8]{}; std::swprintf(number,8,L"%d",value);
            text(g,714,y-14,36,28,number,12,muted,false,StringAlignmentCenter);
        }
        wchar_t hex[24]{}; std::swprintf(hex,24,L"#%06X",static_cast<unsigned>(*selected_color_&0xffffff));
        text(g,438,478,268,26,hex,14,muted);
    }
    g.Flush(FlushIntentionSync);
    content_dirty_=false;
    }
    if (!composition_ || composition_->GetWidth()!=buffer_->GetWidth() || composition_->GetHeight()!=buffer_->GetHeight()) {
        delete composition_;
        composition_=new Bitmap(client.right,client.bottom,PixelFormat32bppPARGB);
    }
    {
        Graphics composed(composition_);
        composed.SetCompositingMode(CompositingModeSourceCopy);
        composed.DrawImage(buffer_,0,0);
        composed.SetCompositingMode(CompositingModeSourceOver);
        if (cursor_visible_) {
            composed.SetSmoothingMode(SmoothingModeAntiAlias);
            const float x=static_cast<float>(cursor_.x),y=static_cast<float>(cursor_.y);
            const PointF arrow[]{{x,y},{x,y+25},{x+6,y+19},{x+11,y+29},
                                 {x+16,y+27},{x+11,y+17},{x+20,y+17}};
            PointF shadow[7];
            for (std::size_t i=0;i<7;++i) shadow[i]={arrow[i].X+2,arrow[i].Y+3};
            SolidBrush shadow_fill(Color(120,0,8,20));
            composed.FillPolygon(&shadow_fill,shadow,7);
            LinearGradientBrush fill(PointF(x,y),PointF(x+18,y+28),Color(0xffffffff),Color(0xff70d7ff));
            Pen edge(Color(0xff112c46),2.2f); edge.SetLineJoin(LineJoinRound);
            composed.FillPolygon(&fill,arrow,7);
            composed.DrawPolygon(&edge,arrow,7);
            Pen accent(Color(0xff0095ff),1.5f); accent.SetStartCap(LineCapRound); accent.SetEndCap(LineCapRound);
            composed.DrawLine(&accent,x+3,y+8,x+3,y+17);
        }
        composed.Flush(FlushIntentionSync);
    }
    // Present interface and cursor together; no intermediate cursor-free paint reaches the window.
    { Graphics output(dc); output.DrawImage(composition_,0,0); output.Flush(FlushIntentionSync); }

}
LRESULT CALLBACK Panel::window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    auto* self=reinterpret_cast<Panel*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if (message==WM_NCCREATE) {
        self=static_cast<Panel*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self)); self->window_=window;
    }
    if (!self) return DefWindowProcW(window,message,wparam,lparam);
    try {
        if (message==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        if (self->key_capture_ && (message==WM_KEYDOWN || message==WM_SYSKEYDOWN)) {
            if (!(lparam&(1L<<30))) self->accept_key(static_cast<std::uint32_t>(wparam));
            return 0;
        }
        if (message==WM_KEYDOWN && wparam==VK_ESCAPE) {
            if (lparam&(1L<<30)) return 0;
            self->finish_drag();
            if (self->selected_color_) { self->selected_color_=nullptr; self->invalidate(); }
            else if (self->popup_) { self->popup_=0; self->invalidate(); }
            else self->hide();
            return 0;
        }
        if (message==WM_KEYDOWN && wparam=='S' && (GetKeyState(VK_CONTROL)&0x8000)) {
            if (!(lparam&(1L<<30))) self->save_=true;
            return 0;
        }
        if (message==WM_CANCELMODE) { self->finish_drag(); return 0; }
        if (message==WM_CLOSE) { self->hide(); return 0; }
        if (message==WM_PAINT) {
            PAINTSTRUCT ps{}; HDC dc=BeginPaint(window,&ps);
            try { self->paint(dc); } catch (...) { EndPaint(window,&ps); throw; }
            EndPaint(window,&ps); return 0;
        }
        if (message==WM_PRINTCLIENT) { self->paint(reinterpret_cast<HDC>(wparam)); return 0; }
        if (message==WM_ERASEBKGND) return 1;
        if (message==WM_MOUSEWHEEL) {
            POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
            if (ScreenToClient(window,&point)) {
                self->wheel_remainder_+=GET_WHEEL_DELTA_WPARAM(wparam);
                const int steps=self->wheel_remainder_/WHEEL_DELTA;
                self->wheel_remainder_%=WHEEL_DELTA;
                if (steps) self->adjust_control(static_cast<int>(point.x/self->scale_),static_cast<int>(point.y/self->scale_),steps,false,
                    (LOWORD(wparam)&MK_CONTROL)!=0,(LOWORD(wparam)&MK_SHIFT)!=0);
            }
            return 0;
        }
        if (message==WM_RBUTTONDOWN) {
            self->adjust_control(static_cast<int>(GET_X_LPARAM(lparam)/self->scale_),
                static_cast<int>(GET_Y_LPARAM(lparam)/self->scale_),0,true); return 0;
        }
        if (message==WM_LBUTTONDOWN) {
            self->click(static_cast<int>(GET_X_LPARAM(lparam)/self->scale_),
                        static_cast<int>(GET_Y_LPARAM(lparam)/self->scale_)); return 0;
        }
        if (message==WM_LBUTTONUP) { self->finish_drag(); return 0; }
        if (message==WM_CAPTURECHANGED) { self->dragging_=false; self->slider_=-1; return 0; }
        if (message==WM_MOUSEMOVE) {
            self->cursor_at(GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam),true);
            if (!self->tracking_) {
                TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};
                self->tracking_=TrackMouseEvent(&track)!=FALSE;
            }
            self->move(static_cast<int>(GET_X_LPARAM(lparam)/self->scale_),
                       static_cast<int>(GET_Y_LPARAM(lparam)/self->scale_)); return 0;
        }
        if (message==WM_MOUSELEAVE) {
            self->cursor_at(0,0,false);
            self->tracking_=false; self->hover_x_=-1; self->hover_y_=-1; self->invalidate(); return 0;
        }
        if (message==WM_SETCURSOR && LOWORD(lparam)==HTCLIENT) {
            SetCursor(nullptr); return TRUE;
        }
    } catch (...) { self->dirty_=false; self->hide(); }
    return DefWindowProcW(window,message,wparam,lparam);
}
}

