# Esqueleto, nomes e indicador superior

Perfil conferido nos arquivos instalados em 2026-10-18. A leitura do GTA exige o SHA256 ja registrado no INI. A leitura da lista MTA exige tambem:

| Modulo | SHA256 |
| --- | --- |
| client.dll | D386DBB3794A559236278E457BA83D5677B643F177B3EBFFDE1FAE1AB46D43AC |
| game_sa.dll | D17A1ED8ABD31F3E2D9E75ECA9CA6A50A46811B741D7D8D8E83834B52EDE3402 |

`client.dll!InitClient` aponta para a vtable de CClient em RVA 0x3BAB08. A entrada GetPlayerNames, indice 10 no CClientBase publico, aponta para RVA 0x19500. Essa funcao confirma g_pClientGame em RVA 0x4B6960, player manager no campo 0x40, inicio/fim do vetor em 0x10/0x14 e string de nametag em 0x6DC. A vtable primaria CClientPlayer identificada por RTTI esta em RVA 0x3C10F4. GetGameEntity/GetPosition confirmam o campo do wrapper em 0x19C.

O vinculo wrapper -> CPed replica, por leituras, os ajustes de heranca virtual das rotinas examinadas. Nao chama getters C++ de outro runtime. No game_sa.dll instalado, a entrada GetPosition do CEntity aponta para o thunk RVA 0x57766 e a implementacao RVA 0x1A1F0, que acessa o CPed nativo. Esses ajustes sao constantes nomeadas em mta_players.cpp e dependem dos hashes acima.

No GTA instalado, RpSkinAtomicGetHAnimHierarchy em VA 0x7C7540 le o deslocamento de plugin em VA 0xC978A4. RpClumpForAllAtomics em VA 0x749B70 confirma a lista em clump+0x8 e o link em atomic+0x40. O leitor faz essa travessia com limite, le RpHAnimHierarchy e resolve as tags da tabela de nodes para a matriz correspondente. Posicoes nao finitas ou distantes demais do personagem sao rejeitadas.

Validacao executada:

- Build i386 com -Wall -Wextra -Wpedantic -Werror, sem avisos.
- 27 verificacoes: inclui string pequena/grande do MSVC, UTF-8, remocao de cor, rejeicao de nomes invalidos, associacao por ponteiro, vtable incorreta, duplicidade, ordem de ossos invertida, limite de hierarquia e lista ciclica.
- Dispositivo D3D9 HAL isolado: pixels de caixa, vida, esqueleto, nome acentuado e faixa cinza no topo; estados/viewport restaurados; repeticao apos Reset.
- A imagem render-check.png e uma cena sintetica de teste.
- A consulta somente de leitura ao GTA aberto nao encontrou client.dll carregado; nao houve validacao do esqueleto ou nomes sobre outros jogadores reais.

A faixa superior mede 32 x 2 pixels e nao mostra texto. INSERT, F5 e END continuam operando. Os estados de leitura continuam no log.

Referencias: [RenderWare do MTA](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/sdk/game/RenderWare.h), [CClientPlayer](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/mods/deathmatch/logic/CClientPlayer.h), [CClient](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/mods/deathmatch/CClient.cpp), [CClientPed](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/mods/deathmatch/logic/CClientPed.cpp).
