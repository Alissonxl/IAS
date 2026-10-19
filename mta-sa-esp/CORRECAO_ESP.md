# Recuperacao do ESP apos a revisao de desempenho

**HISTORICO - SUBSTITUIDO.** Esta tentativa passou nos testes auxiliares, mas o usuario informou que a marcacao continuava ausente. A entrega atual restaura a base anterior a alteracao de distancia, conforme `RESTAURACAO_ESTAVEL.md`. As alteracoes abaixo nao estao ativas no codigo restaurado.

O usuario informou que INSERT ainda abria o painel, mas nenhum jogador era exibido. O log instalado confirmava carregamento da DLL, hashes dos modulos e atualizacoes da lista de jogadores, sem erro DX9 registrado. O log anterior nao informava quantos alvos chegavam ao renderer, portanto nao permite atribuir sozinho uma causa unica.

## Falha reproduzida

Uma janela alvo com WS_EX_TOPMOST podia passar a frente do overlay. A rotina so restaurava a ordem quando a geometria mudava. Assim, o overlay continuava coletando/desenhando e aparecia como visivel ao Windows, mas podia ficar encoberto pelo jogo.

O teste novo promove a janela auxiliar para topmost sem mover/redimensionar. Com o objeto compilado anterior, a verificacao de restauracao da ordem falhou. O codigo corrigido consulta a ordem e reposiciona o overlay apenas quando necessario, com SWP_NOACTIVATE. O painel tambem verifica a ordem completa, em vez de depender de o overlay ser a janela imediatamente anterior.

## Modo de recuperacao

- screen_culling=0 no perfil instalado e no perfil distribuido. A coleta nao depende do descarte antecipado de projecao.
- O runtime volta a apresentar continuamente o overlay. A variante que reutiliza revisoes continua disponivel para testes explicitos da classe, sem ser aplicada automaticamente ao jogo.
- A validade temporal do frame passa a contar a partir da conclusao da coleta, evitando descarta-lo antes de apresentar caso a coleta demore.
- Permanecem a distancia estendida com cache, os filtros de dimensao, o painel, as cores, vida/colete, a leitura agrupada de metadados, a suspensao em segundo plano, o log no worker e o buffer reutilizavel do painel.

## Diagnostico

Linhas de coleta agora incluem:

- alvos: entidades que chegaram ao frame;
- pool: slots GTA ocupados;
- candidatos: registros analisados depois dos vinculos basicos;
- distancia, dimensao, fora_tela, vida: descartes por cada filtro;
- leituras: falhas de leitura detectadas;
- culling: chave efetivamente aplicada;
- coleta_ms: duracao da coleta.

Se houver entidades coletadas, mas nenhuma desenhavel, o worker registra candidatos/projetados do renderer. O registro e limitado por status_log_ms e fica fora do callback grafico normal. Nomes de jogadores, enderecos de memoria e coordenadas nao entram nesses contadores.

## Entrega e validacao

Build LLVM-MinGW x86/C++20 com -Wall -Wextra -Wpedantic -Werror concluido. Passaram 144 verificacoes: 72 de coleta/configuracao/memoria, 38 do painel, 19 do overlay, quatro passagens DX9, um benchmark funcional, um transporte auxiliar e nove do atualizador. O teste de ordem falhou com o objeto anterior e passou com a correcao; a preservacao de foco tambem passou. MSVC nao foi executado.

A alteracao de configuracao screen_culling=0 ja foi aplicada atomicamente ao INI instalado, preservando as outras escolhas do usuario. O perfil anterior foi salvo em build/before-esp-recovery. F5 recarrega configuracao; a correcao da DLL exige fechar o GTA/MTA e aplicar bin/pendente por injetar.bat.

O atualizador pode aplicar esse par com o MTA fechado, sem abrir ou injetar no jogo. Ele preserva backups em bin/atualizacao-<id>. Se os arquivos estiverem ocupados, a atualizacao permanece pendente para o proximo uso do BAT.

A cena real relatada ainda precisa ser conferida no MTA. O processo estava fechado durante o diagnostico. As verificacoes auxiliares nao comprovam por si so o resultado na sessao real.

## Referencias de API

- [Microsoft: SetWindowPos e SWP_NOACTIVATE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos).
- [Microsoft: consulta da ordem por GetWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindow).

