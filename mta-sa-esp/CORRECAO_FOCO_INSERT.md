# Correcao de foco ao abrir com INSERT

Revisao panel-focus-v2. A revisao panel-input-v1 desativava a janela do jogo com EnableWindow e ativava o painel com SetForegroundWindow/SetFocus. O relato real de minimizacao invalida a estrategia anterior, mesmo tendo passado em janelas auxiliares.

Removido o controlador modal PanelInput. O painel sempre usa WS_EX_NOACTIVATE e responde WM_MOUSEACTIVATE com MA_NOACTIVATE. Abrir, clicar e fechar nao alteram habilitacao da janela, foco, contador ShowCursor ou confinamento ClipCursor. A falha ao tentar tornar o cursor visivel nao impede mais a abertura. O painel fica oculto fora do jogo e pode reaparecer no retorno.

Mantidas as correcoes anteriores de projecao, coleta e desenho. Os testes de entrada verificam o comportamento de janela e cliques sinteticos; nao comprovam o comportamento do DirectInput/cursor no MTA. A captura exclusiva do mouse e a suspensao da camera pelo proprio MTA continuam sem validacao nesta revisao. A interface ForceCursorVisible encontrada no SDK nao possui vinculo binario verificado neste projeto e nao foi chamada por indice ou endereco presumido.

Para usar a nova DLL, feche GTA/MTA completamente e execute injetar.bat da raiz. Uma DLL ja carregada nao e substituida no processo em andamento.

Validacao: compilacao x86 com avisos tratados como erros; 16 verificacoes de foco, 34 controles do painel, 8 de sincronizacao, 66 gerais, hook auxiliar e 9 de atualizacao passaram. Nenhum teste desta revisao controlou o MTA real.

SHA256 da DLL corrigida: DCB677C9E2A1337130CB276A0067FC9F68AF8C39FE6CB0D65793C4EEA14F65FF.
A DLL instalada estava em uso durante a entrega; o par DLL/INI foi preparado em bin/pendente. O carregador aplica a atualizacao apos o jogo fechar.
