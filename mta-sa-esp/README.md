# ESP injetavel para MTA:SA

A entrega tap-menu-v8 acrescenta ajustes precisos pela roda, restauracao com botao direito, ajuda contextual, aviso persistente de alteracoes nao salvas, Ctrl+S e memoria da posicao do painel durante a sessao. A aba Mira inclui calibracao de ganho do mouse e modo Tap manual com estabilizacao. Tiro e troca de arma continuam manuais. Veja MELHORIAS_MENU_TAP.md para controles, testes e limites.

A entrega crosshair-fov-v7 alinha FOV, selecao e movimento com a referencia da mira do HUD, em vez do centro fixo da janela. Distingue camera de ombro e mira centralizada e acompanha a resolucao. Veja CORRECAO_FOV_MIRA.md.

A entrega motion-cursor-v1 atualiza a camera em cada frame, separa a validade da publicacao da coleta e desenha o cursor acima do painel. O perfil usa coleta de 16 ms e espera de 5 ms, com reaproveitamento dos dados e do bitmap da interface. Veja `CORRECAO_MOVIMENTO_CURSOR.md` para testes e limites.

A entrega overlay-owner-v1 cria o overlay como popup pertencente a janela raiz do jogo. Um teste de regressao demonstrou que a versao anterior podia ficar atras do jogo entre apresentacoes e que o vinculo preserva a ordem mesmo apos repetidas promocoes da janela do jogo. Veja `CORRECAO_PROPRIEDADE_JANELA.md`.

A entrega overlay-display-v1 tambem corrige a ordem do overlay sobre a janela do jogo e registra diagnosticos de apresentacao/projecao. Evidencias, testes e limitacoes em `CORRECAO_APRESENTACAO.md`.

A entrega atual mantem a leitura original do painel aprovado e ajusta a ordem de inicializacao: o primeiro vinculo DX9 aguarda os modulos MTA verificados, o personagem local e a estabilizacao do dispositivo da sessao. Detalhes e limites em `CORRECAO_INICIALIZACAO.md`.

DLL C++20 x86 com esqueleto animado, nome abaixo dos pes, caixas 2D, barra de vida e distancia. O topo mostra somente um indicador cinza de 32 x 2 pixels, centralizado. Diagnosticos e atalhos nao ocupam a tela; os estados continuam no arquivo de log. O carregador padrao usa `SetWindowsHookExW(WH_GETMESSAGE)` na thread de uma janela do GTA, com confirmacao de entrada por evento.

## Revisao de estabilidade e desempenho

Coleta completa a cada `collect_interval_ms` (padrao 25 ms), com atualizacao imediata apos mudar o painel. Hashes dos modulos sao conferidos no worker, fora do callback grafico. Frames de configuracoes anteriores sao rejeitados. Sem dimensao local confirmada, o filtro de mesma dimensao suspende o desenho. O caminho exclusivo do overlay dispensa state blocks e a leitura dos ossos dispensa alocacoes por personagem. O atualizador confere DLL e INI antes de trocar, preserva backups e tenta rollback em falhas detectadas.

Detalhes das falhas, testes e limites: `REVISAO_OTIMIZACAO.md`. A simulacao de 200 callbacks produziu 40 coletas; o ganho de FPS no jogo ainda nao foi medido.

## Painel visual e colete

**INSERT abre/fecha o painel**. A caixa **Ativado** controla o ESP; abrir ou fechar o painel preserva esse estado. F5 recarrega o INI e END encerra o worker. O visual aprovado usa tema escuro com azul, tres colunas, hover, cabecalho arrastavel, seletores de cores com paleta e sliders RGB, menus internos e previa que acompanha as opcoes. O painel tem controles de nome, distancia, caixa, snapline, esqueleto, dimensoes, vida, colete e valores HP/AP. As mudancas sao aplicadas imediatamente; **Salvar perfil** grava apenas as opcoes visuais e cores no INI, preservando offsets e hashes. A gravacao usa um arquivo temporario validado e substituicao atomica. Se falhar, o painel informa e o motivo fica no log.

A barra azul de colete fica ao lado da barra verde de vida, a esquerda da caixa. Cada barra pode ser desligada ou receber outra cor. `max_armor=100` define a escala do colete. Colete zero e uma leitura valida: a barra fica vazia. Uma leitura invalida omite a barra de colete sem descartar a vida valida.

