# Painel com controle do cursor MTA - panel-camera-v4

O painel solicita ForceCursorVisible(true,true) pela interface do MTA na thread de renderizacao/UI do jogo. A primeira chamada guarda a visibilidade forcada e a opcao de suspensao de controles anteriores. Fechar o painel, ocultar ao trocar de aplicativo ou encerrar a ferramenta solicita a restauracao no callback seguinte do jogo. Se outro componente alterar esses estados, a restauracao nao sobrescreve a alteracao observada.

O painel continua WS_EX_NOACTIVATE; nao desativa a janela do GTA nem transfere foco. O cursor desenhado na frente do painel agora usa degrade branco/azul, destaque azul, contorno arredondado e sombra. A composicao dupla preserva a imagem de fundo sem rastros.

## Vinculo confirmado nesta instalacao

Perfil separado mta-sa-esp-input.ini, ao lado da DLL. O codigo verifica SHA256 de client.dll e core.dll, o ponteiro de interface salvo na inicializacao do client, a vtable exata e os tres alvos usados para consultar/restaurar o cursor. O perfil foi conferido offline com tests/verify_input_profile.py contra os arquivos instalados. Nao se usam os indices do cabecalho atual sem conferir a DLL: ele possui metodos adicionais em relacao a este binario.

A preparacao e o hash ocorrem no worker. A chamada acontece apenas no EndScene existente do dispositivo principal e somente quando a thread corresponde a janela do jogo. Continua funcionando com a caixa Ativado desmarcada. Nao foram adicionados hooks de teclado, mouse, Present ou WndProc. Se o perfil divergir, nenhuma funcao de cursor e chamada e o log informa o motivo.

Referencias primarias: [CCore::ForceCursorVisible](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/core/CCore.cpp), [CClient::ClientInitialize](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/mods/deathmatch/CClient.cpp), [roteamento de DirectInput para GUI](https://github.com/multitheftauto/mtasa-blue/blob/master/Client/core/DXHook/CProxyDirectInputDevice8.cpp). A leitura do binario local confirma a assinatura x86 de dois argumentos e os RVAs desta instalacao.

## Validacao e uso

Compilacao x86 com avisos como erros; 9 testes de restauracao de estado com core simulado, 8 verificacoes offline do perfil binario, 9 verificacoes de pixels do cursor, 16 de foco, 34 controles do painel, 8 de sincronizacao, 66 gerais, hook auxiliar e 9 de atualizacao passaram. Nenhum teste desta entrega controlou o MTA real; a suspensao da camera precisa de confirmacao dentro do jogo.

Feche o GTA/MTA por completo e execute injetar.bat. O perfil de entrada deve permanecer junto da DLL. Procure no log a versao panel-camera-v4 e Cursor MTA: interface verificada. Se aparecer perfil ausente/hash divergente, o painel continua disponivel, mas a suspensao nativa nao e aplicada.

DLL SHA256: 02FB42FF03FC506155C780D98762624D1CE2D842158E46DA6AEE8C877BF628A1. Retorno da atualizacao: 6 (6 = DLL em uso, atualizacao pendente).
