# Correcao de distancia e custo por quadro

**HISTORICO - RETIRADO DA ENTREGA ATUAL.** A implementacao descrita neste documento foi substituida pela base anterior a mudanca de distancia. Consulte `RESTAURACAO_ESTAVEL.md`. Os parametros e resultados abaixo nao representam o codigo instalado atualmente.

**Atualizacao de recuperacao:** veja `CORRECAO_ESP.md`. O descarte antecipado foi desligado por padrao e o runtime voltou a apresentacao continua. O benchmark abaixo mede a variante experimental com culling explicito; seu percentual nao representa a configuracao atual entregue. As demais otimizacoes e a correcao de distancia permanecem.

Revisao de 2026-10-18, mantendo painel, cores, filtros, caixas, ossos e barras.

## Distancia

O slider e os filtros ja aceitavam 3000 m. Havia dois cortes independentes:

1. A coleta de posicoes em cache so era habilitada ao marcar Todas dimensoes. Com o filtro normal, alvos sem personagem GTA carregado eram omitidos mesmo com cache da mesma dimensao.
2. O renderer descartava a entidade inteira quando a altura projetada era inferior a 3 pixels, eliminando tambem nome e distancia.

A coleta agora considera o cache em ambos os modos de dimensao, preservando a verificacao da dimensao local e do alvo. O renderer usa marcador compacto para posicoes em cache e corpos pequenos; nome/distancia continuam legiveis. As barras e o esqueleto nao sao inventados para registros em cache. O filtro Visiveis/Ocultos continua excluindo cache, cuja visibilidade e desconhecida; use Todos para ve-lo.

3000 m e o limite de desenho dos dados que o cliente possui. Esta revisao nao amplia o streaming, nao solicita sincronizacao adicional e nao recupera dados ausentes do servidor. Posicoes em cache podem estar desatualizadas, especialmente em veiculos/anexos, cujo posicionamento exige outras fontes. A referencia e o campo em cache do perfil fingerprintado existente, sem novos offsets.

## Desempenho

- Candidatos fora da tela sao descartados antes de atributos, raios de visibilidade e ossos. A projecao considera a proporcao real do backbuffer e mantem uma margem nas bordas.
- Coleta suspensa quando o jogo perde o primeiro plano.
- Status rotineiro sai do callback EndScene. O worker grava somente mudancas, respeitando status_log_ms. Falhas excepcionais continuam sendo registradas imediatamente.
- Com Nome desativado, a lista continua validando os vinculos/dimensoes sem ler ou converter nametags.
- Vtable, wrapper e dimensao sao lidos em um bloco limitado a 1024 bytes. Layouts dispersos usam as leituras individuais anteriores.
- Strings curtas reutilizam os bytes ja lidos do objeto MSVC, evitando uma segunda leitura.
- Uma revisao imutavel ja apresentada nao repete a construcao de geometria nem o PresentEx. Resize, perda/recuperacao da janela e dados novos invalidam esse reaproveitamento.
- O painel mantem um buffer GDI+ reutilizavel. Mover o mouse dentro do mesmo controle nao dispara uma pintura completa.
- A cadencia de coleta permanece 25 ms; esta revisao nao desliga funcoes nem reduz sua frequencia configurada.

Chaves opcionais em [runtime]:

~~~ini
screen_culling=1
status_log_ms=2000
~~~

screen_culling=0 permite comparar com a coleta anterior. status_log_ms aceita 250 a 30000 ms.

## Validacao

O build x86 usa C++20, -Wall, -Wextra, -Wpedantic e -Werror. Os testes cobrem limites inclusivos em 1000/3000 m, exclusao acima do limite, cache da mesma dimensao, bloqueio de outras dimensoes, falta de metadados, ausencia de atributos ficticios, visibilidade desconhecida e alvos pequenos na renderizacao DX9. O teste de pixels gera render-far.png.

Os testes tambem verificam descarte de candidatos antes da visibilidade, variante com culling desligado, borda do viewport em retrato, leitura individual de metadados dispersos, controles sem repintura redundante, reaproveitamento de revisoes e invalidacao desse reaproveitamento ao redimensionar/restaurar. As suites de foco, captura auxiliar e atualizador permanecem ativas.

O benchmark mta-sa-perf-checks.exe usa 128 personagens sinteticos, metade em frente e metade atras da camera, 7 rodadas de 100 coletas por variante e mediana do tempo de CPU. A checagem de visibilidade e substituida por uma funcao leve e o esqueleto fica desativado nas duas variantes. O resultado mede somente a coleta da fixture, sem representar FPS reais do MTA. O log do build inclui a medicao.

Na execucao final: 37,187 ms por lote sem culling e 29,431 ms com culling, reducao de 20,9%. Foram preservados os 64 alvos em tela e evitadas 64 chamadas de visibilidade por coleta. Resultados de tempo variam com a carga do Windows; os testes nao exigem um ganho percentual minimo.

Passaram 133 verificacoes: 67 de memoria/configuracao/entidades, 36 do painel, 15 do overlay, quatro passagens de pixels DX9, um benchmark funcional, um transporte de hook auxiliar e nove do atualizador.

Esta versao ainda nao foi executada numa sessao real do MTA. A melhora de FPS e o alcance efetivo dependem da cena, dos dados recebidos e da composicao grafica do Windows.

## Atualizacao

O ZIP contem fontes e binarios novos. A atualizacao local fica em bin/pendente para aplicar apos fechar o GTA/MTA e executar injetar.bat. O perfil instalado e preservado na preparacao; o alcance solicitado passa a 3000 m e entram as chaves novas de desempenho. Backups da entrega anterior ficam em build/before-distance-perf.

## Referencias

- [MTA: elementos carregados fisicamente e virtualizados](https://wiki.multitheftauto.com/wiki/IsElementStreamedIn).
- [MTA: criacao do objeto GTA no stream-in](https://wiki.multitheftauto.com/wiki/OnClientElementStreamIn).
- A implementacao local de referencia de CClientPed::GetPosition esta em research/CClientPed.cpp; o ramo sem ped/veiculo/anexo usa a posicao armazenada.

