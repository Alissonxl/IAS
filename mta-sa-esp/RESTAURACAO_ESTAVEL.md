# Restauracao solicitada em 2026-10-18

Registro da restauracao anterior. A versao atual acrescenta somente a espera de inicializacao e diagnosticos descritos em CORRECAO_INICIALIZACAO.md; o hash abaixo identifica a DLL original preservada.

Foi restaurado o pacote original do painel aprovado, anterior as alteracoes de distancia/desempenho e a tentativa graphics-binding-v1.

- DLL instalada: bin/mta-sa-esp.dll.
- SHA256: 94EA82EC9B599BDA0BA23683F7D9839E722956E18A19249E318CFE04F822788E.
- Os 26 fontes coincidem com build/before-distance-perf/src.
- Carregador, testes, scripts, fontes e configuracoes foram repostos do ZIP original preservado em build/panel-v2-release/mta-sa-esp.zip.
- Os BATs voltaram a selecionar bin, sem redirecionamento para graphics-binding-release.
- Restaurados os valores originais do perfil, incluindo max_distance=250 e enabled=1. O perfil mais recente foi preservado no backup abaixo.
- Removidos do projeto ativo os arquivos exclusivos da tentativa mais recente, depois de copia-los para o backup.
- O atualizador aplicou o par DLL/INI com sucesso e preservou o par substituido.
- O ZIP em IAS/mta-sa-esp.zip voltou a ser o pacote original, byte a byte.

Backup do estado substituido: C:\Users\Administrator\Desktop\IAS\mta-sa-esp\build\before-user-rollback-20261018-153427
Copia extraida da entrega original: C:\Users\Administrator\Desktop\IAS\mta-sa-esp\build\rollback-panel-v2-20261018-153427

Verificacao: todos os arquivos do pacote original, exceto seu manifesto que e conferido separadamente, coincidem com os arquivos instalados. As verificacoes de dados/configuracao passaram e o BAT foi verificado sem abrir ou injetar no jogo. Nenhuma nova sessao do MTA foi iniciada; restaurar esse backup nao comprova funcionamento ou compatibilidade atuais com a verificacao de integridade.
