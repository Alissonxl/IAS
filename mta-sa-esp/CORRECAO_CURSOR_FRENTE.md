# Cursor na frente do painel - panel-cursor-v3

O cursor do painel agora e desenhado na composicao final da interface. A imagem dos controles fica em cache sem cursor; cada apresentacao combina essa imagem com uma seta branca de contorno escuro em outro bitmap e apresenta o resultado de uma vez. O cursor nao depende da visibilidade do cursor nativo controlada pelo jogo.

Mover o cursor invalida somente suas regioes antiga e nova, sem redesenhar os controles. A posicao usa coordenadas reais do cliente, sem aplicar a escala do layout duas vezes. A consulta de posicao tambem ocorre no pump para atualizar o cursor quando o jogo nao entrega WM_MOUSEMOVE ao painel. A seta nativa e ocultada apenas no WM_SETCURSOR da area cliente; nao ha ShowCursor, ClipCursor, transferencia de foco ou desativacao da janela do jogo.

Testes de pixels verificam cursor na frente, estabilidade em repinturas consecutivas e restauracao da regiao anterior sem rastros. Testes auxiliares nao comprovam a captura de mouse/controle da camera dentro do MTA. Essa limitacao permanece.

Feche GTA/MTA por completo e execute injetar.bat para carregar esta DLL; uma DLL carregada nao e atualizada no processo em andamento.

Validacao desta entrega: compilacao estrita x86, verificacoes gerais 66, sincronizacao 8, hook auxiliar, atualizacao 9, pixels do cursor 9 e preservacao de foco em janela auxiliar 16 passaram. A suite geral do painel passou em 31/34; tres verificacoes ligadas a foco falharam nesta sessao e permanecem inconclusivas para o MTA. Nao foi feito teste ao vivo no jogo.

DLL SHA256: BF2449200293F7D803422604EC28C674789CEE1BD30A2A0AF61320A5425DCBDE. Retorno da tentativa de atualizacao: 0 (6 significa arquivo instalado em uso; nova versao fica em bin/pendente).
