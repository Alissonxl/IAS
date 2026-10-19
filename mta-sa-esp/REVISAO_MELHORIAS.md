# Revisao de usabilidade e selecao - panel-review-v6

A revisao corrigiu problemas reproduzidos e reduziu trabalho por frame, mantendo os controles existentes.

- Captura de tecla: ao ocultar o jogo ou trocar de aplicativo, cancela a captura pendente, menus, editor de cor e arraste. O pump verifica visibilidade/foco antes de consultar teclas globais, impedindo que uma captura escondida continue registrando teclas.
- Previsao: mudar cabeca/peito/quadril limpa o historico de velocidade e o residuo de movimento. Antes, a distancia entre dois ossos podia ser interpretada como deslocamento do alvo.
- Ativacao: mudar a tecla ou o modo de ativacao exige soltar o novo controle antes de armar novamente. Uma alternancia ativa nao e transferida para outra configuracao.
- Selecao: frames comuns procuram o candidato mais proximo em uma passagem; a ordenacao completa fica restrita ao comando de trocar alvo. A trava e o desempate por identidade continuam deterministas. Nao foi medido ganho de FPS no MTA.
- Previa: o raio inicial coincide com o ajuste de 70 px e o alvo ilustrativo deixa de ser destacado quando esta fora do circulo exibido.

## Evidencia

A suite de mira ampliada, compilada contra a copia anterior, apresentou 3 falhas: troca de osso, troca de modo e troca de tecla segurada. Isso confirma as regressoes sem enviar entrada de mouse. A versao corrigida e validada pelos mesmos testes, alem dos testes de cancelamento de captura, controles e renderizacao.

As verificacoes sao feitas com poses sinteticas, arquivos locais e janelas auxiliares. A precisao da mira, a sensibilidade e o desempenho dentro do MTA continuam dependentes de validacao no jogo. Nao foram adicionados hooks ou alterados offsets nesta revisao.

Validacao final: compilacao estrita x86; 35 testes de mira, 19 de foco/captura, 23 da aba Mira, 34 da aba Visual e 9 de pixels do cursor passaram, assim como as verificacoes gerais 70, sincronizacao 8, restauracao de cursor 9, hook auxiliar e atualizacao 9 executadas pelo build. A versao anterior falhou em 3 dos novos cenarios; a corrigida passou nos mesmos cenarios. Nao houve operacao da mira ou medicao de FPS no MTA.

DLL SHA256: AD4BE9FD05E9696E27A718E29F625C20262F335BEE1C585221788F59556A84F3. Atualizacao: 0 (6 = instalada em uso, nova versao em bin/pendente). Perfis existentes preservados.
