# Painel, colete e opcoes visuais

Implementacao de 2026-10-18, revisada conforme o mockup aprovado. Painel Win32/GDI+ sem moldura nativa, tema escuro, azul, cantos arredondados e tres colunas: Elementos, Ajustes e Previa. A previa acompanha interruptores, caixas, paletas visivel/oculto, vida, colete, valores, nome, distancia e snapline. Ela usa um personagem ilustrativo, sem dados reais. Nao depende de ImGui.

## Comportamento e foco

- INSERT abre/fecha; a caixa Ativado controla o ESP. Abrir, fechar ou editar outras opcoes nao muda a ativacao.
- F5 recarrega o perfil; END encerra ate reiniciar. `panel_vk=0x2D` e o padrao; atalhos ativos devem ser diferentes. O antigo `toggle_vk` e ignorado.
- A janela usa `WS_EX_NOACTIVATE`, `SW_SHOWNOACTIVATE`, `SWP_NOACTIVATE` e `WM_MOUSEACTIVATE=MA_NOACTIVATE`. Nao ha chamadas SetForegroundWindow/SetFocus no painel.
- Dropdowns, paleta e sliders RGB sao desenhados na mesma janela; nao ha ChooseColor ou popups nativos que recebam foco.
- Hover, sliders com arraste, cabecalho arrastavel e Salvar perfil funcionam por mouse. Ao soltar fora da janela, o arraste termina. O tamanho se ajusta a area util ao abrir; o arraste fica limitado ao monitor.
- O painel acompanha o alvo: ao minimizar/trocar de aplicativo se oculta; quando o alvo volta ao primeiro plano, reaparece se estava aberto. A janela do ESP fica abaixo do painel.
- Falhas de desenho desmarcam Ativado; edicoes em cores/filtros nao reativam um renderer que falhou.

Use janela ou janela sem bordas. A preservacao de foco foi exercitada com janelas auxiliares no Windows; esta versao ainda nao foi validada em uma sessao real do MTA. A integracao com o cursor e as regras de entrada do jogo precisa ser conferida nessa sessao.

## Perfil

- `ped_armor=0x548`: campo float adjacente a vida/maximo no CPed GTA SA US 1.0.
- `camera_position_rva=0x76F9CC`: posicao da matriz da camera (TheCamera + 0x974 + 0x30).
- `line_clear_rva=0x16A490`: CWorld::GetIsLineOfSightClear, chamada cdecl com dois vetores por referencia e sete booleanos. E executada no callback do dispositivo do jogo, dentro do perfil de executavel ja validado por SHA256.
- `element_dimension=0x64`: confirmado no getter virtual de CClientPlayer do client.dll local, RVA 0x353F0: `66 8B 41 64 C3`.
- `ped_cached_position=0x354`: ramo sem ped/veiculo/anexo de CClientPed::GetPosition, RVA 0x78D1B, le vetor em +0x354/+0x35C. E tratado como cache, sem promessa de atualidade.
- As leituras MTA continuam exigindo os hashes de client.dll e game_sa.dll existentes no perfil.

Nao houve validacao desta versao numa sessao real do MTA. A resolucao de dimensao e posicao em cache foi conferida estaticamente no binario instalado; a logica foi exercitada com estruturas sinteticas. A funcao de visibilidade real exige confirmacao em sessao; os testes usam uma funcao substituta com a mesma assinatura para exercitar os dois resultados e o estado desconhecido.

## Verificacoes

Build LLVM-MinGW x86 com C++20 e `-Wall -Wextra -Wpedantic -Werror`. Passaram 115 verificacoes: 54 de memoria/configuracao/entidades/agendamento, 34 do painel, 14 do overlay separado, tres passagens graficas, um transporte de hook no auxiliar e nove do atualizador. Testes do painel exercitam abertura e fechamento sem ativacao, checkbox mestre, controles que nao reativam o ESP, limites do slider, menus, paleta, canais RGB, salvamento e preservacao do perfil, visibilidade por alvo e destruicao da janela. As imagens foram inspecionadas: `bin/panel-check.png` foi renderizada pelo codigo real; `bin/panel-colors.png` mostra o seletor interno. O mockup aprovado fica em `design/painel-aprovado.png`.

Biblioteca do painel: `gdiplus`, alem de `user32` e `gdi32`. MSVC nao foi executado. Os arquivos anteriores foram preservados em `build/before-panel-v2`. A atualizacao local fica em `bin/pendente`, para aplicar pelo BAT apos fechar o GTA/MTA; o ZIP leva os binarios novos diretamente.

## Referencias

- [Microsoft: WS_EX_NOACTIVATE](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles).
- [Microsoft: MA_NOACTIVATE permite o clique sem ativar](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mouseactivate).

- [MTA CPedSA: campos de vida e armadura](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/game_sa/CPedSA.h).
- [Plugin-SDK: assinatura e endereco da linha de visao](https://github.com/DK22Pac/plugin-sdk/blob/master/plugin_sa/game_sa/CWorld.cpp).
- [Plugin-SDK: offsets da matriz da camera](https://github.com/DK22Pac/plugin-sdk/blob/master/plugin_sa/game_sa/CCamera.h).
- [MTA CClientEntity: semantica da dimensao](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/mods/deathmatch/logic/CClientEntity.h).
- [Microsoft: retorno da API de gravacao/flush do INI](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-writeprivateprofilestringw).
