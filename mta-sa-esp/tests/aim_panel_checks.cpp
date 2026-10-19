#include <cmath>
#define wmain original_visual_panel_checks
#include "panel_checks.cpp"
#undef wmain
int wmain(int argc,wchar_t** argv) {
    if (argc!=3) return 2;
    esp::Settings settings{},edited{}; std::wstring error;
    if (!esp::load_settings(argv[1],settings,error)) return 3;
    esp::Panel panel(GetModuleHandleW(nullptr)); panel.sync(settings,true);
    check(panel.toggle(nullptr,error),"panel opens for aim tab checks");
    const HWND foreground=GetForegroundWindow();
    click(panel.window(),600,42);
    check(!panel.take_changes(edited),"changing to Mira tab does not enable aim or alter Visual");
    const std::wstring preview=std::wstring(argv[2])+L".default.bmp";
    check(bitmap(panel.window(),preview.c_str()),"render reimagined Mira tab");
    click(panel.window(),100,115); check(panel.take_changes(edited) && edited.aim.enabled,"aim banner independently enables aim");
    check(!panel.take_enabled_change() && edited.visual_enabled,"aim toggle does not change ESP master");
    click(panel.window(),100,280); check(panel.take_changes(edited) && edited.aim.peds,"NPC checkbox changes aim filter");
    click(panel.window(),680,260); check(panel.take_changes(edited) && edited.aim.fov==600,"FOV slider reaches its upper endpoint");
    click(panel.window(),420,334); check(panel.take_changes(edited) && edited.aim.smooth==1,"smooth slider reaches lower endpoint");
    click(panel.window(),680,408); check(panel.take_changes(edited) && edited.aim.prediction_ms==200,"prediction slider uses milliseconds");
    click(panel.window(),510,470); click(panel.window(),510,576);
    check(panel.take_changes(edited) && edited.aim.bone==2,"bone dropdown selects pelvis");
    click(panel.window(),800,490); click(panel.window(),800,564);
    check(panel.take_changes(edited) && edited.aim.mode==1,"activation dropdown selects toggle mode");
    click(panel.window(),500,560); check(panel.capturing_key(),"aim key button enters focus-independent capture");
    SendMessageW(panel.window(),WM_KEYDOWN,VK_INSERT,0);
    check(panel.capturing_key() && !panel.take_changes(edited),"key capture rejects a panel-shortcut collision");
    SendMessageW(panel.window(),WM_KEYDOWN,VK_XBUTTON1,0);
    check(!panel.capturing_key() && panel.take_changes(edited) && edited.aim.key==VK_XBUTTON1,"mouse side button can be bound to aim");
    click(panel.window(),800,568); SendMessageW(panel.window(),WM_KEYDOWN,'Q',0);
    check(panel.take_changes(edited) && edited.aim.switch_key=='Q',"switch-target binding accepts a keyboard key");
    click(panel.window(),800,568); SendMessageW(panel.window(),WM_KEYDOWN,VK_DELETE,0);
    check(panel.take_changes(edited) && !edited.aim.switch_key,"Delete clears optional switch binding");
    click(panel.window(),500,560); SendMessageW(panel.window(),WM_KEYDOWN,VK_ESCAPE,0);
    check(!panel.capturing_key() && IsWindowVisible(panel.window()),"Escape cancels binding without closing panel");
    click(panel.window(),500,560);
    click(panel.window(),480,42); check(!panel.capturing_key(),"changing tabs cancels pending key capture");
    check(GetForegroundWindow()==foreground,"tabs and dropdowns preserve foreground focus");
    click(panel.window(),600,42); click(panel.window(),900,650);
    check(panel.take_save() && !panel.take_save(),"save button works from Mira tab");
    const std::wstring saved=std::wstring(argv[2])+L".ini";
    CopyFileW(argv[1],saved.c_str(),FALSE);
    check(esp::save_visual_settings(saved,edited,error),"combined Visual and Mira profile saves atomically");
    esp::Settings roundtrip;
    check(esp::load_settings(saved,roundtrip,error) && roundtrip.aim.enabled && roundtrip.aim.peds &&
        roundtrip.aim.fov==600 && roundtrip.aim.smooth==1 && roundtrip.aim.prediction_ms==200 &&
        roundtrip.aim.bone==2 && roundtrip.aim.mode==1 && roundtrip.aim.key==VK_XBUTTON1 && !roundtrip.aim.switch_key,"all edited aim settings survive profile reload");
    DeleteFileW(saved.c_str());
    click(panel.window(),800,490); click(panel.window(),800,596);
    check(panel.take_changes(edited) && edited.aim.mode==2,"activation menu offers Tap manual as a third mode");
    const auto wheel=[&](int x,int y,int delta) {
        RECT area{}; GetClientRect(panel.window(),&area); POINT point{x*area.right/1120,y*area.bottom/690};
        ClientToScreen(panel.window(),&point);
        SendMessageW(panel.window(),WM_MOUSEWHEEL,MAKEWPARAM(0,delta),MAKELPARAM(point.x,point.y));
    };
    wheel(500,260,-WHEEL_DELTA);
    check(panel.take_changes(edited) && edited.aim.fov==599,"wheel adjusts FOV in precise one-pixel increments");
    wheel(500,334,WHEEL_DELTA);
    check(panel.take_changes(edited) && std::abs(edited.aim.smooth-1.1f)<.001f,"wheel adjusts smoothing in tenths");
    RECT area{}; GetClientRect(panel.window(),&area);
    SendMessageW(panel.window(),WM_RBUTTONDOWN,0,MAKELPARAM(500*area.right/1120,334*area.bottom/690));
    check(panel.take_changes(edited) && edited.aim.smooth==2,"right click restores the hovered control default");
    wheel(200,580,WHEEL_DELTA);
    check(panel.take_changes(edited) && std::abs(edited.aim.input_gain-1.01f)<.001f,"mouse gain is editable from the panel with fine wheel steps");
    check(panel.has_unsaved_changes(),"edits keep an explicit unsaved state");
    panel.status(L"MTA diagnostic refresh");
    check(panel.has_unsaved_changes(),"periodic game status cannot erase unsaved indication");
    panel.mark_saved(false); check(panel.has_unsaved_changes(),"failed saving keeps unsaved indication");
    panel.mark_saved(true); check(!panel.has_unsaved_changes(),"successful saving clears unsaved indication");
    CopyFileW(argv[1],saved.c_str(),FALSE);
    check(esp::save_visual_settings(saved,edited,error) && esp::load_settings(saved,roundtrip,error) &&
        roundtrip.aim.mode==2 && std::abs(roundtrip.aim.input_gain-1.01f)<.001f,"Tap mode and mouse gain survive profile reload");
    DeleteFileW(saved.c_str());
    RECT before{}; GetWindowRect(panel.window(),&before);
    SetWindowPos(panel.window(),nullptr,before.left+8,before.top+8,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    RECT moved{}; GetWindowRect(panel.window(),&moved); panel.hide(); panel.toggle(nullptr,error);
    RECT reopened{}; GetWindowRect(panel.window(),&reopened);
    check(moved.left==reopened.left && moved.top==reopened.top,"panel remembers its position when reopened");
    check(bitmap(panel.window(),argv[2]),"capture final Mira layout after interactions");
    panel.hide();
    return failures?1:0;
}

