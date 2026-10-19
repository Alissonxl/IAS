# Apresentacao do overlay — overlay-display-v1

A ultima execucao carregou a DLL startup-order-v1 (BB5D35C1E44F8529D0E37865AE51B92B2146A73501BDEFFA5479B99FCF5C3125). O log registrou callbacks selecionados, coletas e 15–16 entidades com o ESP ativado. O usuario confirmou que as marcacoes tambem faltam ao vivo, nao apenas na captura. Portanto ha evidencia de coleta nessa sessao; a ausencia visual exige verificar a apresentacao e projecao.

## Mudancas

- Corrigida a ordem das janelas: antes, o overlay so era colocado no topo quando sua geometria mudava. Agora sua ordem em relacao ao jogo e conferida e restaurada quando necessario, mesmo com tamanho/posicao constantes.
- O painel tambem preserva sua ordem acima do overlay e do jogo.
- O dispositivo proprio do overlay usa D3DCREATE_NOWINDOWCHANGES. Segundo a [documentacao da Microsoft](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dcreate), esse sinalizador impede que o Direct3D altere a janela de foco. O reposicionamento continua usando SWP_NOACTIVATE.
- Diagnosticos de apresentacao a cada cinco segundos: modo interno/desativado/reset/sem frame/frame expirado/tela cheia exclusiva/sem foco/apresentar; janela alvo, primeiro plano, idade do frame, visibilidade e ordem do overlay.
- Diagnosticos do desenho: quantidade recebida, entidades enquadradas, descartes por projecao, tamanho ou fora da tela e alvo grafico ignorado.

A espera de inicializacao da entrega anterior foi mantida. Enumeracao, vinculo MTA, dimensoes, distancia, ossos e calculo da projecao nao foram alterados. Nao ha hook Present nem reinstalacao periodica de callbacks.

## Validacao e limites

Passaram as 64 verificacoes de dados/configuracao/inicializacao, o transporte no processo auxiliar, as nove verificacoes do atualizador e as tres passagens de pixels/estados DX9. Os contadores novos do renderer concordaram com o jogador sintetico visivel.

Na suite do overlay, passaram 15 de 18 verificacoes, incluindo a reproducao do jogo cobrindo o overlay e a recuperacao da ordem sem mudar a geometria. Falharam tres verificacoes relativas ao foco: a janela auxiliar nao conseguiu assumir o primeiro plano para estabelecer o ambiente de teste; durante a apresentacao o primeiro plano ficou nulo. O overlay nao apareceu como janela em primeiro plano no diagnostico, mas isso nao comprova preservacao de foco. Essas falhas foram mantidas no teste e nao foram contabilizadas como sucesso. O comportamento de foco precisa de validacao num ambiente controlado.

Nenhuma nova sessao do MTA foi iniciada ou injetada durante os testes. A correcao de ordem foi verificada no auxiliar; ainda nao e uma confirmacao de ESP visivel na sessao do usuario. A verificacao de integridade do MTA continua sendo uma limitacao observada anteriormente.

Depois de fechar completamente a sessao antiga, o injetar.bat da raiz aplica o par DLL/INI pendente, se necessario. O log dessa compilacao deve iniciar com overlay-display-v1. Se os jogadores continuarem ausentes, as linhas Apresentacao permitem verificar o ponto da falha sem alterar novamente a coleta.

Nesta entrega, o atualizador encontrou a DLL em uso e retornou codigo 6. A versao startup-order-v1 continua instalada ate liberar o arquivo; a nova compilacao e o perfil validado estao em bin/pendente. SHA256 da nova DLL: 395B76338F9ACD7A374431A2DDAF49161B10415B221F3D0E5CA5AA3AC374E1D4. O ZIP inclui a nova DLL diretamente em bin.
