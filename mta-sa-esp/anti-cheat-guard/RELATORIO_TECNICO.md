# Relatório técnico: análise do ESP/aim-assist e superfícies de detecção

## Escopo e conclusão executiva

O diretório de trabalho contém um cliente C++ x86 injetável para GTA:SA/MTA, não o servidor MTA nem um resource Lua. A análise cobre o código ativo em `src/`, `loader/`, `mta-sa-esp.ini`, `README.md` e os testes em `tests/`; as cópias de build e o grande snapshot de pesquisa foram excluídos do escopo funcional. Nenhum código original do cliente foi alterado.

O comportamento comprovado é principalmente leitura de dados de entidades carregadas/cacheadas e desenho de ESP. Há um módulo opcional de assistência de mira que calcula um alvo e emite movimentos de mouse locais com `SendInput`; no INI incluído, `aim.enabled=0`. Não encontrei implementação de silent aim, modificação de armas, cadência/dano, alteração de posição/velocidade do jogador ou eventos customizados enviados ao servidor. Injeção e hooks são observáveis no código cliente/loader, mas o servidor Lua não consegue inspecionar processos, DLLs, memória, arquivos ou chamadas locais do jogador.

**Limite central:** ESP/wallhack não altera necessariamente nenhum dado sincronizado. O servidor não pode provar que o jogador desenhou posições ocultas. A mira assistida pode produzir padrões estatísticos, mas nenhuma regra confiável pode ser derivada só deste código e dos eventos server-side. Por isso, o resource incluído pontua somente códigos distintos comunicados pelo anti-cheat nativo do MTA e registra telemetria agregada de combate sem transformá-la em evidência de cheat. Não executa kick/ban.

## Mapa de funcionalidades e superfície

