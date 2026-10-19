# Revisao e otimizacao

Revisao de 2026-10-18. Os pontos abaixo foram corrigidos no codigo e incluidos no pacote atualizado.

## Falhas corrigidas

| Prioridade | Problema encontrado | Correcao |
| --- | --- | --- |
| P1 | SHA256 dos modulos podia ler arquivos no callback EndScene, causando pausas na renderizacao. | Verificacao no worker; EndScene consulta uma identidade imutavel publicada sob lock curto. Falhas de leitura permitem nova tentativa. |
| P1 | Sem metadados MTA, desativar Todas dimensoes ainda permitia o fallback para o pool GTA sem conferir dimensao. | Suspende o desenho enquanto a dimensao local nao puder ser confirmada. |
| P2 | Um frame coletado antes de mudar o painel podia ser desenhado com as opcoes novas, incluindo entidades que o filtro novo deveria excluir. | Cada snapshot conserva sua configuracao. A apresentacao rejeita snapshots de outra configuracao e a proxima coleta e imediata. |
| P2 | Uma edicao comum do painel podia reativar o renderer apos uma falha. | Somente o checkbox Ativado envia uma alteracao explicita de ativacao; as outras edicoes preservam o estado de execucao. |
| P2 | Uma falha na lista MTA limpava nomes, mas deixava dimensoes/posicoes em cache no resultado indisponivel. | Limpa todas as colecoes e reconfirma tambem o ponteiro do gerenciador no fim da leitura. |
| P2 | O atualizador podia trocar a DLL e depois falhar ao escrever o INI. | Confere ambos os destinos antes da troca, prepara e confere copias, preserva backups e tenta restaurar as trocas ja feitas em uma falha detectada. |
| P3 | O teste de teardown do painel procurava globalmente uma classe de janela e podia falhar por encontrar outra instancia. | Verifica exclusivamente o HWND criado pelo teste. |

## Reducao de trabalho

- A coleta completa tem intervalo proprio, independente do numero de callbacks graficos: `collect_interval_ms=25`. Alteracoes de configuracao invalidam a amostra e forcam uma coleta nova.
- Uma simulacao de 200 callbacks em um segundo produziu 40 coletas, contra uma coleta por callback no caminho anterior. Isso mede quantidade de trabalho agendado; nao e uma medicao de FPS dentro do MTA.
- Camera e endereco executavel da linha de visao sao preparados uma vez por coleta, compartilhados pelas entidades.
- O filtro de distancia antecede leituras de vida/colete, ossos e raios. Campos adjacentes de vida, maximo e colete usam uma leitura conjunta. Uma falha apenas no colete permite recuperar a vida separadamente.
- Colete nao e lido quando barra e valores numericos estao desligados. O vetor de entidades reserva capacidade conforme slots ocupados/jogadores elegiveis.
- Ossos usam buffers locais limitados a 256 nos, sem duas alocacoes dinamicas por personagem. A hierarquia e reconfirmada depois da leitura.
- O dispositivo exclusivo do overlay desenha sem criar/salvar/restaurar state blocks. O caminho que usa o dispositivo do jogo conserva a restauracao de estados.
- O renderer desativado retorna antes de criar recursos. Rotulos ASCII desenham glifos diretamente, evitando strings largas temporarias para cada texto/sombra.
- Os testes graficos sao marcados como seriais no CTest para evitar interferencia de foco entre janelas auxiliares.

## Configuracao

```ini
[runtime]
collect_interval_ms=25
module_check_ms=1000
```

`collect_interval_ms`: 5 a 1000 ms, limitado tambem por `frame_timeout_ms`. `module_check_ms`: 250 a 10000 ms, controla a consulta de identidade pelo worker; hashes ja confirmados nao sao recalculados sem mudanca de identidade/perfil. Camera e funcao de visibilidade devem estar ambas configuradas, ou ambas desativadas.

## Verificacao

Build LLVM-MinGW x86/C++20 com `-Wall -Wextra -Wpedantic -Werror` concluido. Passaram 52 verificacoes de memoria/configuracao/projecao/entidades/agendamento, 15 verificacoes do painel, 14 do overlay separado, tres passagens graficas, o transporte de hook no auxiliar e nove verificacoes do atualizador. As passagens graficas verificaram pixels das duas paletas, caixas padrao/cantos, barras de vida/colete, nomes, snaplines, estados do dispositivo emprestado, caminho exclusivo sem state blocks e ausencia de chamadas graficas quando desativado.

Os testes do atualizador usaram arquivos sinteticos em `build`: INI bloqueado, par valido, preservacao de backups e perfil pendente ausente. A tentativa de rollback apos falha entre as duas substituicoes nao foi exercitada por injecao de falha. O par DLL/INI nao constitui uma transacao unica de sistema de arquivos; backups permanecem disponiveis.

MSVC e a versao nova em uma sessao real do MTA continuam sem validacao. Nao foi medido ganho de FPS/latencia no jogo. Os offsets, a compatibilidade da chamada nativa de visibilidade e a captura interna exigem verificacao na sessao correspondente ao perfil. O modo Todas dimensoes continua limitado aos dados que o cliente possui, com registros fora do streaming marcados como cache.

## Entrega

Fontes e binarios novos no ZIP. Na pasta de trabalho, DLL e INI novos ficam em `bin/pendente`; feche o GTA/MTA e execute `injetar.bat` para aplicar. A versao anterior do pacote e os binarios anteriores ficam em `build/before-optimization`. O atualizador preserva o par substituido em uma pasta `bin/atualizacao-<id>`.

## Referencias de API

- [Microsoft: CreateStateBlock ja captura os estados na criacao](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-createstateblock).
- [Microsoft: File.Replace e backup do arquivo substituido](https://learn.microsoft.com/en-us/dotnet/api/system.io.file.replace?view=netframework-4.8.1).
