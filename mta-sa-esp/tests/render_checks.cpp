#include "render.hpp"
#include <cstdio>
#include <cstring>

namespace {
int fail(const char* message) { std::fprintf(stderr,"FAIL %s\n",message); return 1; }
bool save_bitmap(const wchar_t* path,const D3DLOCKED_RECT& pixels,UINT width,UINT height) {
    BITMAPFILEHEADER file_header{};
    BITMAPINFOHEADER image_header{};
    file_header.bfType=0x4d42;
    file_header.bfOffBits=sizeof(file_header)+sizeof(image_header);
    file_header.bfSize=file_header.bfOffBits+width*height*4;
    image_header.biSize=sizeof(image_header);
    image_header.biWidth=static_cast<LONG>(width);
    image_header.biHeight=-static_cast<LONG>(height);
    image_header.biPlanes=1;
    image_header.biBitCount=32;
    image_header.biCompression=BI_RGB;
    esp::Handle file(CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    if (!file.valid()) return false;
    DWORD written{};
    if (!WriteFile(file.get(),&file_header,sizeof(file_header),&written,nullptr) || written!=sizeof(file_header) ||
        !WriteFile(file.get(),&image_header,sizeof(image_header),&written,nullptr) || written!=sizeof(image_header)) return false;
    for (UINT row=0;row<height;++row) {
        const auto* data=static_cast<const unsigned char*>(pixels.pBits)+row*pixels.Pitch;
        if (!WriteFile(file.get(),data,width*4,&written,nullptr) || written!=width*4) return false;
    }
    return true;
}
}
int wmain(int argc,wchar_t** argv) {
    if (argc!=3) return fail("supply profile path and output bitmap path");
    esp::Settings settings{};
    std::wstring error;
    if (!esp::load_settings(argv[1],settings,error)) return fail("load render profile");
    const HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSW window_class{};
    window_class.lpfnWndProc=DefWindowProcW;
    window_class.hInstance=instance;
    window_class.lpszClassName=L"MtaEspRenderChecks";
    if (!RegisterClassW(&window_class)) return fail("register hidden test window");
    const HWND window=CreateWindowW(window_class.lpszClassName,L"MTA ESP render checks",WS_OVERLAPPEDWINDOW,
                                     0,0,800,600,nullptr,nullptr,instance,nullptr);
    if (!window) return fail("create hidden test window");
    int outcome=0;
    {
        esp::ComPtr<IDirect3D9> d3d;
        *d3d.put()=Direct3DCreate9(D3D_SDK_VERSION);
        esp::ComPtr<IDirect3DDevice9> device;
        D3DPRESENT_PARAMETERS parameters{};
        parameters.Windowed=TRUE;
        parameters.SwapEffect=D3DSWAPEFFECT_DISCARD;
        parameters.hDeviceWindow=window;
        parameters.BackBufferWidth=800;
        parameters.BackBufferHeight=600;
        if (!d3d.get() || FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                                 &parameters,device.put()))) outcome=fail("create D3D9 HAL device");
        else {
            esp::Renderer renderer;
            esp::Frame frame{};
            frame.view[9]=-0.1f; frame.view[12]=0.5f; frame.view[13]=0.5f; frame.view[14]=1;
            frame.status=L"Ativo";
            esp::Entity entity{};
            entity.health=50; entity.max_health=100; entity.distance=50;
            entity.armor=75; entity.armor_known=true; entity.visibility=esp::Visibility::Visible;
            settings.snaplines=true;
            entity.name=L"Jo\u00e3o_Teste";
            const std::array<esp::Vec3,21> pose{{
                {0,0,.9f},{0,0,.7f},{0,0,.5f},{0,0,.2f},{0,0,0},
                {.25f,0,.5f},{.4f,0,.3f},{.5f,0,.1f},{.52f,0,.05f},
                {-.25f,0,.5f},{-.4f,0,.3f},{-.5f,0,.1f},{-.52f,0,.05f},
                {-.15f,0,-.1f},{-.15f,0,-.45f},{-.15f,0,-.8f},{-.2f,0,-.9f},
                {.15f,0,-.1f},{.15f,0,-.45f},{.15f,0,-.8f},{.2f,0,-.9f}
            }};
            entity.skeleton.positions=pose;
            entity.skeleton.valid.fill(true);
            frame.entities.push_back(entity);
            frame.view[0]=.1f;
            for (unsigned pass=0;pass<7 && !outcome;++pass) {
                frame.entities[0].visibility=pass?esp::Visibility::Occluded:esp::Visibility::Visible;
                settings.box_style=pass?esp::BoxStyle::Corners:esp::BoxStyle::Default;
                settings.visual_enabled=pass<2;
                settings.aim.enabled=pass>=3;
                settings.aim.show_fov=settings.aim.show_target=true;
                const esp::AimVisual aim{pass==4?esp::Vec3{1,0,1}:esp::Vec3{},true,
                    pass==4?esp::Vec2{.6f,.4f}:esp::Vec2{.5f,.5f},pass!=5,pass==6};
                if (pass) {
                    renderer.reset();
                    if (FAILED(device->Reset(&parameters))) { outcome=fail("reset after releasing resources"); break; }
                }
                const D3DVIEWPORT9 original_viewport{7,9,780,580,0,1};
                const D3DVIEWPORT9 full_viewport{0,0,800,600,0,1};
                device->SetViewport(&full_viewport);
                device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff102030,1,0);
                device->SetViewport(&original_viewport);
                device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
                device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
                device->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
                if (FAILED(device->BeginScene())) { outcome=fail("begin test scene"); break; }
                const auto previous_stats=renderer.stats();
                const auto camera=frame.view;
                if (pass==1) frame.view.fill(0);
                const bool drawn=renderer.draw(device.get(),frame,settings,pass==1?esp::DeviceOwnership::Exclusive:esp::DeviceOwnership::Borrowed,
                                               pass==1?&camera:nullptr,pass>=3?&aim:nullptr);
                frame.view=camera;
                const HRESULT ended=device->EndScene();
                if (!drawn || FAILED(ended)) { outcome=fail("draw test scene"); break; }
                const auto stats=renderer.stats();
                if (pass<2 && (stats.frame_total!=frame.entities.size() || stats.frame_visible!=1 ||
                               stats.projection_rejected || stats.size_rejected || stats.screen_rejected || stats.target_skipped)) {
                    outcome=fail("renderer diagnostics agree with visible test player"); break;
                }
                if (pass==1 && (stats.state_blocks!=previous_stats.state_blocks || stats.draw_calls<=previous_stats.draw_calls)) {
                    outcome=fail("owned renderer draws without creating a state block"); break;
                }
                if (pass==2 && (stats.state_blocks!=previous_stats.state_blocks || stats.font_uploads!=previous_stats.font_uploads ||
                               stats.draw_calls!=previous_stats.draw_calls)) {
                    outcome=fail("disabled renderer does not touch graphics resources"); break;
                }
                DWORD zwrite{},blend{},cull{};
                D3DVIEWPORT9 viewport{};
                device->GetRenderState(D3DRS_ZWRITEENABLE,&zwrite);
                device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);
                device->GetRenderState(D3DRS_CULLMODE,&cull);
                device->GetViewport(&viewport);
                if (pass!=1 && (zwrite!=TRUE || blend!=FALSE || cull!=D3DCULL_CW || std::memcmp(&viewport,&original_viewport,sizeof(viewport)))) {
                    outcome=fail("D3D render states and viewport restored"); break;
                }
                esp::ComPtr<IDirect3DSurface9> backbuffer,copy;
                D3DSURFACE_DESC description{};
                if (FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,backbuffer.put())) ||
                    FAILED(backbuffer->GetDesc(&description)) ||
                    FAILED(device->CreateOffscreenPlainSurface(800,600,description.Format,D3DPOOL_SYSTEMMEM,copy.put(),nullptr)) ||
                    FAILED(device->GetRenderTargetData(backbuffer.get(),copy.get()))) { outcome=fail("copy rendered pixels"); break; }
                D3DLOCKED_RECT pixels{};
                if (FAILED(copy->LockRect(&pixels,nullptr,D3DLOCK_READONLY))) { outcome=fail("lock rendered pixels"); break; }
                const auto sample=[&](UINT x,UINT y){return *reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(pixels.pBits)+y*pixels.Pitch+x*4)&0x00ffffffU;};
                if (pass<2) {
                    const auto box_color=pass?settings.box_colors.occluded:settings.box_colors.visible;
                    const auto skeleton_color=pass?settings.skeleton_colors.occluded:settings.skeleton_colors.visible;
                    const auto snapline_color=pass?settings.snapline_colors.occluded:settings.snapline_colors.visible;
                    const auto name_color=pass?settings.name_colors.occluded:settings.name_colors.visible;
                    if (sample(373,pass?245:280)!=(box_color&0x00ffffffU) || sample(366,330)!=(settings.health_color&0x00ffffffU)) outcome=fail("box and health bar pixels");
                    if (pass && sample(373,280)!=0x00102030U) outcome=fail("corner box omits the middle of each edge");
                    if (sample(360,330)!=(settings.armor_color&0x00ffffffU)) outcome=fail("armor bar adjacent to health pixels");
                    if (sample(400,270)!=(skeleton_color&0x00ffffffU)) outcome=fail("animated skeleton torso pixels");
                    if (sample(399,570)!=(snapline_color&0x00ffffffU)) outcome=fail("snapline pixels from screen bottom");
                    bool name_pixels=false;
                    for (UINT y=365;y<379;++y) for (UINT x=345;x<455;++x) if (sample(x,y)==(name_color&0x00ffffffU)) name_pixels=true;
                    if (!name_pixels) outcome=fail("player name below feet");
                    if (sample(16,16)!=0x00102030U || sample(400,0)==0x00102030U || sample(380,0)!=0x00102030U)
                        outcome=fail("compact top indicator replaces the information panel");
                    if (!pass && !save_bitmap(argv[2],pixels,800,600)) outcome=fail("save rendered test bitmap");
                } else if (pass==3) {
                    if (sample(393,300)!=(settings.aim.target_color&0xffffffU) ||
                        sample(469,300)==0x00102030U || sample(366,330)!=0x00102030U)
                        outcome=fail("aim-only target marker and FOV render while ESP remains disabled");
                } else if (pass==4) {
                    if (sample(473,240)!=(settings.aim.target_color&0xffffffU) ||
                        sample(549,240)==0x00102030U || sample(393,300)!=0x00102030U || sample(469,300)!=0x00102030U)
                        outcome=fail("FOV and target mark share shifted HUD crosshair with no circle left at screen center");
                } else if (pass==5) {
                    if (sample(393,300)!=0x00102030U || sample(469,300)!=0x00102030U)
                        outcome=fail("invalid HUD anchor hides FOV and target marker");
                } else if (pass==6) {
                    if (sample(393,300)!=0x0055ee77U) outcome=fail("aligned Tap target uses the green feedback marker");
                } else if (sample(373,245)!=0x00102030U || sample(366,330)!=0x00102030U ||
                           sample(360,330)!=0x00102030U || sample(400,0)!=0x00102030U || sample(399,570)!=0x00102030U)
                    outcome=fail("master enabled switch suppresses every visual");
                copy->UnlockRect();
                if (!outcome) std::printf("PASS D3D9 draw, state restore and pixels (pass %u)\n",pass+1);
            }
            renderer.reset();
        }
    }
    DestroyWindow(window);
    UnregisterClassW(window_class.lpszClassName,instance);
    return outcome;
}
