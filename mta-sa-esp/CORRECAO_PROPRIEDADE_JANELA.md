# Janela do overlay pertencente ao jogo — overlay-owner-v1

## Evidencia da execucao real

O log da compilacao overlay-display-v1, com o jogo em foco, registrou simultaneamente `estado=apresentar`, `ativado=1`, `overlay_visivel=1`, um jogador enquadrado e `acima_jogo=0`. Isso demonstra que a janela do overlay estava abaixo do jogo naquela amostra, apesar de haver dados prontos para desenho. Em outros momentos, o usuario estava fora do jogo e os frames expiravam; esse segundo comportamento nao explica a amostra em foco.

A correcao anterior reposicionava o overlay em cada apresentacao quando necessario. Isso ainda permitia que o jogo voltasse a passar a frente entre duas apresentacoes.

## Alteracao

O popup do overlay agora e criado com a janela raiz do jogo como proprietaria. Conforme a [documentacao do Windows](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#owned-windows), uma janela pertencente a outra permanece acima de sua proprietaria. O recurso usa o parametro hwndParent de CreateWindowExW para um WS_POPUP; nao transforma o overlay em janela filha nem muda as coordenadas para o espaco do cliente.

O proprietario precisa pertencer ao processo atual. Caso o alvo mude, o overlay antigo e liberado e recriado com o proprietario correto. O estilo sem ativacao, passagem de cliques, dispositivo DX9 proprio e D3DCREATE_NOWINDOWCHANGES continuam presentes. Nenhum hook, offset, filtro, enumeracao ou calculo da projecao foi alterado nesta entrega.

O log informa `overlay-owner-v1` e inclui o handle da janela proprietaria na linha Apresentacao. A hipotese de janelas concorrendo na ordem agora tem uma correcao baseada no relacionamento entre janelas, alem da verificacao de ordem ja existente.

## Teste de regressao

O teste atualizado foi executado contra o objeto do apresentador anterior, sem recompila-lo com a correcao. Falharam tres verificacoes: vinculo com a proprietaria, permanencia acima apos promover o jogo e permanencia acima apos 16 promocoes sem outra apresentacao do worker.

Depois da correcao, passaram as 17 verificacoes da suite no modo --ownership-only, incluindo esses tres casos, geometria, minimizar/restaurar, passagem de cliques e preservacao do buffer do jogo. Tambem passaram as tres passagens de pixels/estados DX9, 64 verificacoes de dados/inicializacao, transporte auxiliar e nove verificacoes do atualizador.

O modo --ownership-only permite verificar propriedade e ordem de forma independente do controle do primeiro plano pelo ambiente. Ele informa explicitamente que nao testa preservacao do foco. O modo completo continua disponivel, com suas verificacoes estritas de foco; a limitacao da rodada anterior nao foi convertida em sucesso.

Nenhum jogo foi iniciado ou injetado durante os testes. O teste reproduz a falha de ordem observada no log, mas a visibilidade na sessao real precisa de confirmacao apos aplicar a compilacao nova.

## Instalacao desta entrega

A DLL ativa estava em uso e o atualizador retornou codigo 6. A nova DLL e o perfil validado estao em bin/pendente; a instalacao antiga foi preservada. Feche completamente o GTA/MTA e execute injetar.bat para aplicar o par pendente. O ZIP leva a DLL nova diretamente em bin.

SHA256 da nova DLL: 28DED1F3F7FFA57568706BCFC22B944AE6992C317534B437CF46CA390084159F.