Nome, distancia, caixa, snapline e esqueleto possuem dois seletores: **Vis.** e **Oculto**. A checagem usa `CWorld::GetIsLineOfSightClear` da camera para o corpo e a cabeca, considerando edificios, veiculos e objetos; outros peds nao bloqueiam os raios. Qualquer raio livre classifica como visivel; ambos bloqueados classificam como oculto. Falha na checagem ou jogador em outra dimensao recebe cinza, sem inventar um resultado.

**Tipo de desenho** oferece Todos, Visiveis e Ocultos. Resultados desconhecidos aparecem somente em Todos. As caixas oferecem Padrao e Cantos. Snapline liga a parte inferior central da tela aos pes. O controle de distancia vai de 1 a 3000 metros.

Com **Todas dimensoes** desativado, o filtro exige a mesma dimensao do personagem local quando os metadados MTA estao disponiveis. Ativado, tambem pode incluir jogadores fora do streaming cujas posicoes ainda estejam no cache local, marcados `[cache]`. Esses registros nao mostram vida, colete, ossos ou visibilidade inventados. O recurso nao recupera dados que o servidor nao enviou; posicoes em cache podem estar desatualizadas.

O painel e uma janela pertencente ao jogo, exibida sem ativacao para preservar o foco do MTA. Todos os controles, inclusive menus e seletor RGB, sao desenhados na propria janela: nao abrem dialogos externos. Ao trocar de aplicativo, o painel fica oculto e reaparece quando o jogo volta ao primeiro plano. O cabecalho permite arrastar e o tamanho inicial se ajusta a area util do monitor. `panel_vk` permite alterar o atalho (padrao `0x2D`, INSERT). O antigo `toggle_vk` nao e mais utilizado. Use modo janela ou sem bordas; a sobreposicao em tela cheia exclusiva nao e suportada.

Validacao e perfil dos novos campos: `VALIDACAO_PAINEL.md`. Previa do painel: `bin/panel-check.png`.

## Desenho separado e capturas

O perfil agora habilita `separate_overlay=1`. Nesse modo, EndScene apenas coleta os dados; caixas, esqueleto, nomes, vida, distancia e indicador sao apresentados por `SeparateOverlay::present`, em uma janela transparente com dispositivo Direct3D 9Ex proprio. Nenhum desses desenhos e enviado ao backbuffer do jogo. O codigo publico de `CScreenShot` do MTA usa `GetBackBufferPixels` para a captura interna:

[MTA: CScreenShot.cpp](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/core/CScreenShot.cpp)

Use **modo janela ou janela sem bordas**. Em tela cheia exclusiva, o overlay fica oculto e o motivo aparece no log. Falha do overlay desativa o desenho e desmarca Ativado; marque novamente no painel para tentar outra vez. F5 recarrega o perfil.

```ini
[capture]
separate_overlay=1
frame_timeout_ms=250
```

`frame_timeout_ms` limita a idade de um frame antes de esconder o overlay (50 a 2000 ms). Perda de foco, janela minimizada e END tambem o escondem. Os recursos pertencem a thread do worker e sao liberados antes de destruir a janela. O dispositivo 9Ex usa sua propria fabrica, para nao inicializar outro dispositivo pelo proxy Direct3DCreate9 do MTA.

`separate_overlay=0` restaura o desenho interno anterior, que pode aparecer nas capturas. Capturas do desktop, Print Screen e gravadores de tela podem incluir a janela separada. Esta mudanca nao modifica a rotina de captura do MTA nem garante comportamento de ferramentas de terceiros. A captura interna do MTA instalado ainda precisa ser conferida em uma sessao; o teste automatizado usa uma janela auxiliar e dispositivos reais, sem abrir o jogo.

Depois de substituir a DLL, reinicie o GTA/MTA. F5 atualiza configuracoes, mas nao carrega codigo de uma DLL nova.

Nesta entrega, a DLL da pasta de trabalho estava bloqueada para escrita. A DLL e o INI novos estao em `bin/pendente`: feche o GTA/MTA e execute `injetar.bat` para aplicar a troca. Se o bloqueio persistir, o BAT informa o erro e preserva os arquivos. O ZIP atualizado ja leva a DLL e o INI novos diretamente em `bin`.

```powershell
.\bin\mta-sa-overlay-checks.exe "$PWD\mta-sa-esp.ini"
```

