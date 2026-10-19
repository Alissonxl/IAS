#include "render.hpp"
#include "memory.hpp"
#include "separate_overlay.hpp"
#include "panel.hpp"
#include "frame_state.hpp"
#include "startup_gate.hpp"
#include "mta_cursor.hpp"
#include "aim.hpp"
#include "crosshair.hpp"
#include <atomic>
#include <algorithm>
#include <bit>
#include <memory>
#include <mutex>

namespace esp {
void set_log_path(const std::wstring& path);
namespace {
constexpr std::size_t reset_index=16,end_scene_index=42;
using EndScene=HRESULT(WINAPI*)(IDirect3DDevice9*);
using Reset=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
class SettingsSnapshot {
public:
    std::shared_ptr<const Settings> load() const {
        std::lock_guard lock(mutex_);
        return value_;
    }
    void store(std::shared_ptr<const Settings> value) {
        std::lock_guard lock(mutex_);
        value_=std::move(value);
    }
private:
    mutable std::mutex mutex_;
    std::shared_ptr<const Settings> value_;
};
struct Runtime {
    std::uintptr_t base{},table{};
    SettingsSnapshot settings;
    std::atomic<bool> enabled{true},stopped{false},resetting{false};
    std::atomic<unsigned long> callbacks{},selected{},collections{};
    std::mutex draw_mutex;
    Renderer renderer;
    MtaReader mta_reader;
    MtaCursor cursor_input;
    AimController aim_controller;
    std::atomic<bool> aim_permitted{};
    std::atomic<bool> aim_input_failed{};
    std::shared_ptr<const PublishedFrame> latest;
    EndScene original_end{};
    Reset original_reset{};
    std::wstring last_status;
};
// These objects live until process termination: callbacks must never point at an unloaded DLL.
std::atomic<Runtime*> runtime{};

bool main_device(Runtime& state,IDirect3DDevice9* device) {
    std::uintptr_t current{};
    const auto settings=state.settings.load();
    return read(state.base+settings->layout.device,current) && current==reinterpret_cast<std::uintptr_t>(device);
}
HRESULT WINAPI on_end_scene(IDirect3DDevice9* device) noexcept {
    Runtime& state=*runtime.load();
    state.callbacks.fetch_add(1,std::memory_order_relaxed);
    if (main_device(state,device) && !state.resetting.load()) {
        state.cursor_input.tick();
        state.selected.fetch_add(1,std::memory_order_relaxed);
        std::unique_lock lock(state.draw_mutex,std::try_to_lock);
        if (lock.owns_lock()) {
            if (state.stopped.load() || (!state.enabled.load() && !state.settings.load()->aim.enabled)) {
                state.aim_controller.suspend();
                state.latest.reset();
                state.renderer.reset();
            }
            else try {
                const auto settings=state.settings.load();
                bool drawn=true;
                const auto now=GetTickCount64();
                auto next=state.latest?std::make_shared<PublishedFrame>(*state.latest):std::make_shared<PublishedFrame>();
                next->settings=settings;
                if (!state.latest || state.latest->needs_collection(settings,now)) {
                    const auto players=state.mta_reader.collect(settings->mta);
                    next->frame=std::make_shared<const Frame>(collect_frame(state.base,*settings,&players));
                    next->collected_at=now;
                    state.collections.fetch_add(1,std::memory_order_relaxed);
                    if (next->frame->status!=state.last_status) { log_line(next->frame->status); state.last_status=next->frame->status; }
                    ComPtr<IDirect3DSwapChain9> chain;
                    D3DPRESENT_PARAMETERS parameters{};
                    if (FAILED(device->GetSwapChain(0,chain.put())) || FAILED(chain->GetPresentParameters(&parameters))) {
                        state.latest.reset();
                        drawn=false;
                    } else {
                        next->target=parameters.hDeviceWindow;
                        if (!next->target) {
                            D3DDEVICE_CREATION_PARAMETERS creation{};
                            if (SUCCEEDED(device->GetCreationParameters(&creation))) next->target=creation.hFocusWindow;
                        }
                        next->windowed=parameters.Windowed!=FALSE;

                    }
                }
                if (drawn) {
                    if (!read(state.base+settings->layout.view,next->view) ||
                        !std::all_of(next->view.begin(),next->view.end(),[](float value){return std::isfinite(value);})) {
                        state.latest.reset();
                    } else {
                        next->timestamp=GetTickCount64();
                        const auto anchor=read_crosshair(state.base,settings->aim);
                        next->aim={}; next->aim.center=anchor.ratio; next->aim.center_valid=anchor.valid;
                        RECT aim_area{};
                        if (GetClientRect(next->target,&aim_area)) {
                            const auto& aim=settings->aim;
                            const auto step=state.aim_controller.update(*next->frame,next->view,*settings,
                                static_cast<float>(aim_area.right),static_cast<float>(aim_area.bottom),next->collected_at,
                                next->timestamp,(GetAsyncKeyState(static_cast<int>(aim.key))&0x8000)!=0,
                                aim.switch_key && (GetAsyncKeyState(static_cast<int>(aim.switch_key))&0x8000)!=0,
                                anchor.valid && settings->aim.enabled && state.aim_permitted.load() && state.cursor_input.game_input_available() &&
                                GetForegroundWindow()==GetAncestor(next->target,GA_ROOT),anchor.ratio,anchor.aiming);
                            next->aim=step.visual; next->aim.center_valid=anchor.valid;
                            if (step.active && (step.dx || step.dy)) {
                                INPUT input{}; input.type=INPUT_MOUSE; input.mi.dx=step.dx; input.mi.dy=step.dy; input.mi.dwFlags=MOUSEEVENTF_MOVE;
                                if (SendInput(1,&input,sizeof(input))!=1) {
                                    state.aim_permitted.store(false);
                                    if (!state.aim_input_failed.exchange(true)) log_line(L"Mira: SendInput rejeitado; desative e reative Mira para tentar novamente");
                                }
                            }
                        }
                        state.latest=std::move(next);
                    }
                }
                if (settings->separate_overlay) state.renderer.reset();
                else if (drawn && state.latest) drawn=state.renderer.draw(device,*state.latest->frame,*settings,DeviceOwnership::Borrowed,&state.latest->view,&state.latest->aim);
                if (!drawn) {
                    log_line(L"Falha ao criar/desenhar recursos DX9; ESP desativado. Use Ativado no painel para tentar novamente.");
                    state.enabled.store(false);
                    state.renderer.reset();
                }
            } catch (...) {
                log_line(L"Excecao no frame; ESP desativado.");
                state.enabled.store(false);
                state.latest.reset();
                state.renderer.reset();
            }
        }
    }
    return state.original_end(device);
}
HRESULT WINAPI on_reset(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* parameters) noexcept {
    Runtime& state=*runtime.load();
    const bool selected=main_device(state,device);
    if (selected) {
        state.resetting.store(true);
        std::lock_guard lock(state.draw_mutex);
        state.latest.reset();
        state.renderer.reset();
    }
    const HRESULT result=state.original_reset(device,parameters);
    if (selected) {
        state.resetting.store(false);
        log_line(SUCCEEDED(result)?L"Dispositivo DX9 restaurado":L"Reset DX9 falhou; aguardando recuperacao");
    }
    return result;
}
bool replace_slot(std::uintptr_t address,void* expected,void* replacement) {
    MEMORY_BASIC_INFORMATION info{};
    if (address%alignof(void*) || VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info))!=sizeof(info)) return false;
    const DWORD writable=(info.Protect&0xf0U)?PAGE_EXECUTE_READWRITE:PAGE_READWRITE;
    DWORD old{};
    if (!VirtualProtect(reinterpret_cast<void*>(address),sizeof(void*),writable,&old)) return false;
    void* previous=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(address),replacement,expected);
    DWORD ignored{};
    if (!VirtualProtect(reinterpret_cast<void*>(address),sizeof(void*),old,&ignored)) log_line(L"Falha ao restaurar protecao da vtable");
    return previous==expected;
}
bool install(Runtime& state,std::uintptr_t table) {
    std::uintptr_t end{},reset{};
    if (!read(table+end_scene_index*sizeof(void*),end) || !executable(end) ||
        !read(table+reset_index*sizeof(void*),reset) || !executable(reset)) return false;
    state.table=table;
    state.original_end=std::bit_cast<EndScene>(end);
    state.original_reset=std::bit_cast<Reset>(reset);
    const auto reset_slot=table+reset_index*sizeof(void*),end_slot=table+end_scene_index*sizeof(void*);
    if (!replace_slot(reset_slot,reinterpret_cast<void*>(reset),std::bit_cast<void*>(&on_reset))) return false;
    if (!replace_slot(end_slot,reinterpret_cast<void*>(end),std::bit_cast<void*>(&on_end_scene))) {
        if (!replace_slot(reset_slot,std::bit_cast<void*>(&on_reset),reinterpret_cast<void*>(reset))) log_line(L"Rollback do Reset falhou; callback permanece em modo de encaminhamento");
        return false;
    }
    return true;
}
bool valid_image(std::uintptr_t base,const Settings& settings) {
    const auto& layout=settings.layout;
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS32 nt{};
    if (!read(base,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0 || dos.e_lfanew>0x100000 ||
        !read(base+static_cast<std::uintptr_t>(dos.e_lfanew),nt) || nt.Signature!=IMAGE_NT_SIGNATURE ||
        nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 || nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC) return false;
    const auto size=nt.OptionalHeader.SizeOfImage;
    const auto& visibility=settings.visibility;
    return size>64 && layout.device<size-4 && layout.pool<size-4 && layout.local<size-4 && layout.view<size-64 && settings.bones.skin_plugin<size-4 &&
        ((!visibility.camera && !visibility.line_clear) || (visibility.camera && visibility.line_clear &&
          visibility.camera<size-12 && visibility.line_clear<size));
}
}
DWORD WINAPI worker(void* module) noexcept {
    try {
        const auto dll_path=module_path(static_cast<HMODULE>(module));
        if (dll_path.empty()) return 1;
        const auto stem=dll_path.substr(0,dll_path.find_last_of(L'.'));
        const auto ini_path=stem+L".ini";
        set_log_path(stem+L".log");
        log_line(L"Versao tap-menu-v8 | PID="+std::to_wstring(GetCurrentProcessId())+L" | DLL="+dll_path);
        Settings settings{};
        std::wstring error,digest;
        if (!load_settings(ini_path,settings,error)) { log_line(L"INI: "+error); return 2; }
        const auto exe_path=module_path(nullptr);
        const auto basename=exe_path.substr(exe_path.find_last_of(L"\\/")+1);
        if (_wcsicmp(basename.c_str(),L"gta_sa.exe")!=0) { log_line(L"Processo incorreto; esperado gta_sa.exe"); return 3; }
        if (!sha256_file(exe_path,digest)) { log_line(L"Nao foi possivel calcular SHA256 do executavel"); return 4; }
        if (digest!=settings.sha256) { log_line(L"Hash nao corresponde ao INI; nenhum hook instalado. SHA256="+digest); return 5; }
        auto state=std::make_unique<Runtime>();
        state->base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!valid_image(state->base,settings)) { log_line(L"Layout fora da imagem PE x86"); return 6; }
        log_line(L"Base do executavel desta sessao="+std::to_wstring(state->base));
        state->settings.store(std::make_shared<const Settings>(settings));
        state->enabled.store(settings.visual_enabled);
        state->mta_reader.refresh(settings.mta);
        const auto deadline=GetTickCount64()+settings.wait_ms;
        std::uintptr_t device{},table{};
        log_line(L"Hash confirmado. Aguardando dispositivo DX9.");
        while (GetTickCount64()<deadline) {
            if (read(state->base+settings.layout.device,device) && device && read(device,table) && table) break;
            table=0;
            Sleep(settings.poll_ms);
        }
        if (!table) { log_line(L"Tempo esgotado aguardando ponteiro DX9; confirme device_rva"); return 7; }
        HMODULE pinned{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                               reinterpret_cast<LPCWSTR>(module),&pinned)) { log_line(L"Falha ao fixar vida util da DLL"); return 8; }
        Runtime* active=state.release();
        runtime.store(active);
        log_line(L"Hash confirmado; callbacks ainda nao instalados. Aguardando inicializacao completa do MTA.");
        SeparateOverlay overlay(static_cast<HINSTANCE>(module));
        Panel panel(static_cast<HINSTANCE>(module));
        panel.sync(settings,settings.visual_enabled);
        log_line(settings.separate_overlay?L"Desenho em janela separada; exige modo janela/sem bordas. Captura do desktop pode incluir overlay.":L"Desenho interno: os elementos podem aparecer nas capturas do jogo.");
        bool exclusive_reported=false;
        std::shared_ptr<const PublishedFrame> presented;
        bool was_reload=false,was_stop=false,was_panel=false;
        ULONGLONG next_module_check{},next_diagnostic{};
        ULONGLONG next_input_check{};
        bool input_ready=false;
        std::wstring input_status;
        StartupGate startup;
        auto startup_state=StartupGate::State::Modules;
        bool startup_reported=false,installed=false;
        std::uintptr_t bound_device{};
        while (!active->stopped.load()) {
            overlay.pump();
            panel.pump();
            Settings edited{};
            if (panel.take_changes(edited)) {
                if (panel.take_enabled_change()) active->enabled.store(edited.visual_enabled);
                else edited.visual_enabled=active->enabled.load();
                panel.sync(edited,edited.visual_enabled);
                active->settings.store(std::make_shared<const Settings>(std::move(edited)));
            }
            auto current=active->settings.load();
            const auto now=GetTickCount64();
            if (now>=next_module_check) {
                active->mta_reader.refresh(current->mta);
                next_module_check=now+current->module_check_ms;
            }
            if (!input_ready && installed && now>=next_input_check) {
                input_ready=active->cursor_input.prepare(stem+L"-input.ini",error);
                const auto status=input_ready?L"Cursor MTA: interface verificada; controles suspensos enquanto painel aberto":L"Cursor MTA: "+error;
                if (status!=input_status) { log_line(status); input_status=status; }
                next_input_check=now+5000;
            }
            if (!installed) {
                std::uintptr_t local{};
                device=table=0;
                const bool modules_ready=active->mta_reader.ready(current->mta);
                const bool player_ready=read(active->base+current->layout.local,local) && local;
                if (!read(active->base+current->layout.device,device) || !device || !read(device,table)) device=table=0;
                const auto sampled=startup.sample(modules_ready,player_ready,device,table,GetTickCount64());
                if (!startup_reported || sampled!=startup_state) {
                    const auto message=sampled==StartupGate::State::Modules?L"Inicializacao: aguardando modulos MTA verificados":
                        sampled==StartupGate::State::Player?L"Inicializacao: aguardando personagem local":
                        sampled==StartupGate::State::Device?L"Inicializacao: aguardando dispositivo DX9 atual":
                        sampled==StartupGate::State::Settling?L"Inicializacao: aguardando estabilizacao do dispositivo":
                        L"Inicializacao: MTA e dispositivo prontos";
                    log_line(message); panel.status(message);
                    startup_state=sampled; startup_reported=true;
                }
                if (sampled==StartupGate::State::Ready) {
                    if (!install(*active,table)) {
                        active->stopped.store(true); log_line(L"Falha ao instalar callbacks DX9 no dispositivo atual");
                        continue;
                    }
                    installed=true; bound_device=device;
                    log_line(L"Callbacks DX9 instalados apos inicializacao do MTA | dispositivo="+
                        std::to_wstring(bound_device)+L" tabela="+std::to_wstring(table));
                    panel.status(L"MTA pronto; aguardando primeira coleta.");
                }
            }
            if (panel.take_save()) {
                const bool saved=save_visual_settings(ini_path,*current,error);
                panel.mark_saved(saved);
                panel.status(saved?L"Perfil salvo no INI.":L"Falha ao salvar. Consulte o log.");
                log_line(saved?L"Painel: perfil visual salvo":L"Painel: "+error);
            }
            DWORD foreground_pid{};
            GetWindowThreadProcessId(GetForegroundWindow(),&foreground_pid);
            const bool focused=foreground_pid==GetCurrentProcessId();
            const auto pressed=[&](std::uint32_t key){return focused && !panel.capturing_key() && (GetAsyncKeyState(static_cast<int>(key))&0x8000)!=0;};
            const bool reload=pressed(current->reload_key),stop=pressed(current->stop_key),open_panel=pressed(current->panel_key);
            if (reload && !was_reload) {
                Settings next{};
                if (!load_settings(ini_path,next,error)) log_line(L"Recarga rejeitada: "+error);
                else if (next.sha256!=digest || next.layout.device!=current->layout.device) log_line(L"Recarga rejeitada: hash/device_rva exige reinicio");
                else if (!valid_image(active->base,next)) log_line(L"Recarga rejeitada: layout fora da imagem PE");
                else {
                    active->enabled.store(next.visual_enabled);
                    panel.sync(next,next.visual_enabled);
                    panel.mark_saved(true);
                    active->settings.store(std::make_shared<const Settings>(std::move(next))); log_line(L"INI recarregado");
                }
            }
            if (stop && !was_stop) active->stopped.store(true);
            current=active->settings.load();
            std::shared_ptr<const PublishedFrame> frame;
            {
                std::lock_guard lock(active->draw_mutex);
                frame=active->latest;
            }
            if (open_panel && !was_panel) {
                panel.sync(*active->settings.load(),active->enabled.load());
                if (!panel.toggle(frame?frame->target:GetForegroundWindow(),error)) log_line(L"Painel: "+error);
            }
            const auto presentation=!current->separate_overlay?L"interno":
                (!active->enabled.load() && !current->aim.enabled)?L"desativado":active->resetting.load()?L"reset":
                !frame?L"sem_frame":!frame->matches(current,GetTickCount64(),current->frame_timeout_ms)?L"frame_expirado":
                !frame->windowed?L"tela_cheia_exclusiva":
                GetForegroundWindow()!=GetAncestor(frame->target,GA_ROOT)?L"sem_foco":L"apresentar";
            if (current->separate_overlay && (active->enabled.load() || current->aim.enabled) && !active->stopped.load() &&
                !active->resetting.load() && frame && frame->matches(current,GetTickCount64(),current->frame_timeout_ms)) {
                if (!frame->windowed) {
                    overlay.hide();
                    if (!exclusive_reported) log_line(L"Overlay separado oculto: selecione modo janela ou sem bordas no MTA.");
                    exclusive_reported=true;
                } else if (GetForegroundWindow()!=GetAncestor(frame->target,GA_ROOT)) {
                    overlay.hide();
                } else {
                    exclusive_reported=false;
                    if ((frame!=presented || !IsWindowVisible(overlay.window())) &&
                        !overlay.present(frame->target,*frame->frame,*current,error,&frame->view,&frame->aim)) {
                        overlay.reset();
                        active->enabled.store(false);
                        log_line(L"Overlay separado desativado: "+error+L". Use Ativado no painel para tentar novamente.");
                    } else presented=frame;
                }
            } else overlay.hide();
            if (installed && now>=next_diagnostic) {
                log_line(L"Coleta | callbacks="+std::to_wstring(active->callbacks.load())+
                    L" selecionados="+std::to_wstring(active->selected.load())+
                    L" coletas="+std::to_wstring(active->collections.load())+
                    L" entidades="+std::to_wstring(frame?frame->frame->entities.size():0)+
                    L" ativado="+std::to_wstring(active->enabled.load()));
                const auto render=overlay.render_stats();
                log_line(L"Apresentacao | estado="+std::wstring(presentation)+
                    L" janela="+std::to_wstring(frame?reinterpret_cast<std::uintptr_t>(frame->target):0)+
                    L" foco="+std::to_wstring(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
                    L" idade_ms="+(frame?std::to_wstring(GetTickCount64()-frame->timestamp):L"sem_frame")+
                    L" overlay_visivel="+std::to_wstring(IsWindowVisible(overlay.window())!=FALSE)+
                    L" proprietaria="+std::to_wstring(reinterpret_cast<std::uintptr_t>(GetWindow(overlay.window(),GW_OWNER)))+
                    L" acima_jogo="+std::to_wstring(frame && window_above(overlay.window(),frame->target))+
                    L" enviados="+std::to_wstring(render.frame_total)+L" enquadrados="+std::to_wstring(render.frame_visible)+
                    L" rejeicao_projecao="+std::to_wstring(render.projection_rejected)+
                    L" rejeicao_tamanho="+std::to_wstring(render.size_rejected)+
                    L" fora_tela="+std::to_wstring(render.screen_rejected)+
                    L" alvo_ignorado="+std::to_wstring(render.target_skipped)+
                    L" centro_mira_valido="+std::to_wstring(frame && frame->aim.center_valid)+
                    L" centro_mira_x="+std::to_wstring(frame?frame->aim.center.x:0)+
                    L" centro_mira_y="+std::to_wstring(frame?frame->aim.center.y:0));
                panel.status(!active->enabled.load()?L"ESP desativado":
                    frame && frame->matches(current,GetTickCount64(),current->frame_timeout_ms)?
                    frame->frame->status+L" | Jogadores: "+std::to_wstring(frame->frame->entities.size()):
                    L"Sem coleta recente. Consulte o log.");
                next_diagnostic=now+5000;
            }
            if (current->visual_enabled!=active->enabled.load()) {
                auto next=*current; next.visual_enabled=active->enabled.load();
                panel.sync(next,next.visual_enabled);
                active->settings.store(std::make_shared<const Settings>(std::move(next)));
            }
            panel.follow_target();
            panel.above(overlay.window());
            active->cursor_input.request(frame?frame->target:GetForegroundWindow(),
                IsWindowVisible(panel.window()) && !active->stopped.load());
            if (!current->aim.enabled) active->aim_input_failed.store(false);
            active->aim_permitted.store(focused && !IsWindowVisible(panel.window()) && !panel.capturing_key() &&
                !active->aim_input_failed.load() && !active->stopped.load());
            was_reload=reload; was_stop=stop; was_panel=open_panel;
            Sleep(current->poll_ms);
        }
        active->aim_permitted.store(false);
        active->cursor_input.request(nullptr,false);
        panel.hide();
        overlay.hide();
        log_line(L"ESP encerrado. DLL permanece carregada para manter callbacks validos ate o processo fechar.");
        return 0;
    } catch (...) {
        if (auto* active=runtime.load()) active->stopped.store(true);
        log_line(L"Falha durante inicializacao; ESP encerrado");
        return 10;
    }
}
}




