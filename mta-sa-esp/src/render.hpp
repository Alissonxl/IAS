#pragma once
#include "game.hpp"
#include <d3d9.h>
#include <string_view>
#include <unordered_map>

namespace esp {
enum class DeviceOwnership { Borrowed,Exclusive };
struct RenderStats {
    std::uint64_t state_blocks{},font_uploads{},draw_calls{};
    std::size_t frame_total{},frame_visible{},projection_rejected{},size_rejected{},screen_rejected{};
    bool target_skipped{};
};
class Renderer {
public:
    bool draw(IDirect3DDevice9* device, const Frame& frame, const Settings& settings,DeviceOwnership ownership=DeviceOwnership::Borrowed,
              const ViewMatrix* camera=nullptr,const AimVisual* aim=nullptr);
    void reset() noexcept;
    RenderStats stats() const noexcept { return stats_; }
private:
    struct Vertex { float x,y,z,rhw; D3DCOLOR color; float u,v; };
    bool make_font(IDirect3DDevice9* device);
    void rect(float x,float y,float w,float h,D3DCOLOR color);
    void box(float x,float y,float w,float h,float t,D3DCOLOR color);
    void corner_box(float x,float y,float w,float h,float t,D3DCOLOR color);
    void line(Vec2 from,Vec2 to,float thickness,D3DCOLOR color);
    void text(float x,float y,std::wstring_view value,D3DCOLOR color);
    void text(float x,float y,std::string_view value,D3DCOLOR color);
    void glyph(float x,float y,wchar_t value,D3DCOLOR color);
    void add_glyphs(std::wstring_view value);
    ComPtr<IDirect3DTexture9> font_;
    IDirect3DDevice9* font_device_{};
    std::vector<Vertex> solid_,glyphs_;
    std::wstring characters_;
    std::unordered_map<wchar_t,std::size_t> glyph_index_;
    std::size_t font_count_{};
    RenderStats stats_;
};
}