O teste verifica apresentacao separada, janela transparente sem ativacao, passagem de cliques, igualdade byte a byte do backbuffer auxiliar antes/depois, redimensionamento, minimizar/restaurar e teardown. O teste de renderizacao existente continua conferindo pixels e estados graficos. Detalhes em `VALIDACAO_CAPTURA.md`.

## Atualizacao desta versao

Se a DLL anterior estiver em uso, os arquivos novos ficam em `bin/pendente`. Feche o GTA/MTA e execute `injetar.bat`: o BAT aplica a atualizacao antes de abrir o jogo. Se o Windows ainda negar a substituicao, o BAT informa a falha e preserva a atualizacao para outra tentativa. Nao encerra processos automaticamente. O ZIP ja contem os novos binarios em `bin`.

O INI inclui `skeleton=1`, `names=1` e `boxes=1` em `[visual]`. Para deixar apenas o esqueleto, use `boxes=0`; barra de vida e informacoes abaixo do nome continuam independentes, por `health_bar` e `labels`. `skeleton_thickness` ajusta as linhas. F5 recarrega o INI depois que a nova DLL estiver carregada.

O nome e o texto de nametag do proprio MTA, inclusive quando o servidor personaliza esse texto. UTF-8 e convertido para Unicode; marcadores `#RRGGBB` sao removidos. Se o nome nao estiver disponivel, ele e omitido. Nao e substituido por um ID ou nome inventado.

Os ossos usam as matrizes RenderWare da animacao atual, identificadas pelas tags de cabeca, pescoco, tronco, bracos, maos, quadris, joelhos, tornozelos e pes. Nao chama funcoes do jogo para forcar atualizacao. Modelos sem hierarquia valida ou ossos fora do limite configurado sao omitidos. Animacoes que o proprio jogo nao atualiza podem fornecer a ultima pose calculada.

## Usar os binarios

1. Mantenha `bin/mta-sa-esp.dll`, `bin/mta-sa-loader.exe`, `bin/mta-sa-esp.ini` e `bin/carregar.cmd` juntos.
2. Execute `injetar.bat` na raiz ou em `bin`. O BAT confere a DLL e o INI; se o GTA estiver fechado, abre o inicializador do MTA e aguarda o processo do jogo. Se uma instancia ja estiver aberta, usa essa instancia. Havendo duas ou mais instancias, encerra sem escolher arbitrariamente.
3. O carregador informa se o callback exportado da DLL confirmou entrada no processo. O worker inicia fora de `DllMain`; o estado do ESP e registrado em `bin/mta-sa-esp.log`. A confirmacao do hook, sozinha, nao comprova que o pool de jogadores esteja disponivel.

Para conferir os arquivos sem carregar a DLL: `injetar.bat /verificar`. O `carregar.cmd` anterior continua disponivel em `bin`.

### Inicializacao automatica

O perfil fornecido inclui:

```ini
[launcher]
path=C:\Program Files (x86)\MTA San Andreas 1.6\Multi Theft Auto.exe
timeout_ms=90000
poll_ms=250
ready_delay_ms=3000
```

O caminho foi encontrado nesta maquina. Edite `[launcher] path` no INI de `bin` se o MTA for movido. O carregador abre `Multi Theft Auto.exe` pelo mecanismo normal do Windows, aguarda `gta_sa.exe` por ate 90 segundos e aguarda mais 3 segundos apos a deteccao. Em seguida, confere arquitetura e permissoes e tenta o carregamento normal da DLL. Se o jogo fechar ou mudar durante a espera, a tentativa encerra. Nao abre uma segunda instancia se ja existir uma.

A abertura automatica nao altera direitos de acesso. O modo padrao `--hook` usa consulta/espera do processo e a API de hooks da janela; nao solicita `PROCESS_VM_OPERATION`, `PROCESS_VM_WRITE` nem `PROCESS_CREATE_THREAD`. O metodo anterior `--auto` continua disponivel e mantem a verificacao de direitos antes de alocar memoria.

Os prazos da rota de hook sao editaveis no INI:

```ini
[loader]
window_timeout_ms=30000
hook_timeout_ms=10000
hook_poll_ms=200
```

Para validar somente a configuracao de abertura, sem iniciar o jogo:

```powershell
.\bin\mta-sa-loader.exe --verificar-inicializacao "$PWD\bin\mta-sa-esp.ini"
```

