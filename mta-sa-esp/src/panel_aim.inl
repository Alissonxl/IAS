// Included in panel.cpp so both pages share the same drawing primitives.
namespace {
int aim_hover_region(int x,int y,int popup=0) {
    if (popup==3 || popup==4) {
        const int left=popup==3?420:742,top=popup==3?496:516,span=popup==3?260:332;
        for (int i=0;i<3;++i) if (hit(x,y,left,top+i*32,span,32)) return 100+i;
        return 0;
    }
    if (hit(x,y,44,538,312,58)) return 50;
    if (hit(x,y,430,24,110,36)) return 1;
    if (hit(x,y,550,24,110,36)) return 2;
    if (hit(x,y,24,92,1072,52)) return 3;
    if (hit(x,y,1060,24,34,34)) return 4;
    if (hit(x,y,880,628,216,42)) return 5;
    for (int i=0;i<7;++i) if (hit(x,y,40,218+i*47,286,32)) return 10+i;
    if (hit(x,y,312,362,40,24)) return 20;
    for (int i=0;i<3;++i) if (hit(x,y,414,218+i*74,272,60)) return 30+i;
    if (hit(x,y,420,452,260,40)) return 40;
    if (hit(x,y,420,542,260,40)) return 41;
    if (hit(x,y,742,472,332,40)) return 42;
    if (hit(x,y,742,550,332,40)) return 43;
    return 0;
}
std::wstring key_name(std::uint32_t key) {
    if (!key) return L"N\u00e3o definido";
    switch (key) {
        case VK_LBUTTON:return L"Mouse esquerdo";
        case VK_RBUTTON:return L"Mouse direito";
        case VK_MBUTTON:return L"Mouse central";
        case VK_XBUTTON1:return L"Mouse 4";
        case VK_XBUTTON2:return L"Mouse 5";
        default:return shortcut(key);
    }
}
}
void Panel::capture_key(int slot) {
    key_capture_=slot; binding_error_=false; popup_=0;
    for (int i=1;i<255;++i) key_previous_[static_cast<std::size_t>(i)]=(GetAsyncKeyState(i)&0x8000)!=0;
    invalidate();
}
void Panel::accept_key(std::uint32_t key) {
    if (!key_capture_ || !key || key>254) return;
    if (key==VK_ESCAPE) { key_capture_=0; invalidate(); return; }
    if (key_capture_==2 && (key==VK_DELETE || key==VK_BACK)) {
        values_.aim.switch_key=0; key_capture_=0; changed(); return;
    }
    if (key==values_.panel_key || key==values_.reload_key || key==values_.stop_key ||
        (key_capture_==1 && key==values_.aim.switch_key) || (key_capture_==2 && key==values_.aim.key)) {
        binding_error_=true; status(L"Tecla em uso por outro atalho. Escolha outra."); return;
    }
    if (key_capture_==1) values_.aim.key=key;
    else values_.aim.switch_key=key;
    key_capture_=0; changed();
}
void Panel::click_aim(int x,int y) {
    if (key_capture_) return;
    if (popup_==3 || popup_==4) {
        const int top=popup_==3?496:516,count=3,left=popup_==3?420:742,span=popup_==3?260:332;
        for (int i=0;i<count;++i) if (hit(x,y,left,top+i*32,span,32)) {
            if (popup_==3) values_.aim.bone=static_cast<std::uint32_t>(i);
            else values_.aim.mode=static_cast<std::uint32_t>(i);
            popup_=0; changed(); return;
        }
        popup_=0; invalidate(); return;
    }
    if (hit(x,y,24,92,1072,52)) { values_.aim.enabled=!values_.aim.enabled; changed(); return; }
    for (std::size_t i=0;i<aim_toggles.size();++i) if (hit(x,y,40,218+static_cast<int>(i)*47,262,32)) {
        const auto member=aim_toggles[i].member; values_.aim.*member=!(values_.aim.*member); changed(); return;
    }
    if (hit(x,y,312,362,40,24)) { selected_color_=&values_.aim.target_color; invalidate(); return; }
    for (int i=0;i<3;++i) if (hit(x,y,414,244+i*74,272,32)) {
        slider_=10+i; SetCapture(window_); slide(x); return;
    }
    if (hit(x,y,420,452,260,40)) { popup_=3; invalidate(); return; }
    if (hit(x,y,420,542,260,40)) { capture_key(1); return; }
    if (hit(x,y,742,472,332,40)) { popup_=4; invalidate(); return; }
    if (hit(x,y,742,550,332,40)) { capture_key(2); return; }
    if (hit(x,y,74,566,272,30)) { slider_=13; SetCapture(window_); slide(x); return; }
    if (hit(x,y,20,12,390,66)) {
        dragging_=true; GetCursorPos(&drag_origin_); RECT bounds{}; GetWindowRect(window_,&bounds);
        window_origin_={bounds.left,bounds.top}; SetCapture(window_);
    }
}
void Panel::paint_aim(Gdiplus::Graphics& g) {
    using namespace Gdiplus;
    const auto& aim=values_.aim;
    const auto hovered=[&](int x,int y,int w,int h){return hit(hover_x_,hover_y_,x,y,w,h);};
    rounded(g,24,94,1072,50,8,0xff102538,0xff16466d);
    check_box(g,42,107,aim.enabled,hovered(24,92,1072,52));
    text(g,78,98,150,40,aim.enabled?L"Mira ativada":L"Ativar mira",17,ink,true);
    text(g,246,98,710,40,L"Assist\u00eancia pela tecla escolhida. Pausada enquanto este painel estiver aberto.",13,muted);
    dot(g,1068,119,6,aim.enabled?blue:muted);
    rounded(g,24,158,356,450,10,card,border);
    rounded(g,394,158,312,450,10,card,border);
    rounded(g,720,158,376,450,10,card,border);
    text(g,44,172,300,32,L"Alvos e exibi\u00e7\u00e3o",20,ink,true);
    text(g,420,172,260,32,L"Ajuste da mira",20,ink,true);
    text(g,742,172,330,32,L"Pr\u00e9via e ativa\u00e7\u00e3o",20,ink,true);
    for (std::size_t i=0;i<aim_toggles.size();++i) {
        const int y=218+static_cast<int>(i)*47;
        const bool hover=hovered(40,y,262,32);
        if (hover) rounded(g,34,static_cast<float>(y-1),268,34,6,0xff1b2b3c);
        check_box(g,44,y+4,aim.*(aim_toggles[i].member),hover);
        text(g,80,static_cast<float>(y),226,32,aim_toggles[i].label,14);
    }
    rounded(g,312,362,40,24,5,aim.target_color,0xff536b83);
    text(g,44,538,220,24,L"Ganho do mouse",13,muted);
    wchar_t gain[24]{}; std::swprintf(gain,24,L"%.2f",static_cast<double>(aim.input_gain));
    text(g,272,538,80,24,gain,13,0xff78d5ff,true,StringAlignmentFar);
    const float gain_x=80+260*std::clamp((aim.input_gain-.01f)/4.99f,0.0f,1.0f);
    line(g,80,580,340,580,border,5); line(g,80,580,gain_x,580,blue,5); dot(g,gain_x,580,7,0xff8fdcff);
    const std::array<float,3> value{aim.fov,aim.smooth,aim.prediction_ms};
    constexpr std::array<float,3> low{1,1,0},high{600,30,200};
    constexpr std::array<const wchar_t*,3> label{L"Raio do FOV",L"Suaviza\u00e7\u00e3o",L"Previs\u00e3o do movimento"};
    for (int i=0;i<3;++i) {
        const float y=218.0f+i*74;
        text(g,420,y,182,26,label[static_cast<std::size_t>(i)],14);
        wchar_t number[32]{};
        std::swprintf(number,32,i==0?L"%.0f px":i==1?L"%.1f":L"%.0f ms",static_cast<double>(value[static_cast<std::size_t>(i)]));
        text(g,602,y,78,26,number,13,0xff78d5ff,true,StringAlignmentFar);
        const float thumb=420+260*(value[static_cast<std::size_t>(i)]-low[static_cast<std::size_t>(i)])/
            (high[static_cast<std::size_t>(i)]-low[static_cast<std::size_t>(i)]);
        line(g,420,y+42,680,y+42,border,5); line(g,420,y+42,thumb,y+42,blue,5); dot(g,thumb,y+42,7,0xff8fdcff);
    }
    text(g,420,422,260,24,L"Ponto de mira",14);
    constexpr std::array<const wchar_t*,3> bones{L"Cabe\u00e7a",L"Peito",L"Quadril"};
    const auto button=[&](int x,int y,int w,const std::wstring& label,bool active) {
        rounded(g,static_cast<float>(x),static_cast<float>(y),static_cast<float>(w),40,6,
            hovered(x,y,w,40)?0xff213247:0xff172332,active?blue:border);
        text(g,static_cast<float>(x+14),static_cast<float>(y),static_cast<float>(w-28),40,label.c_str(),14);
    };
    button(420,452,260,bones[std::min<std::uint32_t>(aim.bone,2)],popup_==3);
    line(g,654,469,659,474,muted,1.6f); line(g,659,474,664,469,muted,1.6f);
    text(g,420,512,260,24,L"Tecla da mira",14);
    button(420,542,260,key_capture_==1?L"Pressione uma tecla...":key_name(aim.key),key_capture_==1);
    rounded(g,738,216,340,224,8,0xff101923,0xff24313f);
    for (int x=754;x<1078;x+=24) line(g,static_cast<float>(x),218,static_cast<float>(x),438,0xff192936);
    for (int y=238;y<438;y+=24) line(g,740,static_cast<float>(y),1076,static_cast<float>(y),0xff192936);
    const float preview_radius=std::clamp(aim.fov,25.0f,100.0f);
    if (aim.show_fov) { Pen ring(Color(0xff339dcb),1.5f); g.DrawEllipse(&ring,908-preview_radius,328-preview_radius,preview_radius*2,preview_radius*2); }
    line(g,892,328,924,328,0xff738b9e); line(g,908,312,908,344,0xff738b9e);
    dot(g,945,278,12,0xff445d70); line(g,945,300,945,354,0xff445d70,22);
    line(g,935,350,925,401,0xff445d70,10); line(g,955,350,965,401,0xff445d70,10);
    const float target_y=aim.bone==0?278.0f:aim.bone==1?314.0f:352.0f;
    if (aim.show_target && std::hypot(37.0f,target_y-328)<=preview_radius) {
        Pen target(Color(aim.target_color),2); g.DrawEllipse(&target,937.0f,target_y-8,16.0f,16.0f);
        line(g,908,328,945,target_y,aim.target_color);
    }
    text(g,750,410,316,24,L"Pr\u00e9via ilustrativa dos ajustes",12,muted,false,StringAlignmentCenter);
    text(g,742,446,332,24,L"Modo de ativa\u00e7\u00e3o",14);
    button(742,472,332,aim.mode==0?L"Segurar tecla":aim.mode==1?L"Alternar ao pressionar":L"Tap manual",popup_==4);
    line(g,1044,489,1049,494,muted,1.6f); line(g,1049,494,1054,489,muted,1.6f);
    text(g,742,522,332,24,L"Trocar alvo (opcional)",14);
    button(742,550,332,key_capture_==2?L"Pressione uma tecla...":key_name(aim.switch_key),key_capture_==2);
    if (key_capture_) text(g,742,582,332,20,L"ESC cancela. Delete remove a tecla de troca.",11,muted);
    if (popup_==3 || popup_==4) {
        const int count=3,x=popup_==3?420:742,y=popup_==3?496:516,w=popup_==3?260:332;
        rounded(g,static_cast<float>(x),static_cast<float>(y),static_cast<float>(w),static_cast<float>(count*32),6,0xff203449,0xff407190);
        for (int i=0;i<count;++i) {
            if (hovered(x,y+i*32,w,32)) rounded(g,static_cast<float>(x+2),static_cast<float>(y+i*32+2),static_cast<float>(w-4),28,4,0xff24567b);
            text(g,static_cast<float>(x+14),static_cast<float>(y+i*32),static_cast<float>(w-28),32,
                popup_==3?bones[static_cast<std::size_t>(i)]:(i==0?L"Segurar tecla":i==1?L"Alternar ao pressionar":L"Tap manual"),14);
        }
    }
}
