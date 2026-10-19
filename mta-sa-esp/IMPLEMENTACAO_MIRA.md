# Abas Visual e Mira - panel-aim-v5

O painel existente agora tem duas abas no cabecalho. Visual conserva os controles anteriores. Mira oferece ativacao independente, jogadores/NPCs, manter alvo, destaque com cor editavel, filtro de veiculos, checagem de visibilidade, circulo de FOV, suavizacao, previsao por velocidade, cabeca/peito/quadril, tecla configuravel, modos segurar/alternar e tecla opcional para trocar de alvo. Ambos os grupos sao salvos atomicamente pelo mesmo botao Salvar perfil.

## Como usar

Abra com INSERT e selecione Mira. A configuracao inicial vem desligada; ao ativar, a tecla padrao e o botao esquerdo do mouse, em modo Segurar tecla. Os botoes de tecla aceitam teclado e botoes laterais do mouse. Escape cancela a captura; Delete remove a tecla opcional de troca. Atalhos reservados do painel e teclas duplicadas sao rejeitados.

O FOV e o raio em pixels em torno do centro da janela, e a previsao usa milissegundos (0 a 200). A distancia maxima segue a aba Visual. Suavizacao maior deixa a correcao mais gradual. O osso precisa estar disponivel no esqueleto carregado; nao se substitui um osso ausente por uma posicao aproximada.

## Implementacao

src/aim.hpp separa selecao, identificacao, previsao e suavizacao do envio de entrada. A selecao usa apenas personagens vivos carregados, na dimensao verificada, com identidade formada pelo slot e sua geracao. A coleta de alvos e separada dos filtros de desenho. Desativar nomes, caixas, Visual ou selecionar apenas ocultos nao remove os candidatos da Mira. Alvos distantes existentes apenas no cache nao podem ser usados pela mira.

O movimento utiliza SendInput relativo, limitado por passo, sem escrever angulos da camera. input_gain e max_step na secao aim sao parametros de calibracao conforme a sensibilidade do jogo/Windows. A previsao usa velocidade observada, descarta saltos implausiveis e nao simula balistica de projeteis.

A entrada fica suspensa com o painel, captura de tecla, chat, menu/configuracoes MTA, perda de foco, dados expirados ou fim da ferramenta. Depois de fechar um menu, uma tecla que ja estava segurada precisa ser solta antes de iniciar a mira. A falha de SendInput interrompe as tentativas ate desligar e religar Mira. O funcionamento real do movimento depende da entrada aceita pelo jogo; nao foi validado ao vivo nesta entrega.

O perfil mta-sa-esp-input.ini continua verificando client/core por SHA256 e agora tambem valida os tres metodos usados para consultar chat/menu/configuracoes. Referencias primarias: [CPedSAInterface e flags de veiculo](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/game_sa/CPedSA.h) e [interface do core](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/sdk/core/CCoreInterface.h). O byte de veiculo 0x46D e a mascara 1 ficam editaveis no INI, vinculados ao hash do GTA ja exigido pelo projeto.

## Validacao

Compilacao x86 com avisos tratados como erros. Passaram 30 verificacoes de selecao/movimento com poses sinteticas e sem enviar entrada, 23 controles/salvamento da aba Mira, 34 da aba Visual, 9 de pixels do cursor, 16 de foco, 4 passagens DX9 com verificacao de pixels/estado (incluindo Mira com Visual desligado), 70 gerais/coleta, 8 de sincronizacao, 9 de restauracao de cursor, hook auxiliar e 9 da atualizacao. O perfil do core passou em 11 verificacoes offline contra as DLLs instaladas. Nenhum teste desta versao operou a mira no MTA.

Feche GTA/MTA completamente e execute injetar.bat da raiz para atualizar a DLL. O log deve indicar panel-aim-v5. A atualizacao preserva o perfil Visual existente e acrescenta a secao aim desligada.

DLL SHA256: C0E794BE1451225AAE8BD6C2EAD4574FC723CF52CDE12EC8DC954BE24081E61C. Retorno da atualizacao: 0 (6 indica DLL instalada em uso e nova versao em bin/pendente).