Para consultar os niveis de privilegio sem tentar injetar:

```powershell
.\injetar.bat /diagnosticar
```

O modo de diagnostico consulta tokens e o caminho do processo usando acesso limitado. Ele nao aloca memoria, escreve no processo nem cria thread remota. Se o carregador tiver integridade menor que o jogo, confira se o GTA/MTA foi iniciado como administrador e use o mesmo nivel de privilegio nos dois. Se os niveis forem iguais, o erro 5 sozinho nao identifica a causa do bloqueio. O carregador nao tenta contornar uma negacao de acesso.

O diagnostico tambem consulta, por `NtQueryObject(ObjectBasicInformation)`, os direitos concedidos a um handle com a mascara usada pelo metodo anterior de alocacao remota. Ausencia desses direitos continua bloqueando esse metodo com codigo 3. Essa mascara nao e a exigencia do novo modo `--hook`. Consulte `DIAGNOSTICO_ACESSO.md` para o resultado observado.

Com ambos os processos no mesmo nivel de integridade, confira o mecanismo de alocacao fora do jogo:

De dois cliques em `autoteste.bat` na raiz ou na pasta `bin`. Esse arquivo seleciona o modo de teste automaticamente. Tambem e possivel executar:

```powershell
.\injetar.bat /autoteste
```

Esse modo cria um auxiliar oculto do proprio carregador e usa a mesma mascara de `OpenProcess` para alocar uma pagina de dados, escrever e reler um marcador e liberar a pagina. O auxiliar encerra pelo evento de conclusao, com prazo maximo de 15 segundos. Nao carrega DLL, nao executa codigo remoto e nao abre o GTA/MTA. Sucesso nesse teste demonstra funcionamento dessas APIs nesse auxiliar; nao prova a causa do bloqueio no jogo nem a compatibilidade da DLL com o MTA.

Para testar a nova rota, de dois cliques em `autoteste-hook.bat`. Esse teste cria uma janela auxiliar oculta e confirma a execucao de uma DLL de prova nessa thread por meio de evento. Ele nao usa o jogo.

Atalhos com o jogo em primeiro plano:

| Tecla | Acao |
| --- | --- |
| INSERT | Abre/fecha o painel; Ativado controla o desenho |
| F5 | Rele o INI ao lado da DLL |
| END | Encerra o ESP ate reiniciar o jogo |

Para escolher um PID quando houver mais de uma instancia:

```powershell
.\bin\mta-sa-loader.exe --hook 1234 "C:\caminho\mta-sa-esp\bin\mta-sa-esp.dll"
```

O carregador seleciona somente processos chamados `gta_sa.exe`. Ele usa as permissoes normais do processo Windows; falhas de acesso ou carregamento sao exibidas com o erro correspondente. O manifesto usa o nivel de privilegio herdado do usuario.

## Perfil fornecido e limites

- MTA informado pelo usuario: `1.6-release-24149`.
- Executavel lido: `C:\ProgramData\MTA San Andreas All\1.6\GTA San Andreas\gta_sa.exe`.
- SHA256: `77485627B4EF17F92819318050D501E171C7AB84CEFFE5091B973B9E29F9CC98`.
- PE i386, base preferida `0x400000`; marcador de versao em VA `0x748ADD`: `FF 53`, correspondente a US 1.0 no identificador do MTA.
- O carregamento da DLL e os callbacks DX9 foram confirmados no processo MTA/GTA, PID 16732, em 2026-10-18. O log ficou em `Aguardando personagem local`; a leitura do pool e a exibicao das caixas sobre jogadores ainda precisam de validacao com um personagem em sessao.
- A leitura usa o pool GTA de personagens carregados no cliente. Elementos fora desse pool nao aparecem.
- Quando a lista MTA esta disponivel, `players_only=1` exige a associacao exata entre o jogador MTA e seu personagem GTA. Se o mod ainda nao estiver carregado ou o perfil nao corresponder aos modulos, permanece o filtro anterior de `CPed::m_pPlayerData`, que tambem pode incluir peds de scripts; nomes ficam indisponiveis. Use `players_only=0` para mostrar todos os personagens elegiveis do pool.
- A lista MTA exige SHA256 correspondente de `client.dll` e `game_sa.dll`, vtable de jogador correspondente, vetor limitado e vinculo unico por ponteiro nativo. O INI registra os valores conferidos nos arquivos instalados nesta maquina. Atualizacoes do MTA podem exigir outro perfil.
- IDs, equipes e dimensoes do MTA nao sao lidos. Personagens mortos sao omitidos. As caixas continuam usando altura aproximada configuravel; o esqueleto usa as matrizes individuais dos ossos.
- END desativa a logica e libera os recursos graficos no proximo callback. A DLL e fixada no processo, com os callbacks encaminhando aos originais, ate o jogo fechar. Nao ha auto-unload durante chamadas graficas em andamento.

