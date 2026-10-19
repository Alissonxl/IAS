#pragma once
#include "render.hpp"

namespace esp {
// The window, device and renderer belong exclusively to the calling worker thread.
class SeparateOverlay {
public:
    explicit SeparateOverlay(HINSTANCE instance) noexcept : instance_(instance) {}
    ~SeparateOverlay();
    SeparateOverlay(const SeparateOverlay&)=delete;
    SeparateOverlay& operator=(const SeparateOverlay&)=delete;
    bool present(HWND target,const Frame& frame,const Settings& settings,std::wstring& error,const ViewMatrix* camera=nullptr,const AimVisual* aim=nullptr);
    void hide() noexcept;
    void pump() noexcept;
    void reset() noexcept { release(); }
    HWND window() const noexcept { return window_; }
    RenderStats render_stats() const noexcept { return renderer_.stats(); }
private:
    bool create(HWND owner,std::wstring& error);
    void release() noexcept;
    bool resize(UINT width,UINT height,std::wstring& error);
    static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam);
    HINSTANCE instance_{};
    HWND window_{};
    ATOM atom_{};
    ComPtr<IDirect3D9Ex> d3d_;
    ComPtr<IDirect3DDevice9Ex> device_;
    Renderer renderer_;
    D3DPRESENT_PARAMETERS parameters_{};
    RECT position_{};
};
}
