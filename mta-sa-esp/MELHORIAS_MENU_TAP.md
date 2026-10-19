# Menu e Tap manual — tap-menu-v8

## Usabilidade

- INSERT abre e fecha. Visual e Mira continuam com interruptores independentes; o painel aberto pausa a Mira.
- Roda sobre os ajustes altera valores em passos pequenos, sem precisar arrastar. Shift multiplica o passo por dez.
- Distancia: roda = 10 m; Ctrl+roda = 1 m; Shift+roda = 100 m.
- Mira: FOV = 1 px; suavizacao = 0,1; previsao = 1 ms; ganho = 0,01 por passo.
- Botao direito sobre um ajuste restaura seu padrao: distancia 500 m, FOV 70 px, suavizacao 2, previsao 0 ms, ganho 1.
- O rodape explica o controle sob o mouse. A captura de atalhos informa conflitos; ESC cancela e Delete remove a troca de alvo opcional.
- A indicacao NAO SALVO permanece enquanto houver alteracoes, mesmo quando o diagnostico do jogo atualiza. Falha ao salvar mantem o aviso. Salvar perfil e Ctrl+S gravam as duas abas.
- A posicao arrastada e lembrada ao reabrir durante a sessao e limitada a area util do monitor. Nao e persistida entre processos.
- Os menus de selecao destacam cada opcao ao passar o mouse, inclusive o terceiro modo de ativacao.

## Tap manual

O modo Tap manual acelera a aproximacao inicial e reduz a resposta perto do osso. Dentro da tolerancia para de corrigir; uma faixa maior de retomada evita oscilacao por pequenos deslocamentos. Quando estabiliza, o destaque do alvo fica verde. A cor indica alinhamento projetado, nao confirma acerto do tiro.

Segure a tecla configurada para alinhar e dispare manualmente. O controlador so envia movimento relativo do mouse. Nao envia cliques, Q ou troca de arma. Ao guardar a arma manualmente, suspende a correcao; ao voltar a mirar pode retomar com a tecla mantida. Abrir painel ou perder foco continua exigindo soltar e pressionar a tecla novamente.

Ganho do mouse na aba Mira permite calibrar a correspondencia entre movimento e pixels: reduza se ultrapassar o alvo, aumente se o movimento ficar curto. A configuracao preserva a tecla escolhida pelo usuario (perfil atual: botao direito).

No INI, [aim] usa mode=2 para Tap manual. tap_boost=2 controla a aproximacao (1 a 4), tap_tolerance_px=0.75 a parada (0.1 a 4 px) e tap_resume_px=1.5 a retomada (da tolerancia ate 8 px). max_step continua limitando o deslocamento por atualizacao.

## Validacao

Build x86 com C++20, -Wall -Wextra -Wpedantic -Werror. Passaram 53 verificacoes de mira, 34 interacoes da aba Mira, 34 da aba Visual, 19 de entrada/foco, 9 de composicao do cursor e 7 de renderizacao DX9. O build tambem executou os testes existentes de coleta, configuracao, atualizacao e ciclo do cursor.

Os testes usam memoria sintetica e janelas auxiliares. Nao injetaram a nova DLL no MTA, nao enviaram movimentos reais de mira e nao mediram FPS ou taxa de acerto em partida. A calibracao de ganho e o alinhamento ainda precisam ser conferidos no jogo.

## Atualizacao

Se a DLL estiver carregada, a dupla DLL/INI fica em bin/pendente. Feche o jogo completamente e execute injetar.bat na pasta do projeto; o atualizador aplica a dupla antes de carregar. Preserva backups e as preferencias existentes.