Edite **o INI dentro de `bin`** para mudar o comportamento dos binarios. O INI na raiz e o modelo copiado durante o build; recompilar substitui a copia em `bin`.

## Compilar

O projeto nao depende de ImGui, MinHook ou do SDK legado D3DX. O texto usa um atlas GDI e o desenho usa Direct3D 9.

Com LLVM-MinGW x86:

```powershell
.\build.ps1 -Compiler "C:\llvm-mingw\bin\i686-w64-mingw32-clang++.exe"
```

Nesta maquina, `build.ps1` tambem encontra o compilador portatil em `../.tools/llvm-mingw-20261006-ucrt-x86_64`. O compilador nao integra o ZIP do projeto.

Com Visual Studio e o workload de C++ instalado:

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

O CMake gera os binarios em `build/Release`; use o `carregar.cmd` desse diretorio. A compilacao MSVC nao foi executada neste ambiente.

Build obrigatoriamente x86. Flags: C++20; LLVM `-Wall -Wextra -Wpedantic -Werror`; MSVC `/W4 /WX /permissive- /EHsc /utf-8`. Bibliotecas: `d3d9`, `user32`, `gdi32`, `bcrypt`, `advapi32`, `shell32`, `comctl32`, `comdlg32`, alem das bibliotecas de sistema padrao. Subsistema Windows para a DLL, console para carregador e testes. Nao requer instalacao de driver nem registro de servico.

## Verificacao realizada

Compilacao LLVM-MinGW 20261006 concluida para DLL e executaveis i386, com runtime C++ estatico e imports de sistema Windows/UCRT. Vinte e sete verificacoes de memoria, projecao, INI, pool, nomes UTF-8, associacao MTA e hierarquia de ossos passaram.

O teste grafico criou uma janela oculta e um dispositivo Direct3D 9 HAL, desenhou uma entidade sintetica com esqueleto e nome acentuado abaixo dos pes, conferiu pixels da caixa, vida, tronco, nome e indicador discreto, verificou restauracao de estados/viewport e repetiu apos Reset do dispositivo. As duas passagens passaram. Imagem resultante: `bin/render-check.png`; nao e uma captura do MTA.

```powershell
.\bin\mta-sa-checks.exe "$PWD\mta-sa-esp.ini"
.\bin\mta-sa-render-checks.exe "$PWD\mta-sa-esp.ini" "$PWD\bin\render-check.bmp"
```

O teste `mta-sa-hook-checks.exe` confirmou o transporte por hook em uma janela auxiliar. Depois, o carregador abriu o MTA e confirmou entrada da DLL no GTA (PID 16732). O log confirmou hash, callbacks DX9 instalados e dispositivo restaurado. Ainda nao houve validacao visual sobre jogadores. Evidencia: `VALIDACAO_HOOK.md`.

A extensao de esqueleto e nomes foi testada com dados sinteticos e desenho DX9. Na consulta somente de leitura ao jogo aberto durante esta atualizacao, `client.dll` ainda nao estava na lista de modulos; nao foi possivel confirmar nomes e ossos de jogadores reais nessa sessao. A nova DLL permanece pronta para a proxima abertura. Detalhes do perfil: `VALIDACAO_ESQUELETO.md`.

## Primeiro sintoma e diagnostico

