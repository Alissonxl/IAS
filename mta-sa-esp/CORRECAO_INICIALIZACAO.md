# Inicializacao por sessao — startup-order-v1

O codigo original usa RVAs somados a base de gta_sa.exe obtida por GetModuleHandleW em cada novo processo. As listas de jogadores sao refeitas em cada coleta. Nao ha endereco absoluto ou lista de personagens de uma sessao anterior salvo no perfil.

O log de 18/10/2026 mostrou uma sequencia relevante: callbacks instalados as 15:35:30, modulos MTA confirmados apenas as 15:36:18 e nenhuma linha de coleta nessa tentativa. Isso sustenta a hipotese de vinculo inicial feito antes de o MTA concluir a criacao do dispositivo/proxy. O log original nao prova qual dispositivo era usado depois.

## Alteracao aplicada

O painel continua disponivel durante a espera. A instalacao inicial dos mesmos callbacks EndScene/Reset agora exige:

1. Modulos MTA carregados e com identidades verificadas correspondentes ao perfil atual.
2. Ponteiro do personagem local disponivel nesta sessao.
3. Dispositivo e vtable atuais estaveis por um segundo apos essas condicoes.

Uma troca de dispositivo/vtable durante a espera reinicia a contagem. Perda dos modulos ou do personagem invalida a prontidao. Cada processo tem sua propria instancia do controle, sem herdar o estado da sessao anterior. A instalacao acontece uma unica vez, depois dessa espera.

O log informa versao, caminho da DLL, PID, base do executavel, etapas de espera e dispositivo/vtable selecionados. A cada cinco segundos apos a instalacao registra callbacks recebidos, chamadas selecionadas, coletas e entidades. Assim e possivel distinguir falta de callback de uma coleta que descarta os jogadores.

Nao foram alteradas a enumeracao dos jogadores, a conversao dos vinculos MTA para personagens, offsets, filtros de dimensao/distancia, leitura dos ossos, projecao ou desenho. Nao foi adicionado callback Present nem reinstalacao automatica de hooks. O codigo de coleta original permanece preservado no backup.

## Validacao e limites

Passaram 64 verificacoes de dados/configuracao/projecao/inicializacao, incluindo dez verificacoes novas para inicializacao tardia, troca de dispositivo/vtable, perda de prontidao, nova sessao e identidade ainda nao verificada. Passaram o transporte no processo auxiliar e nove verificacoes do atualizador. Compilacao x86 C++20 com O2 e avisos estritos tratados como erros.

Nao foi iniciado nem injetado o MTA durante a validacao. A hipotese depende de uma nova execucao para confirmacao; esta alteracao nao comprova compatibilidade com a verificacao de integridade do jogo. Os testes visuais nao foram repetidos, pois o desenho nao foi alterado.

Feche totalmente a sessao anterior antes de executar o injetar.bat da raiz. O BAT usa bin e aplica o par DLL/INI pendente caso uma sessao aberta tenha bloqueado a atualizacao. O perfil visual instalado e preservado.

Nesta entrega, o Windows bloqueou a substituicao de bin/mta-sa-esp.dll por estar em uso. A DLL original continua instalada; a nova DLL e o perfil validado estao em bin/pendente. O atualizador nao substituiu arquivos a forca. O pacote ZIP contem a compilacao nova diretamente em bin.

SHA256 da compilacao startup-order-v1: BB5D35C1E44F8529D0E37865AE51B92B2146A73501BDEFFA5479B99FCF5C3125.
