// Shared precision controls and contextual help for the Visual and Mira pages.
bool Panel::adjust_control(int x,int y,int direction,bool reset,bool fine,bool coarse) {
    if (popup_ || key_capture_ || selected_color_) return false;
    float* value{}; float low{},high{},step{},initial{};
    if (!tab_ && hit(x,y,414,406,272,72)) {
        value=&values_.max_distance; low=1; high=3000; step=fine?1.0f:10.0f; initial=500;
    } else if (tab_==1) {
        if (hit(x,y,414,218,272,60)) { value=&values_.aim.fov; low=1; high=600; step=1; initial=70; }
        else if (hit(x,y,414,292,272,60)) { value=&values_.aim.smooth; low=1; high=30; step=.1f; initial=2; }
        else if (hit(x,y,414,366,272,60)) { value=&values_.aim.prediction_ms; low=0; high=200; step=1; initial=0; }
        else if (hit(x,y,44,538,312,58)) { value=&values_.aim.input_gain; low=.01f; high=5; step=.01f; initial=1; }
    }
    if (!value) return false;
    if (coarse) step*=10;
    const float next=reset?initial:std::clamp(std::round((*value+static_cast<float>(direction)*step)*100)/100,low,high);
    if (*value!=next) { *value=next; changed(); }
    return true;
}
std::wstring Panel::footer_text() const {
    std::wstring hint=status_;
    const int x=hover_x_,y=hover_y_;
    if (key_capture_) hint=binding_error_?L"Tecla em uso por outro atalho. Escolha outra ou pressione ESC.":
        L"Pressione a tecla desejada. ESC cancela; Delete limpa a troca de alvo.";
    else if (popup_) hint=L"Escolha uma opcao. Clique fora para fechar sem alterar.";
    else if (selected_color_) hint=L"Paleta e RGB alteram somente esta cor. ESC fecha o editor.";
    else if (hit(x,y,880,628,216,42)) hint=L"Salvar as duas abas no perfil. Atalho: Ctrl+S.";
    else if (hit(x,y,430,24,110,36)) hint=L"Visual: nomes, caixas, esqueleto, vida e colete.";
    else if (hit(x,y,550,24,110,36)) hint=L"Mira: selecao, Tap manual, FOV e calibracao do mouse.";
    else if (tab_==1) {
        if (hit(x,y,414,218,272,60)) hint=L"Raio em pixels ao redor da mira real. Role para ajuste fino; botao direito restaura.";
        else if (hit(x,y,414,292,272,60)) hint=L"Maior suavizacao = correcao mais gradual. Role para ajustar; botao direito restaura.";
        else if (hit(x,y,414,366,272,60)) hint=L"Previsao em milissegundos; zero desliga. Role para ajustar; botao direito restaura.";
        else if (hit(x,y,44,538,312,58)) hint=L"Ganho: reduza se ultrapassar o alvo; aumente se faltar movimento. Role ou arraste.";
        else if (hit(x,y,742,472,332,40)) hint=L"Tap: segure a tecla para alinhar antes do tiro. Verde indica alinhamento; tiro e Q sao manuais.";
        else if (hit(x,y,420,542,260,40)) hint=L"Clique e pressione uma tecla ou botao do mouse. Abrir o painel pausa a mira.";
        else if (hit(x,y,742,550,332,40)) hint=L"Atalho opcional para trocar alvo. Delete remove; ESC cancela a captura.";
        else if (hit(x,y,420,452,260,40)) hint=L"Seleciona um osso real carregado: cabeca, peito ou quadril.";
        else if (hit(x,y,312,362,40,24)) hint=L"Cor do destaque do alvo. No Tap, verde indica que a mira estabilizou.";
        else if (hit(x,y,40,218,262,32)) hint=L"Permite alvos de jogadores carregados na mesma dimensao.";
        else if (hit(x,y,40,265,262,32)) hint=L"Inclui NPCs carregados. A selecao continua respeitando distancia e visibilidade.";
        else if (hit(x,y,40,312,262,32)) hint=L"Mantem o alvo atual enquanto ele permanecer elegivel dentro do FOV.";
        else if (hit(x,y,40,453,262,32)) hint=L"Exclui alvos ocultos e casos em que a visibilidade nao pode ser confirmada.";
    } else {
        if (hit(x,y,24,92,1072,52)) hint=L"Este interruptor controla somente o Visual. A Mira tem ativacao independente.";
        else if (hit(x,y,414,406,272,72)) hint=L"Distancia: roda=10 m; Ctrl=1 m, Shift=100 m. Botao direito restaura 500 m.";
        else if (hit(x,y,420,252,260,40)) hint=L"Filtra o desenho por visibilidade; nao altera os candidatos da Mira.";
        else if (hit(x,y,420,342,260,40)) hint=L"Caixa completa ou apenas os cantos. A posicao segue a mesma projecao.";
        else {
            constexpr std::array<const wchar_t*,10> hints{L"",L"Exibe o nome recebido pelo MTA.",
                L"Distancia em metros; registros distantes em cache sao identificados como cache.",
                L"Desenha uma caixa ao redor do personagem.",L"Liga a base da tela ao personagem com uma linha.",
                L"Desenha somente os ossos que puderam ser lidos e projetados.",
                L"Exibe tambem outras dimensoes no Visual. A Mira continua limitada a alvos elegiveis.",
                L"Barra de vida proporcional a vida maxima lida.",L"Barra de colete ao lado da vida, quando o valor estiver disponivel.",
                L"Exibe os valores numericos de vida e colete."};
            for (std::size_t i=1;i<row_y.size();++i) {
                if (hit(x,y,40,row_y[i],195,30)) hint=hints[i];
                else if (hit(x,y,248,row_y[i],116,30)) hint=L"Clique para editar a cor: visivel a esquerda, oculto a direita.";
            }
        }
    }
    if (unsaved_) hint=L"N\u00c3O SALVO | "+hint;
    return hint;
}
