# FOV alinhado com a mira do HUD - crosshair-fov-v7

O problema vinha de usar metade da largura e da altura para desenhar o FOV e calcular a correcao de mouse. A mira em terceira pessoa usa multiplicadores do HUD. Agora a posicao normalizada e lida a cada frame, publicada junto da camera e usada pelo circulo, pela linha de destaque, pelo filtro de FOV e pela correcao de movimento.

A leitura distingue os modos de camera 53/55/65, que usam os multiplicadores, dos modos com mira centralizada, como sniper. O indice da camera ativa e limitado a tres cameras. Coordenadas invalidas e assinatura de HUD divergente suspendem o FOV e a entrada de mira, sem substituir o dado por um deslocamento arbitrario. A verificacao de hash do GTA existente continua obrigatoria.

Os RVAs novos ficam na secao aim do INI. O leitor valida os operandos usados por DrawCrossHairs antes de interpretar os multiplicadores. O trecho local disassemblado usa X em 0xB6EC14, Y em 0xB6EC10, camera ativa em 0xB6F081 e modos em 0xB6F1A8 com passo 0x238. A leitura e somente de dados/codigo; nenhum novo hook ou escrita na camera foi adicionado.

Referencia primaria da formula do HUD: [DrawCrossHairs](https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Hud.cpp). O codigo original multiplica a largura e altura pelos multiplicadores ao posicionar a mira circular. A verificacao no executavel local e a assinatura em memoria evitam depender apenas da documentacao da estrutura.

Passaram 46 testes de selecao e referencia do HUD com memoria sintetica, incluindo mira fora do centro, correcao zero no alvo alinhado, troca de resolucao, sniper, modos de ombro/veiculo e dados invalidos. Os seis testes DX9 de pixels/estado incluem circulo e marcador deslocados e ausencia do circulo antigo no centro. Os testes da aba Mira e as verificacoes normais do build tambem passaram. Nao houve validacao visual dentro do MTA nesta entrega.

Feche GTA/MTA completamente e execute injetar.bat para carregar a DLL nova. O log identifica crosshair-fov-v7 e registra centro_mira_valido, centro_mira_x e centro_mira_y para conferir a referencia efetivamente lida no jogo.

DLL SHA256: F0214922E130CF1EC3FBEE124EC66823ED74A1AF4FCC860C9DF94E5FEA9ED8BC. Retorno da atualizacao: 6 (6 significa DLL instalada em uso e correcao em bin/pendente).
