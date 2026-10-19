#pragma once
#include "memory.hpp"
#include "cursor_lease.hpp"
#include <atomic>
#include <memory>
#include <array>
#include <mutex>

namespace esp {
class MtaCursor {
    struct Binding {
        HMODULE client{},core{};
        std::uintptr_t slot{},table{},object{},gui_offset{};
        std::array<std::uint32_t,6> indices{},functions{};
        ~Binding() { if (client) FreeLibrary(client); if (core) FreeLibrary(core); }
        bool valid() const {
            std::uintptr_t current{},vtable{},gui{};
            if (!read(slot,current) || current!=object || !read(object,vtable) || vtable!=table ||
                !read(object+gui_offset,gui) || !gui) return false;
            for (std::size_t i=0;i<indices.size();++i) {
                std::uintptr_t fn{};
                if (!read(table+indices[i]*sizeof(std::uintptr_t),fn) ||
                    fn!=reinterpret_cast<std::uintptr_t>(core)+functions[i]) return false;
            }
            return true;
        }
        bool visible() const { return getter(0); }
        bool controls() const { return getter(1); }
        bool getter(std::size_t i) const {
            using Get=bool(__thiscall*)(void*);
            return reinterpret_cast<Get>(reinterpret_cast<std::uintptr_t>(core)+functions[i])(reinterpret_cast<void*>(object));
        }
        void force(bool visible,bool controls) const {
            using Set=void(__thiscall*)(void*,bool,bool);
            reinterpret_cast<Set>(reinterpret_cast<std::uintptr_t>(core)+functions[2])(reinterpret_cast<void*>(object),visible,controls);
        }
    };
public:
    // Resolve and hash once on the worker; invoke only on the game's render/UI thread.
    bool prepare(const std::wstring& path,std::wstring& error) {
        { std::lock_guard lock(binding_mutex_); if (binding_) return true; }
        auto binding=std::make_shared<Binding>();
        if (!GetModuleHandleExW(0,L"client.dll",&binding->client) || !GetModuleHandleExW(0,L"core.dll",&binding->core)) {
            error=L"aguardando client/core"; return false;
        }
        const auto number=[&](const wchar_t* key,std::uint32_t& value) {
            wchar_t text[40]{}; GetPrivateProfileStringW(L"input",key,L"",text,40,path.c_str());
            wchar_t* end{}; const auto parsed=std::wcstoul(text,&end,0);
            if (!*text || !end || *end || parsed>0x10000000) return false;
            value=static_cast<std::uint32_t>(parsed); return true;
        };
        const auto verified=[&](HMODULE module,const wchar_t* key) {
            wchar_t expected[66]{}; GetPrivateProfileStringW(L"input",key,L"",expected,66,path.c_str());
            std::wstring actual;
            return wcslen(expected)==64 && sha256_file(module_path(module),actual) && _wcsicmp(expected,actual.c_str())==0;
        };
        std::uint32_t slot{},table{},gui{};
        if (!verified(binding->client,L"client_sha256") || !verified(binding->core,L"core_sha256") ||
            !number(L"core_pointer_rva",slot) || !slot || !number(L"core_vtable_rva",table) || !table ||
            !number(L"local_gui_offset",gui) || gui>4096 ||
            !number(L"visible_index",binding->indices[0]) || !number(L"controls_index",binding->indices[1]) ||
            !number(L"force_index",binding->indices[2]) ||
            !number(L"visible_rva",binding->functions[0]) || !number(L"controls_rva",binding->functions[1]) ||
            !number(L"force_rva",binding->functions[2]) ||
            !number(L"chat_index",binding->indices[3]) || !number(L"chat_rva",binding->functions[3]) ||
            !number(L"menu_index",binding->indices[4]) || !number(L"menu_rva",binding->functions[4]) ||
            !number(L"settings_index",binding->indices[5]) || !number(L"settings_rva",binding->functions[5])) {
            error=L"perfil de entrada ausente, invalido ou hash divergente"; return false;
        }
        for (const auto index:binding->indices) if (index>128) { error=L"indice de entrada fora do limite"; return false; }
        binding->slot=reinterpret_cast<std::uintptr_t>(binding->client)+slot;
        binding->table=reinterpret_cast<std::uintptr_t>(binding->core)+table;
        binding->gui_offset=gui;
        if (!read(binding->slot,binding->object) || !binding->object || !binding->valid()) {
            error=L"interface de cursor ainda indisponivel ou diferente do perfil"; return false;
        }
        { std::lock_guard lock(binding_mutex_); binding_=std::move(binding); }
        error.clear(); return true;
    }
    void request(HWND target,bool wanted) noexcept { if (target) target_.store(target); wanted_.store(wanted); }
    bool game_input_available() {
        std::shared_ptr<Binding> binding;
        { std::lock_guard lock(binding_mutex_); binding=binding_; }
        const HWND target=target_.load();
        return binding && target && GetWindowThreadProcessId(target,nullptr)==GetCurrentThreadId() &&
            binding->valid() && !binding->visible() && !binding->getter(3) && !binding->getter(4) && !binding->getter(5);
    }
    void tick() {
        std::shared_ptr<Binding> binding;
        { std::lock_guard lock(binding_mutex_); binding=binding_; }
        if (!binding) return;
        const HWND target=target_.load();
        if (!target || GetWindowThreadProcessId(target,nullptr)!=GetCurrentThreadId()) return;
        if (!binding->valid()) { lease_.abandon(); return; }
        lease_.update(*binding,wanted_.load() && GetForegroundWindow()==GetAncestor(target,GA_ROOT));
    }
private:
    std::mutex binding_mutex_;
    std::shared_ptr<Binding> binding_;
    std::atomic<bool> wanted_{};
    std::atomic<HWND> target_{};
    CursorLease lease_;
};
}