| Sintoma / log | Verificacao |
| --- | --- |
| `SetWindowsHookExW` falhou | Confira a mensagem Windows, arquitetura x86, thread/janela e niveis de integridade. Nenhuma confirmacao de entrada foi obtida. |
| Hook aceito, sem confirmacao no prazo | A API retornou um hook, mas o callback da DLL nao sinalizou entrada; o carregador nao informa sucesso. |
| `LoadLibraryW retornou NULL` | Confirme DLL x86 e dependencias; o ESP ainda nao inicializou. |
| `VirtualAllocEx` / erro 5 | Execute `injetar.bat /diagnosticar`. O codigo significa acesso negado; offsets e INI nao corrigem a permissao de alocacao remota. |
| Sem log | Confirme acesso de escrita a pasta da DLL; os diagnosticos tambem usam `OutputDebugStringW`. |
| `Hash nao corresponde ao INI` | O executavel e diferente do perfil. Nenhum callback e instalado. |
| `Tempo esgotado aguardando ponteiro DX9` | O dispositivo ainda nao existe ou `device_rva` nao corresponde ao layout. |
| `Callbacks DX9 instalados`, mas nenhum HUD | O caminho de EndScene pode nao estar usando a vtable selecionada; a integracao com essa sessao permanece pendente. |
| `Aguardando personagem local` | Entre em uma sessao e confirme `local_ped_rva`. |
| Falha de pool ou capacidade | Confirme `ped_pool_rva`, campos do pool, stride e limite de slots. |
| HUD aparece, mas caixas nao | Confirme personagens carregados, distancia, filtro de player data, matriz e offsets. |
| Sem nomes | Consulte o log para `MTA: aguardando mod`, hash divergente ou vinculo invalido. O nome so aparece quando a associacao MTA foi validada. |
| Sem esqueleto | Confirme `skeleton=1`, personagem carregado, clump skinned e matrizes dentro de `max_bone_distance`. |
| Faixa cinza sem informacoes | Comportamento esperado: o painel superior foi removido; diagnosticos ficam no log. |
| Desenho desativado apos falha DX9 | Consulte o log; marque Ativado no painel para tentar novamente. |

## Mecanismo

`DllMain` apenas registra o modulo e desativa notificacoes de thread. O callback `EspMessageHook` chama `EspInitialize`, que fixa a vida util da DLL e inicia o worker uma unica vez. O hook e restrito a uma thread da janela do PID selecionado, encaminha as mensagens sem modifica-las e e removido apos confirmacao ou falha. O evento confirma entrada do callback; o log do worker confirma a inicializacao do ESP.

O worker valida o hash do executavel e os limites da imagem PE e aguarda o dispositivo. A instalacao troca atomicamente as entradas Reset/EndScene da vtable existente e preserva os alvos originais. O metodo anterior de `LoadLibraryW` chama explicitamente `EspInitialize` depois de carregar a biblioteca.

Cada coleta faz leituras limitadas no proprio processo, diferencia leitura zero de falha, ignora slots livres e reconfirma a geracao do slot antes de aceitar a entidade. A projecao segue a matriz RenderWare com bias de viewport. Ao usar o dispositivo do jogo, o renderer obtem o dispositivo real por state block e preserva seus estados. O overlay separado usa seu dispositivo exclusivo sem essa captura de estados. Recursos de pool default sao liberados antes de Reset.

## Referencias de layout

- [MTA: identificacao da versao GTA](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/game_sa/CGameSA.cpp)
- [MTA: projecao de coordenadas](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/core/Graphics/CGraphics.cpp)
- [MTA: proxy Direct3D 9](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/core/DXHook/CProxyDirect3DDevice9.cpp)
- [MTA: tags de ossos](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/sdk/game/CPed.h)
- [MTA: estruturas de hierarquia e matrizes](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/sdk/game/RenderWare.h)
- [MTA: jogador e texto de nametag](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/mods/deathmatch/logic/CClientPlayer.h)
- [MTA: enumeracao dos nomes](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/mods/deathmatch/CClient.cpp)
- [Plugin-SDK: pool de personagens](https://github.com/DK22Pac/plugin-sdk/blob/master/plugin_sa/game_sa/CPools.cpp)
- [Plugin-SDK: estrutura do pool](https://github.com/DK22Pac/plugin-sdk/blob/master/plugin_sa/game_sa/CPool.h)
- [Plugin-SDK: campos CPed](https://github.com/DK22Pac/plugin-sdk/blob/master/plugin_sa/game_sa/CPed.h)
- [Plugin-SDK: tamanho do slot CCopPed](https://github.com/DK22Pac/plugin-sdk/blob/master/plugin_sa/game_sa/CCopPed.h)
- [LLVM-MinGW oficial](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006)
- [Microsoft: SetWindowsHookExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw)
- [Microsoft: callback GetMsgProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/getmsgproc)