| Funcionalidade observada | Fundamentação no código | O que faz e interação com GTA/MTA | Visibilidade server-side | Cobertura nativa / confiança |
|---|---|---|---|---|
| ESP de jogadores e peds | `src/game.cpp` (`collect_frame`); `src/render.cpp` (`Renderer::draw`); opções em `mta-sa-esp.ini` `[visual]` | Lê pool de peds, posição, vida, armadura, matriz de câmera e transforma coordenadas para tela. Desenha caixas, nomes, distância, barras, snaplines e esqueleto. `players_only=1`, `max_distance=250` no perfil. | Nenhuma chamada ao servidor necessária. Um jogador só aparece por dados locais de streaming/cache. O servidor não recebe notificação de que a informação foi desenhada ou consultada. | A fonte não prova uma detecção nativa específica para esse ESP. `onPlayerACInfo` pode fornecer sinais nativos disponíveis na versão/configuração do servidor; não garante detectar este recurso. Confiança para detectar ESP via Lua: **baixa/inexistente**. |
| ESP através de obstáculos | `src/visibility.cpp` (`trace_visibility`, `matches_drawing_type`); `src/render.cpp`; opções `[visibility]`/`[visual]` | Faz raycast local da câmera para corpo/cabeça. Quando o filtro é `All` (perfil padrão `drawing_type=0`), entidades ocluídas também são desenhadas; o filtro oculto/visível é uma escolha local. | Não é possível observar o desenho ou as consultas locais com Lua server-side. Eventos de mira/alvo não provam que informação veio de ESP. | Sem sinal específico comprovado. Capturas de tela e heurísticas de comportamento não são prova confiável. **Baixa/inexistente** server-side. |
| Assistência de mira (opcional; não silent aim) | `src/aim.hpp` (`AimController::update`); `src/runtime.cpp` (`on_end_scene`); `src/crosshair.hpp` | Seleciona entidades carregadas e elegíveis, filtra por FOV, osso, vida, distância, veículo e visibilidade; suaviza o deslocamento e chama `SendInput` para mover o mouse. Opcionalmente prevê movimento. O perfil fornecido está desligado (`aim.enabled=0`). | O servidor não vê tecla, FOV, coordenada de tela, alvo interno, velocidade do mouse nem chamada a `SendInput`. `onPlayerTarget`/`getPedTarget` só expõem alvo sincronizado e não indicam como ele foi adquirido. Dados de tiro/acerto podem alimentar revisão estatística, mas não provam a assistência. | `onPlayerACInfo` somente se um mecanismo nativo gerar um código aplicável; não há comprovação no material de que isso ocorre com este hook/overlay. **Baixa** para heurísticas de mira; sinal nativo depende do código recebido. |
| Silent aim / redirecionamento de projétil | Busca em `src/`, `loader/` e configuração; lógica de `src/aim.hpp` e evento de `SendInput` em `src/runtime.cpp` | Não localizado. O caminho implementado move a entrada do mouse; não altera o `hitElement`, o projétil, a direção da arma ou o dano por uma rotina silent aim. | Não aplicável para esta implementação. Não se deve inferir silent aim apenas por uma taxa de acerto alta. | Nenhuma detecção específica justificada. |
| Armas, tiros, munição, dano ou cadência modificados | `src/aim.hpp`, `src/runtime.cpp`, busca de chamadas/rotinas de arma e perfil | Não encontrei rotina que escreva em campos de arma, munição, dano ou cadência. O código de mira só lê posições/esqueleto e produz entrada do mouse. | `onPlayerWeaponFire` e `onPlayerDamage` permitem observação parcial; `hitElement` de `onPlayerWeaponFire` é explicitamente documentado como potencialmente incorreto. O evento de dano não inclui dano letal. | O resource agrega eventos para revisão, sem pontuá-los. Não há detecção de manipulação de arma específica fundamentada. |
| Velocidade, teleportes e movimento | `src/aim.hpp`, `src/runtime.cpp` | Não há escrita de posição/velocidade do ped nem rotina de teleporte. `SendInput` mexe no mouse, não no movimento do jogador. | Posição sincronizada pode suportar anti-cheat de movimento genérico no servidor, mas isso não seria detecção deste cliente ESP/aim. | Não implementado para evitar atribuir a este cheat uma funcionalidade que não existe. |
| Hooks, injeção e alteração de memória local | `loader/hook_transport.hpp`, `loader/main.cpp`, `src/runtime.cpp`, `src/dllmain.cpp`, `src/memory.hpp` | Loader usa mecanismo Windows de hook de mensagens para carregar a DLL; DLL intercepta slots DX9 `EndScene`/`Reset`. As rotinas de leitura inspecionadas usam leitura de memória; não encontrei escrita de memória de jogo relacionada a aim/ESP. | Processo/DLL/hooks/memória do cliente não são acessíveis a um resource server-side. O servidor não deve tentar consultar arquivos ou processos dos jogadores. | Só mecanismos nativos do cliente MTA podem eventualmente sinalizar atividade; registre `onPlayerACInfo` sem afirmar que há cobertura garantida. |
| Comunicação e eventos cliente-servidor | `src/` e `loader/` (busca por `triggerServerEvent`, eventos, rede); `src/mta_players.cpp` | O ESP consulta objetos locais de `client.dll`/`game_sa.dll`; não foi encontrado evento customizado do cheat. | Não há payload customizado deste cheat a validar. O resource não cria nem confia em eventos de cliente. | Não há detecção de manipulação de eventos específica. |

## Implementação entregue

Resource independente em `anti-cheat-guard/`:

- `meta.xml`: resource exclusivamente server-side; não distribui script client-side.
- `server.lua`: registra `onPlayerACInfo`, `onPlayerWeaponFire`, `onPlayerDamage`, join/quit e início do resource. Solicita `resendPlayerACInfo` para jogadores conectados na inicialização, como recomenda a documentação oficial.
- `config.lua`: score configurável por código AC, valor default por código, limiar de revisão e decaimento.
- `tests/anti_cheat_tests.lua`: harness Lua com APIs MTA simuladas para verificar deduplicação, score configurável, agregação de 10.000 tiros, ausência de score de tiros/dano e alerta somente de revisão.

### Política do score

O score é um **índice de triagem**, não probabilidade nem conclusão de fraude. Cada código distinto recebido via `onPlayerACInfo` soma `nativeAcWeights[code]` ou `nativeAcDefaultPoints`; reenvios do mesmo código não inflacionam score. O padrão é 10 pontos por código distinto, revisão manual a partir de 30 e decaimento de 2 pontos por minuto. O resource apenas registra a recomendação `review_only`; não expulsa nem bane.

Tiros, `hitElement`, dano, latência e partes do corpo são telemetria agregada por janela. Nenhum desses dados adiciona pontos. Assim se evita transformar latência, sincronização, arma, distância, mapa, modo de jogo, dano letal ou o aviso de `hitElement` impreciso em punição automática.

### Como instalar

