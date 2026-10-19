# Resultado da investigacao do acesso ao GTA

Este relatorio registra a limitacao do metodo anterior por alocacao remota. A nova rota por hook confirmou entrada no GTA em 2026-10-18; consulte `VALIDACAO_HOOK.md`. A mascara de direitos antiga nao foi alterada.

A consulta foi executada diretamente no PID 17276, sem alocar/escrever memoria nem criar thread remota.

```text
Carregador: integridade=alto (0x3000), elevado=sim
Processo selecionado: integridade=alto (0x3000), elevado=sim
Handle: solicitado=0x0000043A concedido=0x00001410 ausente=0x0000002A
  AUSENTE: PROCESS_CREATE_THREAD
  AUSENTE: PROCESS_VM_OPERATION (exigido por VirtualAllocEx)
  AUSENTE: PROCESS_VM_WRITE
```

O executavel consultado foi `C:\ProgramData\MTA San Andreas All\1.6\GTA San Andreas\gta_sa.exe`.

## Conclusao demonstrada

`OpenProcess` retornou um handle, mas esse handle nao continha tres direitos solicitados. A consulta de `ObjectBasicInformation` verificou a mascara realmente concedida; sucesso em `OpenProcess` sozinho nao bastou para confirmar as permissoes necessarias. A ausencia de `PROCESS_VM_OPERATION` explica a falha de `VirtualAllocEx` nesse handle.

A diferenca de nivel de integridade nao explica o resultado observado. O autoteste em processo auxiliar passou com a mesma mascara solicitada; nele, a mascara concedida foi `0x0000143A`, sem direitos ausentes.

Esta investigacao nao identifica qual componente restringiu o acesso. Nao houve alteracao de permissoes, politicas de seguranca, servicos ou drivers. A DLL nao foi carregada no jogo.

## Alteracao entregue

O carregador agora consulta as permissoes do proprio handle antes da alocacao. Se faltarem direitos, apresenta os nomes e retorna codigo 3 antes de modificar o processo. A verificacao tambem aparece em `injetar.bat /diagnosticar` e no autoteste.

O bloqueio do caminho anterior de `LoadLibraryW` permanece: ele depende de obter um handle com os direitos exigidos. Editar offsets ou mudar o tamanho da pagina nao supre direitos ausentes nesse handle. A alternativa entregue usa a API de hook de mensagens e nao solicita essas tres permissoes.

Referencias: [consulta da mascara GrantedAccess](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryobject), [permissao exigida por VirtualAllocEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualallocex).
