> Historico: esta estrategia foi retirada em panel-focus-v2 apos minimizar o MTA na sessao real. Veja CORRECAO_FOCO_INSERT.md.

# Entrada do painel — panel-input-v1

O painel anterior permanecia sem ativacao e desenhava um cursor adicional dentro de seus pixels. Com o jogo recebendo a entrada e podendo reposicionar/esconder o cursor, a camera continuava respondendo ao mouse e a imagem extra podia alternar com o cursor nativo. Esta entrega substitui esse comportamento por uma sessao de entrada do painel.

## Interacao

Quando o alvo e uma janela visivel do proprio processo MTA, o painel e um popup pertencente a ela e recebe foco de teclado/mouse. A entrada Win32 da janela do jogo e temporariamente desabilitada, e o limite do cursor e liberado se estava restrito. O jogo permanece como proprietario da janela; nao se abre outro aplicativo.

O painel usa somente o cursor nativo. Foi removido o desenho de DrawIconEx e a alternancia de invalidacao por posicao do cursor. A visibilidade e adquirida por um numero limitado de chamadas ShowCursor; todas sao balanceadas no encerramento da sessao, inclusive em erro. O bitmap em cache da interface permanece.

Ao fechar por INSERT, Escape ou X, o painel encerra arrastes, habilita novamente a janela do jogo e devolve o foco. O limite anterior do cursor so e restaurado quando foi alterado e o foco voltou ao jogo. Ao trocar para outra janela, o painel fecha e restaura os controles sem ativar o jogo por cima do aplicativo escolhido.

Nao ha bloqueio global de entrada, hook de teclado/mouse ou chamada nova a funcoes internas do MTA. EnableWindow desabilita a entrada da janela, conforme a [documentacao da Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enablewindow); a camera real depende tambem de como o jogo processa seu dispositivo de entrada e precisa de confirmacao ao vivo.

## Outras correcoes de usabilidade

- Escape fecha primeiro o seletor de cor ou dropdown; so depois fecha o painel.
- Repeticoes automaticas da tecla Escape nao fecham varias camadas numa unica pressionada.
- Escape e WM_CANCELMODE liberam um arraste interrompido, sem deixar captura presa.
- Ctrl+S solicita salvar o perfil; repeticao de tecla nao dispara gravacoes continuas.
- Uma janela ja desabilitada por outro dialogo nao e assumida nem habilitada indevidamente.
- A aquisicao de entrada valida o processo, a propriedade da janela e o foco. Se falhar, desfaz as alteracoes e retorna uma mensagem especifica.
- Janelas auxiliares sem alvo valido do proprio processo conservam o modo sem ativacao; isso permite testar os controles isoladamente sem desabilitar aplicativos externos.

## Validacao

Passaram 16 verificacoes de entrada com janelas auxiliares reais: foco no painel, janela proprietaria desabilitada, cursor nativo disponivel, Escape/dropdown, repeticao, captura/cancelamento de arraste, reabertura, restauracao do foco/cursor e troca de janela sem roubar foco. Tambem passaram as 34 verificacoes do painel e o modo de pixels que confirma ausencia de cursor duplicado/trilhas.

As 66 verificacoes de dados/inicializacao, oito de sincronizacao de camera, transporte auxiliar e nove do atualizador passaram na compilacao. Coleta, ossos, projecao e apresentacao de frames nao foram alterados nesta entrega. Nenhum MTA foi iniciado/injetado nos testes; o comportamento da camera no jogo continua pendente de validacao na proxima sessao.

## Aplicacao

O Windows manteve a DLL anterior em uso e o atualizador retornou 6. A nova DLL e o perfil validado estao em bin/pendente. Feche completamente o GTA/MTA e execute injetar.bat para aplicar a atualizacao. O ZIP inclui a nova DLL diretamente em bin.

SHA256 da DLL panel-input-v1: 29449A02C16C8F147A7BFE270E627DE6CCA6ECAA9AC73B9D5632BC89E9C870F4.