1. Copie a pasta `anti-cheat-guard` para `mods/deathmatch/resources/`.
2. Inicie no console do servidor com `start anti-cheat-guard` (ou adicione o resource à configuração de resources).
3. Consulte `server.log` para registros prefixados com `[mta-esp-guard]`.
4. Ajuste `nativeAcWeights` somente após consultar o significado do código AC na documentação/versão usada pelo servidor. Não desative proteções nativas para tentar fazer o resource detectá-las.
5. Faça primeiro testes em servidor de staging. Não configure nenhuma punição automática com base neste score.

## Validação e limitações

Teste automatizado incluído (requer interpretador Lua 5.1+; não disponível no ambiente de análise):

```powershell
lua tests\anti_cheat_tests.lua
```

O teste mocka as APIs MTA e verifica o comportamento interno do resource, incluindo 10.000 eventos de tiro para assegurar agregação e falta de incremento no score. Não mede custo real de rede/servidor nem substitui teste dentro de uma instância MTA. Validação em servidor: conecte dois jogadores, dispare armas distintas, compare summaries com dano não letal e confirme que nenhuma atividade de combate altera score. Em staging, confirme que um evento AC nativo real aparece e que o mesmo código reenviado não soma pontos de novo. Conforme a documentação de `onPlayerACInfo`, a disponibilidade/uso de cada código depende de sua configuração de anti-cheat no servidor; este resource não muda `mtaserver.conf` nem recomenda desativar proteções nativas para produzir eventos.

Não é possível testar “parâmetros alterados do cheat” como se fossem detecções quando o servidor não observa as configurações client-side. Mudar FOV, suavização, tecla, osso, visibilidade ou ESP não muda a lógica server-side deste resource; precisamente por isso não se alegam falsos negativos/positivos resolvidos para aimbot/ESP.

`onPlayerDamage` não é disparado para dano que mata o jogador. `onPlayerWeaponFire` não cobre projéteis, melee ou câmera e depende de bullet sync; a documentação ainda alerta que o alvo (`hitElement`) pode estar incorreto e que coordenadas podem ser atípicas em determinadas orientações. Ping registrado é contexto, não fator de score.

## Referências oficiais de API

- [`onPlayerACInfo`](https://wiki.multitheftauto.com/wiki/OnPlayerACInfo) e [`resendPlayerACInfo`](https://wiki.multitheftauto.com/wiki/ResendPlayerACInfo): sinais disponíveis no anti-cheat MTA e reenvio no start.
- [`onPlayerWeaponFire`](https://wiki.multitheftauto.com/wiki/OnPlayerWeaponFire): campos e limitações de bullet sync/hitElement.
- [`onPlayerDamage`](https://wiki.multitheftauto.com/wiki/OnPlayerDamage): fonte, atacante, arma/body part/loss e exceção de dano letal.
- [`onPlayerTarget`](https://wiki.multitheftauto.com/wiki/OnPlayerTarget) e [`getPedTarget`](https://wiki.multitheftauto.com/wiki/GetPedTarget): informação de alvo sincronizada, sem prova do método de mira.

## Trechos de código-fonte fundamentadores

Os números são relativos aos arquivos ativos; a fonte original não foi modificada.

- `src/aim.hpp`: `AimController::update` (seleção, filtro de FOV/visibilidade/veículo, previsão, suavização e passos de input), linhas aproximadas 12–91.
- `src/runtime.cpp`: `on_end_scene`, leitura de âncora/câmera, permissão contextual e chamada a `SendInput`, linhas aproximadas 43–112.
- `src/crosshair.hpp`: leitura e validação local do ponto de mira do HUD, linhas 8–31.
- `src/game.cpp`: leitura do pool e dados locais de ped, dimensões, vida, posição e elegibilidade de alvo, linhas 14–112.
- `src/mta_players.cpp`: associação de jogadores/nome/dimensão e posição cacheada via objetos locais do MTA, linhas 44–114.
- `src/visibility.cpp`: raycast local e filtro de visibilidade, linhas 7–28.
- `src/render.cpp`: desenho do FOV, alvo, caixas, snaplines e esqueleto, linhas 158–228.
- `src/config.hpp` e `mta-sa-esp.ini`: configuração de ESP e aim; `aim.enabled=0` no perfil fornecido.
- `loader/hook_transport.hpp`, `loader/main.cpp` e `src/runtime.cpp`: transporte do hook e interceptação DX9; não oferecem uma interface de observação remota ao servidor.
