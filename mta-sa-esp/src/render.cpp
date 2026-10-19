#include "render.hpp"
#include "visibility.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace esp {
namespace {
constexpr UINT atlas_side=512,cell_width=16,cell_height=24,columns=atlas_side/cell_width;
constexpr std::size_t maximum_glyphs=columns*(atlas_side/cell_height);
constexpr float glyph_advance=10;
constexpr float indicator_width=32,indicator_height=2;
constexpr D3DCOLOR indicator_color=0xa0909090;
constexpr DWORD vertex_format=D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1;
struct StateRestore {
    ComPtr<IDirect3DStateBlock9> block;
    bool active{};
    ~StateRestore() { if (active) block->Apply(); }
};
}
void Renderer::reset() noexcept { font_.reset(); font_device_=nullptr; font_count_=0; solid_.clear(); glyphs_.clear(); }
void Renderer::add_glyphs(std::wstring_view value) {
    for (wchar_t c:value) {
        if (c<32 || c==127 || glyph_index_.contains(c) || characters_.size()>=maximum_glyphs) continue;
        glyph_index_.emplace(c,characters_.size());
        characters_.push_back(c);
    }
}
bool Renderer::make_font(IDirect3DDevice9* device) {
    if (font_.get() && font_device_==device && font_count_==characters_.size()) return true;
    reset();
    HDC dc=CreateCompatibleDC(nullptr);
    if (!dc) return false;
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=atlas_side;
    info.bmiHeader.biHeight=-static_cast<LONG>(atlas_side);
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
    void* pixels{};
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    HFONT font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH|FF_MODERN,L"Consolas");
    if (!bitmap || !font || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (font) DeleteObject(font);
        DeleteDC(dc);
        return false;
    }
    const HGDIOBJ old_bitmap=SelectObject(dc,bitmap),old_font=SelectObject(dc,font);
    std::memset(pixels,0,atlas_side*atlas_side*4);
    SetTextColor(dc,RGB(255,255,255));
    SetBkMode(dc,TRANSPARENT);
    for (std::size_t i=0;i<characters_.size();++i) {
        TextOutW(dc,static_cast<int>(i%columns*cell_width+1),
                 static_cast<int>(i/columns*cell_height+1),&characters_[i],1);
    }
    GdiFlush();
    const auto* source=static_cast<const std::uint32_t*>(pixels);
    bool ok=SUCCEEDED(device->CreateTexture(atlas_side,atlas_side,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,
                                            D3DPOOL_DEFAULT,font_.put(),nullptr));
    D3DLOCKED_RECT locked{};
    if (ok) {
        ok=SUCCEEDED(font_->LockRect(0,&locked,nullptr,D3DLOCK_DISCARD));
        if (ok) {
            for (UINT y=0;y<atlas_side;++y) {
                auto* row=reinterpret_cast<std::uint32_t*>(static_cast<unsigned char*>(locked.pBits)+y*locked.Pitch);
                for (UINT x=0;x<atlas_side;++x) row[x]=((source[y*atlas_side+x]&0xffU)<<24)|0x00ffffffU;
            }
            ok=SUCCEEDED(font_->UnlockRect(0));
        }
    }
    SelectObject(dc,old_font);
    SelectObject(dc,old_bitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(dc);
    if (!ok) { reset(); return false; }
    font_device_=device;
    font_count_=characters_.size();
    ++stats_.font_uploads;
    return true;
}
void Renderer::rect(float x,float y,float w,float h,D3DCOLOR color) {
    if (w<=0 || h<=0) return;
    x-=0.5f; y-=0.5f;
    solid_.insert(solid_.end(),{{x,y,0,1,color,0,0},{x+w,y,0,1,color,0,0},{x+w,y+h,0,1,color,0,0},
                               {x,y,0,1,color,0,0},{x+w,y+h,0,1,color,0,0},{x,y+h,0,1,color,0,0}});
}
void Renderer::box(float x,float y,float w,float h,float t,D3DCOLOR color) {
    rect(x,y,w,t,color); rect(x,y+h-t,w,t,color);
    rect(x,y,t,h,color); rect(x+w-t,y,t,h,color);
}
void Renderer::corner_box(float x,float y,float w,float h,float t,D3DCOLOR color) {
    const float cw=w*.25f,ch=h*.25f;
    rect(x,y,cw,t,color); rect(x+w-cw,y,cw,t,color);
    rect(x,y+h-t,cw,t,color); rect(x+w-cw,y+h-t,cw,t,color);
    rect(x,y,t,ch,color); rect(x+w-t,y,t,ch,color);
    rect(x,y+h-ch,t,ch,color); rect(x+w-t,y+h-ch,t,ch,color);
}
void Renderer::line(Vec2 from,Vec2 to,float thickness,D3DCOLOR color) {
    const float dx=to.x-from.x,dy=to.y-from.y,length=std::hypot(dx,dy);
    if (!std::isfinite(length) || length<0.01f) return;
    const float px=-dy/length*thickness*0.5f,py=dx/length*thickness*0.5f;
    const Vertex a{from.x+px-0.5f,from.y+py-0.5f,0,1,color,0,0};
    const Vertex b{to.x+px-0.5f,to.y+py-0.5f,0,1,color,0,0};
    const Vertex c{to.x-px-0.5f,to.y-py-0.5f,0,1,color,0,0};
    const Vertex d{from.x-px-0.5f,from.y-py-0.5f,0,1,color,0,0};
    solid_.insert(solid_.end(),{a,b,c,a,c,d});
}
void Renderer::text(float x,float y,std::string_view value,D3DCOLOR color) {
    for (const unsigned char c:value) { glyph(x,y,static_cast<wchar_t>(c),color); x+=glyph_advance; }
}
void Renderer::text(float x,float y,std::wstring_view value,D3DCOLOR color) {
    for (const wchar_t c:value) { glyph(x,y,c,color); x+=glyph_advance; }
}
void Renderer::glyph(float x,float y,wchar_t value,D3DCOLOR color) {
        auto found=glyph_index_.find(value);
        if (found==glyph_index_.end()) found=glyph_index_.find(L'?');
        if (found!=glyph_index_.end()) {
            const auto index=found->second;
            const float u=static_cast<float>(index%columns*cell_width)/atlas_side;
            const float v=static_cast<float>(index/columns*cell_height)/atlas_side;
            const float u2=u+static_cast<float>(cell_width)/atlas_side;
            const float v2=v+static_cast<float>(cell_height)/atlas_side;
            const float x1=x-0.5f,y1=y-0.5f,x2=x1+cell_width,y2=y1+cell_height;
            glyphs_.insert(glyphs_.end(),{{x1,y1,0,1,color,u,v},{x2,y1,0,1,color,u2,v},{x2,y2,0,1,color,u2,v2},
                                         {x1,y1,0,1,color,u,v},{x2,y2,0,1,color,u2,v2},{x1,y2,0,1,color,u,v2}});
        }
}
bool Renderer::draw(IDirect3DDevice9* device,const Frame& frame,const Settings& settings,DeviceOwnership ownership,const ViewMatrix* camera,const AimVisual* aim) {
    const auto& view=camera?*camera:frame.view;
    stats_.frame_total=frame.entities.size();
    stats_.frame_visible=stats_.projection_rejected=stats_.size_rejected=stats_.screen_rejected=0;
    stats_.target_skipped=false;
    if (!settings.visual_enabled && !settings.aim.enabled) return true;
    if (!device) return false;
    // A state block returns its underlying D3D device, even when MTA passed a proxy.
    // Drawing and restoring on that device keeps the proxy's state cache consistent.
    StateRestore restore;
    ComPtr<IDirect3DDevice9> real_device;
    if (ownership==DeviceOwnership::Borrowed) {
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL,restore.block.put())) ||
            FAILED(restore.block->GetDevice(real_device.put()))) return false;
        restore.active=true;
        ++stats_.state_blocks;
        device=real_device.get();
    }
    ComPtr<IDirect3DSurface9> target,backbuffer;
    if (FAILED(device->GetRenderTarget(0,target.put())) ||
        FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,backbuffer.put()))) return false;
    if (target.get()!=backbuffer.get()) { stats_.target_skipped=true; return true; }
    D3DSURFACE_DESC description{};
    if (FAILED(backbuffer->GetDesc(&description)) || !description.Width || !description.Height) return false;
    const float width=static_cast<float>(description.Width),height=static_cast<float>(description.Height);
    if (characters_.empty()) {
        for (wchar_t c=32;c<127;++c) add_glyphs(std::wstring_view(&c,1));
    }
    for (const auto& entity:frame.entities) if (settings.names) add_glyphs(entity.name);
    if (!make_font(device)) return false;
    solid_.clear(); glyphs_.clear();
    rect((width-indicator_width)*0.5f,0,indicator_width,indicator_height,indicator_color);
    const Vec2 anchor=aim?aim->center:Vec2{.5f,.5f};
    const bool anchor_valid=(!aim || aim->center_valid) && std::isfinite(anchor.x) && std::isfinite(anchor.y) &&
        anchor.x>0 && anchor.x<1 && anchor.y>0 && anchor.y<1;
    const Vec2 aim_center{width*anchor.x,height*anchor.y};
    if (settings.aim.enabled && settings.aim.show_fov && anchor_valid) {
        constexpr int segments=96;
        const float radius=settings.aim.fov;
        for (int i=0;i<segments;++i) {
            const float a=static_cast<float>(i)*6.283185307f/segments,b=static_cast<float>(i+1)*6.283185307f/segments;
            line({aim_center.x+std::cos(a)*radius,aim_center.y+std::sin(a)*radius},
                 {aim_center.x+std::cos(b)*radius,aim_center.y+std::sin(b)*radius},1,0xa070d7ff);
        }
    }
    if (settings.aim.enabled && settings.aim.show_target && anchor_valid && aim && aim->selected) {
        Vec2 target_point{};
        if (project(aim->world,view,width,height,settings.near_clip,target_point)) {
            const auto target_color=aim->aligned?0xff55ee77U:settings.aim.target_color;
            box(target_point.x-7,target_point.y-7,14,14,2,target_color);
            line(aim_center,target_point,1,target_color);
        }
    }
    for (const auto& entity:frame.entities) {
        if (!settings.visual_enabled) break;
        if (entity.distance>settings.max_distance || !matches_drawing_type(entity.visibility,settings.drawing_type)) continue;
        Vec3 head=entity.position,feet=entity.position;
        head.z+=settings.head_offset; feet.z+=settings.foot_offset;
        Vec2 top{},bottom{};
        if (!project(head,view,width,height,settings.near_clip,top) ||
            !project(feet,view,width,height,settings.near_clip,bottom)) { ++stats_.projection_rejected; continue; }
        const float h=std::abs(bottom.y-top.y),w=h*settings.box_ratio;
        if (h<3 || h>height*4 || w<1) { ++stats_.size_rejected; continue; }
        const float x=(top.x+bottom.x-w)*0.5f,y=std::min(top.y,bottom.y);
        if (x+w<0 || x>width || y+h<0 || y>height) { ++stats_.screen_rejected; continue; }
        ++stats_.frame_visible;
        if (settings.boxes) {
            const auto draw_box=[&](float bx,float by,float bw,float bh,float t,D3DCOLOR color) {
                if (settings.box_style==BoxStyle::Corners) corner_box(bx,by,bw,bh,t,color);
                else box(bx,by,bw,bh,t,color);
            };
            draw_box(x-1,y-1,w+2,h+2,settings.thickness+2,0xc0101010);
            draw_box(x,y,w,h,settings.thickness,entity_color(settings.box_colors,entity.visibility));
        }
        if (settings.snaplines)
            line({width*.5f,height-1},{(top.x+bottom.x)*.5f,y+h},settings.snapline_thickness,
                 entity_color(settings.snapline_colors,entity.visibility));
        float label_y=y+h+2;
        if (settings.skeleton) {
            std::array<Vec2,bone_ids.size()> projected{};
            std::array<bool,bone_ids.size()> valid{};
            for (std::size_t i=0;i<projected.size();++i) {
                valid[i]=entity.skeleton.valid[i] && project(entity.skeleton.positions[i],view,width,height,settings.near_clip,projected[i]);
                if (valid[i]) label_y=std::max(label_y,projected[i].y+3);
            }
            for (const auto& link:bone_links) {
                if (!valid[link[0]] || !valid[link[1]]) continue;
                line(projected[link[0]],projected[link[1]],settings.skeleton_thickness+2,0xd0000000);
                line(projected[link[0]],projected[link[1]],settings.skeleton_thickness,entity_color(settings.skeleton_colors,entity.visibility));
            }
        }
        if (settings.health_bar && entity.health_known && entity.max_health>0) {
            const float fraction=std::clamp(entity.health/entity.max_health,0.0f,1.0f);
            rect(x-8,y-1,5,h+2,0xd0000000);
            rect(x-7,y+h*(1-fraction),3,h*fraction,settings.health_color);
        }
        if (settings.armor_bar && entity.armor_known && settings.max_armor>0) {
            const float fraction=std::clamp(entity.armor/settings.max_armor,0.0f,1.0f);
            const float ax=x-(settings.health_bar?14.0f:8.0f);
            rect(ax,y-1,5,h+2,0xd0101010);
            rect(ax+1,y+h*(1-fraction),3,h*fraction,settings.armor_color);
        }
        if (settings.names && !entity.name.empty()) {
            const float name_x=x+(w-static_cast<float>(entity.name.size())*glyph_advance)*0.5f;
            text(name_x+1,label_y+1,entity.name,0xff000000);
            text(name_x,label_y,entity.name,entity_color(settings.name_colors,entity.visibility));
            label_y+=cell_height;
        }
        if (settings.distance_label) {
            char label[64]{};
            std::snprintf(label,sizeof(label),entity.streamed?"%.0fm":"%.0fm [cache]",static_cast<double>(entity.distance));
            const float label_x=x+(w-static_cast<float>(std::strlen(label))*glyph_advance)*0.5f;
            text(label_x+1,label_y+1,label,0xff000000);
            text(label_x,label_y,label,entity_color(settings.distance_colors,entity.visibility));
            label_y+=cell_height;
        }
        if (settings.labels && entity.health_known) {
            char label[64]{};
            if (entity.armor_known) std::snprintf(label,sizeof(label),"HP %.0f | AP %.0f",static_cast<double>(entity.health),static_cast<double>(entity.armor));
            else std::snprintf(label,sizeof(label),"HP %.0f",static_cast<double>(entity.health));
            const float label_x=x+(w-static_cast<float>(std::strlen(label))*glyph_advance)*.5f;
            text(label_x+1,label_y+1,label,0xff000000);
            text(label_x,label_y,label,0xffffffff);
        }
    }
    const D3DVIEWPORT9 viewport{0,0,description.Width,description.Height,0,1};
    bool ok=SUCCEEDED(device->SetViewport(&viewport));
    ok=SUCCEEDED(device->SetVertexShader(nullptr)) && ok;
    ok=SUCCEEDED(device->SetPixelShader(nullptr)) && ok;
    ok=SUCCEEDED(device->SetFVF(vertex_format)) && ok;
    const auto state=[&](D3DRENDERSTATETYPE key,DWORD value){ok=SUCCEEDED(device->SetRenderState(key,value))&&ok;};
    state(D3DRS_ZENABLE,FALSE); state(D3DRS_ZWRITEENABLE,FALSE); state(D3DRS_ALPHATESTENABLE,FALSE);
    state(D3DRS_ALPHABLENDENABLE,TRUE); state(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); state(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    state(D3DRS_BLENDOP,D3DBLENDOP_ADD); state(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
    state(D3DRS_CULLMODE,D3DCULL_NONE); state(D3DRS_FILLMODE,D3DFILL_SOLID); state(D3DRS_LIGHTING,FALSE);
    state(D3DRS_FOGENABLE,FALSE); state(D3DRS_STENCILENABLE,FALSE); state(D3DRS_SCISSORTESTENABLE,FALSE);
    state(D3DRS_SRGBWRITEENABLE,FALSE); state(D3DRS_COLORWRITEENABLE,0x0f); state(D3DRS_CLIPPLANEENABLE,0);
    const auto stage=[&](DWORD index,D3DTEXTURESTAGESTATETYPE key,DWORD value){ok=SUCCEEDED(device->SetTextureStageState(index,key,value))&&ok;};
    stage(1,D3DTSS_COLOROP,D3DTOP_DISABLE); stage(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
    stage(0,D3DTSS_TEXCOORDINDEX,0); stage(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
    stage(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1); stage(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    stage(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1); stage(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    ok=SUCCEEDED(device->SetTexture(0,nullptr))&&ok;
    if (!ok) return false;
    if (!solid_.empty()) {
        if (FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,static_cast<UINT>(solid_.size()/3),solid_.data(),sizeof(Vertex)))) return false;
        ++stats_.draw_calls;
    }
    stage(0,D3DTSS_COLOROP,D3DTOP_MODULATE); stage(0,D3DTSS_COLORARG1,D3DTA_TEXTURE); stage(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    stage(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE); stage(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE); stage(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
    ok=SUCCEEDED(device->SetTexture(0,font_.get()))&&ok;
    ok=SUCCEEDED(device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR))&&ok;
    ok=SUCCEEDED(device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR))&&ok;
    ok=SUCCEEDED(device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE))&&ok;
    ok=SUCCEEDED(device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,0))&&ok;
    ok=SUCCEEDED(device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP))&&ok;
    ok=SUCCEEDED(device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP))&&ok;
    if (!ok) return false;
    if (!glyphs_.empty()) {
        if (FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,static_cast<UINT>(glyphs_.size()/3),glyphs_.data(),sizeof(Vertex)))) return false;
        ++stats_.draw_calls;
    }
    return true;
}
}
