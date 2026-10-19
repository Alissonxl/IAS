#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <string>
#include <utility>

namespace esp {
inline bool window_above(HWND front,HWND back) noexcept {
    front=GetAncestor(front,GA_ROOT); back=GetAncestor(back,GA_ROOT);
    if (!IsWindow(front) || !IsWindow(back) || front==back) return false;
    const bool front_top=(GetWindowLongPtrW(front,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
    const bool back_top=(GetWindowLongPtrW(back,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
    if (front_top!=back_top) return front_top;
    HWND previous=GetWindow(back,GW_HWNDPREV);
    for (unsigned i=0;previous && i<512;++i) {
        if (previous==front) return true;
        const HWND next=GetWindow(previous,GW_HWNDPREV);
        if (next==previous || next==back) break;
        previous=next;
    }
    return false;
}
class Handle {
public:
    explicit Handle(HANDLE value = nullptr) noexcept : value_(value) {}
    ~Handle() { if (valid()) CloseHandle(value_); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    bool valid() const noexcept { return value_ && value_ != INVALID_HANDLE_VALUE; }
    HANDLE get() const noexcept { return value_; }
private:
    HANDLE value_;
};
template<class T> class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    void reset() noexcept { if (ptr_) { ptr_->Release(); ptr_ = nullptr; } }
    T* get() const noexcept { return ptr_; }
    T** put() noexcept { reset(); return &ptr_; }
    T* operator->() const noexcept { return ptr_; }
private:
    T* ptr_{};
};
std::wstring module_path(HMODULE module);
bool sha256_file(const std::wstring& path, std::wstring& digest);
void log_line(const std::wstring& text) noexcept;
DWORD WINAPI worker(void* module) noexcept;
}
