#pragma once
#include "visual_options.hpp"

namespace Gdiplus { class Bitmap; class Graphics; }

namespace esp {
class Panel {
public:
    explicit Panel(HINSTANCE instance) noexcept : instance_(instance) {}
    ~Panel();
    Panel(const Panel&)=delete;
    Panel& operator=(const Panel&)=delete;
    bool toggle(HWND target,std::wstring& error);
    void hide() noexcept;
    void pump() noexcept;
    void follow_target() noexcept;
    void above(HWND overlay) noexcept;
    void sync(const Settings& settings,bool enabled);
    bool take_changes(Settings& settings);
    bool take_enabled_change() noexcept { return std::exchange(enabled_change_,false); }
    bool take_save() noexcept { return std::exchange(save_,false); }
    void mark_saved(bool success) noexcept { if (success) unsaved_=false; invalidate(); }
    bool has_unsaved_changes() const noexcept { return unsaved_; }
    void status(std::wstring text);
    HWND window() const noexcept { return window_; }
    bool capturing_key() const noexcept { return key_capture_!=0; }
private:
    bool create(std::wstring& error);
    void release() noexcept;
    void paint(HDC dc);
    void paint_aim(Gdiplus::Graphics& graphics);
    void click_aim(int x,int y);
    void capture_key(int slot);
    void accept_key(std::uint32_t key);
    void click(int x,int y);
    void move(int x,int y);
    void finish_drag() noexcept;
    void slide(int x);
    void invalidate() noexcept;
    void cursor_at(int x,int y,bool visible) noexcept;
    void changed();
    bool adjust_control(int x,int y,int direction,bool reset,bool fine=false,bool coarse=false);
    std::wstring footer_text() const;
    static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam);
    HINSTANCE instance_{};
    HWND window_{},target_{};
    bool interactive_{};
    ULONG_PTR graphics_token_{};
    Gdiplus::Bitmap* buffer_{};
    Gdiplus::Bitmap* composition_{};
    POINT cursor_{};
    bool cursor_visible_{};
    int tab_{},key_capture_{};
    std::array<bool,256> key_previous_{};
    bool content_dirty_{true};
    ATOM atom_{};
    Settings values_;
    bool dirty_{},save_{},enabled_change_{},unsaved_{},positioned_{},save_shortcut_down_{},binding_error_{};
    int wheel_remainder_{};
    bool open_{},tracking_{},dragging_{};
    int hover_x_{-1},hover_y_{-1},popup_{},slider_{-1};
    POINT drag_origin_{},window_origin_{};
    float scale_{1};
    std::uint32_t* selected_color_{};
    std::wstring status_{L"Altera\u00e7\u00f5es ao vivo"};
};
}
