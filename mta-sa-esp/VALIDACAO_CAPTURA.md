# Validacao do desenho separado

Data: 2026-10-18.

Compilacao LLVM-MinGW x86 concluida com C++20, `-Wall -Wextra -Wpedantic -Werror`. Passaram 32 verificacoes de memoria/projecao/configuracao/pool/nomes/ossos, as duas passagens de verificacao grafica DX9, o teste de transporte de hook no auxiliar e as 14 verificacoes do overlay separado. MSVC/CMake nao foi executado neste ambiente.

## Alteracao

`src/runtime.cpp` publica um snapshot imutavel do frame quando `[capture] separate_overlay=1`; nao chama o renderer no dispositivo do jogo. O worker apresenta o snapshot em `src/separate_overlay.cpp`, com janela layered, color key preto, click-through, sem ativacao e dispositivo Direct3D 9Ex independente. A vida util de cada frame e limitada por `frame_timeout_ms`. Somente a area cliente da janela do jogo recebe o overlay.

O modo separado oculta o desenho fora de janela/sem bordas, durante Reset, sem frames recentes, ao perder foco e ao desativar ou encerrar. Uma falha grafica desativa o desenho e registra a operacao e o HRESULT; nao seleciona o caminho interno como recuperacao.

## Escopo da evidencia

O teste `tests/overlay_checks.cpp` cria uma janela auxiliar, preenche seu backbuffer com uma cor conhecida, apresenta o overlay num dispositivo diferente e compara todos os bytes do backbuffer auxiliar. Tambem verifica estilos de janela, hit testing, foco, alinhamento, resize, minimizar/restaurar e destruicao ordenada. O teste nao injeta no GTA/MTA.

O renderer compartilhado continua sujeito a `tests/render_checks.cpp`, que verifica pixels de caixa, vida, esqueleto, nome e indicador, restauracao de viewport/estados e Reset.

Resultado da captura interna do MTA instalado: **pendente de validacao em sessao**. A separacao foi projetada para capturas do backbuffer; nao ha garantia de exclusao em captura do desktop ou em ferramentas de terceiros.

A transparencia usa color key preto; sombras e contornos totalmente pretos tambem ficam transparentes. Conteudo colorido, nomes e indicador continuam sendo desenhados. O intervalo do worker continua configurado por `[runtime] poll_ms`; o perfil fornecido usa 25 ms.

A DLL existente em `bin` estava bloqueada para escrita durante a entrega. A atualizacao local foi preparada em `bin/pendente`, para aplicacao pelo BAT existente apos liberar o arquivo. O pacote ZIP foi preparado com a DLL e o INI novos diretamente em `bin`. O ZIP anterior e os binarios anteriores foram preservados em `build/before-capture`.

## Fontes

- [MTA: captura interna via GetBackBufferPixels](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/core/CScreenShot.cpp).
- [MTA: inicializacao do proxy Direct3DCreate9](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/core/DXHook/CDirect3DHook9.cpp).
- [Microsoft: Direct3DCreate9Ex](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-direct3dcreate9ex).
- [Microsoft: transparencia por SetLayeredWindowAttributes](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setlayeredwindowattributes).
