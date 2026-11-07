# Retrô Emu (gba-frontend)

Emulador de Game Boy Advance para Windows. Escrevi apenas a "casca" em C (janela, biblioteca de jogos, configurações, teclas, áudio, save); todo o emulador em si vem da **libmgba** (núcleo do [mGBA](https://github.com/mgba-emu/mgba), tag **0.10.5**).

> Nenhuma ROM nem BIOS faz parte deste projeto. Use homebrew livre (por exemplo [jsmolka/gba-tests](https://github.com/jsmolka/gba-tests)) ou cópias que você mesmo possui. O mGBA roda sem BIOS real (usa uma BIOS embutida).

## Uso

- **Abrir o programa sem argumentos** mostra a biblioteca (janela 1280x720, tema escuro). Na primeira vez ele abre o seletor de pastas do Windows para você escolher a pasta das ROMs; a escolha fica salva.
- **Arrastar uma `.gba` em cima do `gameboy front.exe`** (ou `gameboy front.exe "caminho\da\rom.gba"`) abre o jogo direto.
- **`gameboy front.exe --demo`** mostra a biblioteca com 10 jogos fictícios, só para visualizar a interface ("Jogar" fica desligado).

### Nitidez e escala do Windows

O programa avisa ao Windows que cuida da escala (por monitor) e desenha a interface **direto no tamanho real da janela**, em pixels: com a escala do Windows em 125%, a janela de 1280x720 pontos tem 1600x900 pixels reais e tudo é desenhado nesse tamanho (linhas retas alinhadas à grade, cantos e círculos suavizados, texto gerado no tamanho final). Nada é esticado depois de pronto. Ao redimensionar a janela, maximizar ou trocar de monitor, a interface é redesenhada na hora e mantém a proporção 16:9 (faixas na cor da janela quando a janela tem outra proporção). Tamanho mínimo da biblioteca: 960x540 pontos. Quando nada muda, a biblioteca não é redesenhada (quase 0% de CPU parada).

### Biblioteca

Barra de título própria (arraste para mover; minimizar, maximizar, fechar), barra de ferramentas (Adicionar jogo, Jogar, Configurações, Controles, busca) e a tabela de jogos, na largura toda, com título, plataforma, gênero, ano, tempo de jogo e última sessão. A barra de status mostra a quantidade de jogos e o jogo selecionado. Não há barra de menus nem lista de plataformas (por enquanto só existe o GBA).

- Clique numa linha para selecionar; clique duplo ou Enter abre o jogo.
- A busca filtra por título, plataforma e gênero (sem diferenciar maiúsculas nem acentos).
- O tempo de jogo e a última sessão são medidos de verdade e gravados em `%APPDATA%\gba-frontend\library.ini`. A miniatura de cada jogo é o último quadro da sessão (`%APPDATA%\gba-frontend\thumbs\`).
- A lista mostra os `.gba` da pasta escolhida e de até 3 níveis de subpastas.

| Tecla | Ação |
|---|---|
| Setas, PgUp/PgDn, Home/End | Navegar na tabela |
| Enter | Abrir o jogo |
| Ctrl+F | Ir para a busca (ESC limpa o texto, ESC de novo sai da busca) |
| Tab / Shift+Tab | Percorrer menus, barra de ferramentas, busca, sidebar e tabela |
| F1 | Escolher a pasta de ROMs ("Adicionar jogo") |
| F2 | Tela Controles |
| F5 | Atualizar a lista de jogos |

O controle navega na biblioteca: direcional, A = Enter, B = ESC, ombros = PgUp/PgDn.

### Configurações

Botão **Configurações** (ou Ferramentas > Configurações). As mudanças valem na hora e ficam salvas.

- **Som:** volume de 0 a 100 e "Mudo".
- **Velocidade:** multiplicador do avanço rápido (1,5x, 2x, 3x, 4x, 6x, 8x) e a caixa "Manter o avanço rápido sempre ativo": o jogo já começa acelerado (F4 desliga na sessão; Tab e o gatilho direito continuam acelerando enquanto seguros).
- **Turbo:** "Repetir botão ao segurar (turbo)": enquanto você segura um botão, o jogo recebe apertar/soltar repetidamente. Escolha a velocidade da repetição (5, 10, 15 ou 30 por segundo) e **quais botões repetem** (A, B, L, R, Start, Select e o direcional; padrão A e B). Começa desligado.
- **Analógico:** "Usar o analógico como direcional": o analógico esquerdo do controle move o personagem como o direcional (8 direções). A **zona morta** (20% a 80%, padrão 50%) define o quanto é preciso inclinar o analógico para contar como um aperto; abaixo disso ele é ignorado. Começa ligado.
- **Tela:** "Preencher a tela" (amplia o máximo possível mantendo a proporção 3:2, com barras pretas) ou "Inteira" (só múltiplos inteiros, pixels nítidos); e "Abrir jogos em tela cheia".

### Jogando

Teclas padrão (mudam na tela Controles): setas = direcional, Z/X = A/B, A/S = L/R, Enter = Start, Backspace = Select.

| Tecla | Ação |
|---|---|
| Tab (segurar) ou gatilho direito (RT) | Avanço rápido |
| F4 | Liga/desliga o avanço rápido fixo |
| F6 / F7 | Diminui / aumenta o multiplicador do avanço |
| F9 / F10 | Volume − / + |
| F3 | Mudo |
| F11 ou Alt+Enter | Tela cheia |
| F5 / F8 | Salvar / carregar estado |
| F12, Guide ou L3+R3 (controle) | Abre o menu do jogo (pausa o jogo) |
| ESC | Sai da tela cheia; em janela, volta à biblioteca (o save é gravado). Com o menu do jogo aberto, só fecha o menu |

Durante o avanço rápido o jogo fica **sem som** (acompanhar a velocidade com áudio exigiria reamostrar o som); ao soltar, o som volta. Avisos curtos (volume, velocidade, estado salvo) aparecem no canto superior esquerdo do jogo.

O save do jogo (`.sav`) e o estado (`.ss1`) ficam ao lado da ROM, com o mesmo nome. Carregar um estado não sobrescreve o `.sav`. A janela do jogo abre em 3x (720x480) e pode ser redimensionada ou maximizada: a imagem nunca é deformada. O título mostra os quadros por segundo.

### Menu do jogo

Durante o jogo, **F12** (ou **Guide**, ou **L3+R3** juntos no controle) abre um painel por cima da imagem e **pausa o jogo**: assim dá para mexer no emulador sem voltar à biblioteca. Itens: Volume, Mudo, Velocidade do avanço rápido, Turbo, Salvar estado, Carregar estado, as macros (escolher, tocar e gravar; ver abaixo), **Sair do jogo** e Voltar ao jogo. "Sair do jogo" fecha o jogo e volta à biblioteca, como o ESC (o save é gravado e uma gravação de macro em andamento é descartada); pede confirmação: o primeiro Enter, A ou clique só arma ("Confirmar: sair do jogo?") e o segundo confirma; qualquer outra ação cancela. As mudanças valem na hora e ficam salvas.

Navegue com as setas, o direcional ou o analógico (segurar repete), com o mouse (clique, ou arraste a trilha do volume); Enter ou A confirmam. Para fechar: ESC, B, F12, Guide, L3+R3 ou "Voltar ao jogo". Ao voltar, o jogo continua de onde parou.

### Macros (auto clique)

Uma macro é uma gravação do que você apertou no jogo, repetida exatamente depois. Tudo pelo menu do jogo (F12):

- **Gravar nova macro:** o menu fecha e a gravação começa ("Gravando: abra o menu (F12) para parar"). Jogue o que quiser repetir; depois F12 > **Parar e salvar gravação**. A macro ganha o nome "Macro 1", "Macro 2"... Botões já apertados na hora em que a gravação começa (como o Enter que confirmou "Gravar") só entram depois de soltos. Gravar sem apertar nada não cria macro.
- **Macro:** escolhe qual macro usar (setas esquerda/direita, ou clique na metade esquerda/direita da linha). Mostra os passos e a duração.
- **Tocar macro:** o menu fecha e a macro toca no lugar dos seus botões. Apertar **qualquer botão novo** interrompe (botões que você já segurava ao começar só interrompem depois de soltos e apertados de novo). Para parar também: F12 > **Parar macro**. Funciona no avanço rápido.
- Sair do jogo (ESC) com uma gravação em andamento descarta a gravação e avisa na biblioteca.

As macros ficam por jogo, em texto, em `%APPDATA%\gba-frontend\macros\<nome do jogo>\<nome>.txt` (junto com `<nome>.state`, o estado salvo do começo da gravação). Dá para editar no Bloco de Notas:

```
nome=Farmar
modo=loop          # once (uma vez) | count (repetir N vezes) | loop (sem parar)
repetir=1          # 1 a 999, só vale em count
do_estado=0        # 1 = cada repetição recomeça do estado salvo da gravação
---
A+B 12             # botões (A B L R Start Select Cima Baixo Esquerda Direita) e a duração em quadros (60 por segundo)
- 30               # "-" = esperar
Cima 8
```

O jogo tem sorteios aleatórios; sem `do_estado=1` a repetição pode não sair idêntica. Com `do_estado=1` cada repetição começa do mesmo ponto em que a gravação começou (se o `.state` sumir, a macro para e avisa). Limites: 4096 passos e 36000 quadros (10 minutos) por macro. Um arquivo inválido é ignorado (o jogo avisa quantos) e não atrapalha as outras macros. Para criar, renomear, duplicar, apagar e editar os passos por uma tela, veja o editor abaixo.

### Editor de macros (Auto clique)

Configurações > botão **Auto clique** (canto inferior esquerdo). Edita as macros do **jogo selecionado na biblioteca** (sem jogo selecionado, a barra de status avisa). Tudo é **salvo na hora**.

- **Lista de macros** (esquerda): escolher (setas, clique, roda do mouse); **Novo** (Insert) cria "Macro N" com um passo; **Duplicar** (Ctrl+D) copia também o estado salvo; **Apagar** (Delete) pede um segundo acionamento para confirmar. Um ponto verde marca as macros que têm estado salvo.
- **Nome:** Enter ou clique para editar; Enter confirma, Esc cancela. Nome vazio ou que já existe (sem diferenciar maiúsculas) é recusado. Renomear leva o estado salvo junto.
- **Modo:** Uma vez, N vezes ou Repetir. **Repetições** (1 a 999) só valem em N vezes. **Começar do estado salvo** só liga se a macro tem estado salvo (grave de novo para criar).
- **Passos** (tabela): cada linha tem os 10 botões do GBA como chips (clique liga e desliga; ou as teclas **1 a 0** da fileira de cima: A, B, L, R, Start, Select, Cima, Baixo, Esquerda, Direita) e a duração em quadros e em milissegundos. A duração muda com **- / +**, com as setas esquerda/direita (Shift = 10 em 10) ou digitando (Enter ou duplo clique no valor, ou o teclado numérico; Enter confirma). **Adicionar** (Insert), **Duplicar** (Ctrl+D), **Subir/Descer** (Alt+setas) e **Remover** (Delete). Passos iguais seguidos continuam separados.
- **Tab** muda de área (lista, nome, modo, repetições, estado, passos, botões do passo). **Esc** ou **Voltar** fecha.
- Limites: 4096 passos e 36000 quadros por macro, duração de um passo de 1 a 36000 quadros; passar disso é recusado com aviso. "Testar" a macro é pelo menu do jogo (F12 > Tocar macro).

### Tela Controles

Escolha o botão do GBA, aperte Enter e depois a tecla do teclado ou o botão do controle desejado (ESC cancela). Se a tecla já era de outro botão, ela é movida para o novo (sem duplicatas). As teclas dos atalhos (Tab, ESC, F3 a F12) não podem ser atribuídas: aparece "Tecla reservada para atalho". Há "Restaurar padrão" e "Voltar". As alterações são salvas na hora. Esta tela é operada por teclado e controle (o mouse não clica nela).

A configuração fica em `%APPDATA%\gba-frontend\config.ini`, em texto simples (`pasta_roms=`, `volume=`, `mudo=`, `avanco=`, `avanco_sempre=`, `turbo=`, `turbo_taxa=`, `turbo_botoes=`, `analogico=`, `zona_morta=`, `escala=`, `tela_cheia=`, `tecla.A=`, `pad.A=` …). Arquivo ausente, valores inválidos ou linhas desconhecidas voltam ao padrão sem erro.

## Compilar

Requisitos: [MSYS2](https://www.msys2.org/) com o shell **MINGW64** e os pacotes:

```
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-SDL2
```

Depois, no shell MINGW64, dentro de `gba-frontend/`:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

O `gameboy front.exe` é compilado como aplicativo de **janela** (sem terminal junto). Para ver no terminal as mensagens de depuração do programa (caminho do save, erros do núcleo, controle conectado), configure com `-DGBA_CONSOLE=ON`.

O mGBA 0.10.5 vem em `third_party/mgba` (código-fonte da tag 0.10.5). O `CMakeLists.txt` define `CMAKE_POLICY_VERSION_MINIMUM=3.5` porque essa versão do mGBA ainda declara um CMake antigo. A fonte Inter vira o arquivo `build/inter_data.c` na primeira configuração (apague-o para regerar).

Ferramentas de desenvolvimento (não fazem parte do CTest): `build/snapshot.exe [--scale S] [--select N] [--search TEXTO] [--settings] [--keys] --out arq.bmp` grava a interface num BMP (em qualquer escala); `build/video_check.exe` confere o desenho real numa janela de verdade (cópia 1:1, mouse, tela cheia); `build/dpi_probe.exe a|b|c` mostra como o SDL e o Windows tratam a escala; `build/ui_bench.exe` mede o tempo de desenhar a biblioteca em 720p a 4K; `build/crop_compare.exe ESCALA GAMA PREFIXO` gera recortes ampliados do antes/depois (as imagens prontas estão em `docs/nitidez/`).

### Distribuição

Copie `build/gameboy front.exe` e `SDL2.dll` (de `C:\msys64\mingw64\bin`) para a mesma pasta, junto de `LICENSE-mGBA.txt` e `OFL.txt`. O executável não precisa de mais nada além de DLLs do Windows.

## Estrutura

```
src/main.c        laço principal; traduz eventos SDL; avanço rápido, tela cheia, sessão de jogo
src/game.c        núcleo mGBA + áudio de uma ROM; game_run_frames
src/video.c       janela SDL, escala sem deformar, tela cheia, OSD, barra de título própria
src/scale.c       retângulo de destino (preencher/inteira) e conversão do mouse
src/audio.c       fila de áudio SDL; volume; o áudio dita o ritmo do loop (1x)
src/volume.c      curva de volume e aplicação nas amostras
src/pacer.c       quadros devidos no avanço rápido
src/settings.c    volume, mudo, avanço rápido (e sempre ativo), turbo, analógico, escala, tela cheia
src/turbo.c       repetição dos botões segurados (por quadro emulado)
src/stick.c       analógico -> direcional (zona morta com histerese)
src/nav.c         direção segurada -> navegação com repetição (menu do jogo)
src/overlay.c     estado e eventos do menu do jogo (painel por cima do jogo pausado)
src/macro.c       macros: passos, gravador e tocador (lógica pura)
src/macro_store.c macros em arquivos de texto, uma pasta por jogo
src/editor.c      estado e eventos do editor de macros (tela "Auto clique")
src/hotkeys.c     atalhos do jogo e teclas reservadas
src/input.c       teclado e controle -> teclas do GBA (usa as associações)
src/bindings.c    botões do GBA, associações padrão e reatribuição
src/config.c      config.ini (pasta de ROMs, configurações e associações)
src/romscan.c     varredura de ROMs
src/library.c     biblioteca: filtro, formatação, library.ini, miniaturas
src/layout.c      retângulos de cada região da interface (título, ferramentas, tabela, painel) e hit-test
src/view.c        estado e eventos da biblioteca e do painel Configurações
src/menu.c        tela Controles (edição de teclas)
src/ui_draw.c     desenho da biblioteca, dos painéis e da barra de status
src/canvas.c      superfície de desenho em pixels reais: medidas lógicas x escala, bordas alinhadas à grade, cantos/círculos suavizados, miniaturas
src/ui_metrics.c  escala (0,75 a 4,0), faixas e conversão do mouse
src/gfx.c         preenchimento, linhas anti-aliased, mistura, contraste
src/ttf.c         texto com a fonte Inter (stb_truetype), no tamanho final em pixels
src/icons.c       ícones lineares (traços retos alinhados à grade)
src/textedit.c    edição do campo de busca (UTF-8)
src/utf8.c        decodificação UTF-8
src/folderpick.c  seletor de pastas do Windows
src/save.c        caminhos de .sav / .ss1
tests/            testes unitários (CTest)
tools/            snapshot, video_check, dpi_probe, ui_bench e crop_compare
```

## Limitações conhecidas

- Os testes `arm.gba` (teste 235) e `thumb.gba` (teste 230) do gba-tests falham no núcleo do mGBA 0.10.5; os 229 testes anteriores de cada um passam. São casos extremos de CPU e não afetam jogos comuns.
- Sem som durante o avanço rápido; não há velocidade abaixo de 1x.
- Gênero e ano dos jogos aparecem como "—" (não há fonte de metadados).
- Fora do escopo: cheats, gravação de vídeo, link cable, rewind, shaders, outras plataformas além do GBA.

## Licenças

- Este frontend usa o mGBA, licenciado sob a **Mozilla Public License 2.0**. O código-fonte do mGBA está em `third_party/mgba` e em https://github.com/mgba-emu/mgba. Se você modificar arquivos do mGBA, o código dessas modificações deve permanecer disponível sob a MPL 2.0, e o aviso de licença deve ser mantido. O texto da licença está em `third_party/mgba/LICENSE` (e em `LICENSE-mGBA.txt` na pasta `dist/`).
- A fonte de texto é a **Inter** (Copyright © 2016 The Inter Project Authors), licenciada sob a **SIL Open Font License 1.1** (`third_party/inter/OFL.txt`).
- O desenho de texto usa o **stb_truetype** de Sean Barrett, em **domínio público** (`third_party/stb/`).
