# Validacao da rota de hook de mensagens

## Teste isolado

A compilacao x86 passou com `-Wall -Wextra -Wpedantic -Werror`. Quinze verificacoes de memoria, projecao, configuracao e pool passaram. `mta-sa-hook-checks.exe` criou uma janela auxiliar oculta, instalou um hook apenas em sua thread, recebeu confirmacao da DLL de prova por evento e encerrou o auxiliar.

## Teste no MTA/GTA

Em 2026-10-18, foi executado o carregador recompilado com `--hook` e a DLL de `bin`. Ele abriu o inicializador instalado do MTA e encontrou o GTA no PID 16732, thread de janela 16896.

Saida do carregador:

```text
GTA iniciado, PID 16732. Aguardando 3000 ms de inicializacao...
Hook de mensagens instalado apenas na thread 16896 do PID 16732.
Entrada da DLL pelo hook confirmada no processo. Consulte mta-sa-esp.log para o estado do ESP.
```

Linhas registradas pelo worker da DLL:

```text
Hash confirmado. Aguardando dispositivo DX9.
Callbacks DX9 instalados. INSERT abre/fecha painel; Ativado controla ESP; F5 recarrega; END encerra.
Aguardando personagem local
Dispositivo DX9 restaurado
```

Esses resultados confirmam entrada da DLL, inicio do worker e callbacks DX9 nessa execucao. O teste nao confirmou caixas sobre jogadores, pois o log aguardava um personagem local. A arquitetura, o hash e os offsets do dispositivo foram aceitos; o pool e a matriz de projecao ainda dependem da validacao em sessao.

## Executar

Use `injetar.bat`, atualizado para `--hook`. Se o GTA ja estiver aberto, ele usa a instancia unica. Se a DLL ja estiver carregada, `EspInitialize` impede criar um segundo worker nessa biblioteca.

Os prazos de janela, confirmacao e intervalo estao em `[loader]` do INI. `autoteste-hook.bat` exercita somente a janela auxiliar.
