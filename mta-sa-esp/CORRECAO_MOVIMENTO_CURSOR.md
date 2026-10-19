# Movimento e cursor — motion-cursor-v1

O usuario confirmou que o overlay voltou a aparecer e que o deslocamento ocorre durante o movimento, nao com tudo parado. A entrega anterior mantinha camera e poses no mesmo snapshot de coleta de 25 ms, apresentado por um worker que tambem esperava 25 ms. Essa combinacao podia produzir atraso visual adicional, especialmente ao mover a camera.

## Sincronizacao

- A camera e amostrada em cada EndScene selecionado, mesmo quando a coleta de jogadores ainda nao venceu.
- Cada publicacao compartilha dados imutaveis dos jogadores com a anterior, em vez de copiar nomes, ossos e vetores para atualizar apenas a camera.
- Publicacao e coleta possuem tempos separados. Atualizar a camera nao adia a coleta seguinte, e a validade do frame usa o momento real da publicacao.
- Caixa e esqueleto usam a mesma camera atualizada durante o desenho.
- O perfil usa collect_interval_ms=16 e poll_ms=5. O worker pula uma apresentacao duplicada quando ainda nao existe outro frame e o overlay ja esta visivel.
- A coleta continua sem previsao de movimento ou posicoes inventadas. Dados de personagens continuam sendo amostrados; nao se promete latencia zero. A taxa maior de coleta pode aumentar o custo de CPU; FPS no MTA nao foi medido nesta rodada.

## Cursor do painel

O cursor padrao e desenhado depois dos controles, na propria camada do painel, com a posicao atual do mouse. Isso permite ve-lo mesmo quando o jogo desenha seu cursor no buffer que fica abaixo da janela. Ao sair ou fechar o painel, essa imagem desaparece. Nao ha mudanca no contador global ShowCursor nem captura permanente do mouse.

O painel guarda seu bitmap e reconstrui o conteudo quando controles, estado, tamanho ou regiao de hover mudam. Movimento do cursor sozinho invalida suas duas pequenas areas. A janela continua sem ativacao e preserva a correcao de propriedade/ordem da entrega anterior.

## Validacao

Passaram 66 verificacoes de dados/configuracao/inicializacao e oito do callback real de EndScene com memoria/COM isolados. Esses oito testes confirmam que a camera muda entre coletas, o snapshot anterior nao e alterado, os dados sao compartilhados, configuracoes invalidam a coleta e uma camera invalida retira o frame.

Passaram tres testes de pixels/estados DX9. Um deles zera deliberadamente a camera armazenada com as poses e confirma que caixa e ossos ainda sao desenhados com a camera nova fornecida ao renderer.

Passaram o modo de cursor do painel, com verificacao de pixels antes/depois e remocao ao sair, e as 34 verificacoes da suite completa do painel, incluindo foco, controles, cores e perfil. Passaram tambem 17 verificacoes de propriedade/ordem do overlay, transporte auxiliar e nove verificacoes do atualizador. O teste de ordem usa o modo que nao avalia foco; a validacao de foco citada nesta rodada e a do painel.

Nao foi iniciado nem injetado o MTA durante os testes. O alinhamento durante o movimento e o custo no jogo precisam de confirmacao na sessao do usuario apos aplicar a nova DLL.

## Instalacao

A DLL antiga estava em uso; o atualizador retornou 6 e preservou a instalacao. A nova DLL e o perfil com os intervalos ajustados ficam em bin/pendente. Feche totalmente o GTA/MTA e execute injetar.bat para aplicar o par. O ZIP inclui a nova compilacao diretamente em bin.

SHA256 da DLL motion-cursor-v1: A48DE4A1A1A2EC5D9E911FF3048BDD8BED871A637A3028BEA1BA323526EC1E15.
