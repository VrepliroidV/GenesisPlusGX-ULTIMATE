**Genesis Plus GX ULTIMATE 1.2.0**: emulador de Mega Drive / Genesis, Mega CD, **32X**, Master System, Game Gear e SG-1000 para Wii, baseado no [Genesis Plus GX 1.7.5](https://github.com/ekeeke/Genesis-Plus-GX) (Eke-Eke). Os jogos de 32X rodam pelo [PicoDrive](https://github.com/irixxxx/picodrive) (notaz, irixxxx e outros). *English below.*

## Novidades da 1.2.0

### Sega 32X

- O emulador agora roda jogos de **32X**. A emulação do 32X é do PicoDrive, que vem dentro do emulador. O resto é o mesmo do Genesis Plus GX ULTIMATE: menus, controles, molduras, Auto Crop, filtros de imagem, save states, saves de bateria e configurações por jogo.
- **Load Game > SEGA 32X** (o primeiro ícone, no lugar do Recent) abre a lista de jogos de 32X (arquivos `.32x`), começando em `genplusultimate/roms`. Na lista, a screenshot do jogo (pasta `genplusultimate/snaps/32x`) aparece dentro de uma moldura de cartucho de 32X.
- Os saves dos jogos de 32X ficam em `genplusultimate/saves/32x`.
- **BIOS (opcional)**: sem BIOS, o PicoDrive usa a dele e os jogos rodam normalmente. Para usar as originais, coloque `32X_G_BIOS.BIN`, `32X_M_BIOS.BIN` e `32X_S_BIOS.BIN` em `genplusultimate/bios`, ou escolha os arquivos em **System ROM Paths** (32X 68K, M-SH2 e S-SH2 BIOS).
- Os cheats não funcionam nos jogos de 32X.
- Ao sair de um jogo de 32X, a memória que ele usava (uns 11 MB) é liberada na hora.

### 32X Settings

Em **Options > System > 32X Settings** ficam as opções do PicoDrive. Elas também entram nas configurações por jogo.

| Opção | O que faz |
|---|---|
| **Dynamic Recompiler** | ON (padrão). OFF é muito mais lento: só para procurar defeitos. |
| **Renderer** | Camada do Mega Drive: GOOD (padrão), ACCURATE (mais lento) ou FAST. |
| **Frame Skip** | OFF (padrão), AUTO ou 1 a 4. **AUTO** só deixa de desenhar um quadro quando ele atrasaria o jogo ou faria o som engasgar; **1 a 4** pulam sempre esse número de quadros depois de cada um desenhado. O jogo e o som continuam normais: só a imagem daquele quadro não é desenhada. |
| **Master / Slave SH-2 Speed** | Velocidade dos dois processadores do 32X, de 25% a 300%. |
| **SH-2 Auto Overclock** | O overclock dos SH-2 só entra quando o jogo fica lento, até as velocidades de cima (se estiverem em 100%, vão para 200%), e nunca passa do que o Wii consegue. Com **Show FPS**, aparece `SH-2 x% y%`: a velocidade em uso e quanto do quadro os SH-2 trabalharam. |
| **68k Overclock** | OFF ou de +25% a +400%. |
| **Idle Loop Detection** | ON (padrão). Desligue se um jogo travar (vale depois de um reset). |
| **PWM Sound** | O chip de som do 32X (ON). OFF é mais rápido, mas sem o som do 32X. |
| **PWM IRQ Optimization** | Mais rápido, mas pode estragar o som do 32X. |
| **No Sprite Limit** | Menos sprites piscando, mas alguns efeitos podem quebrar. |
| **FM Chip**, **FM Filtering**, **Low-Pass Filter** | Som FM do Mega Drive modelo 1 (YM2612) ou 2 (YM3438), filtro de FM e filtro passa-baixa. |

### Auto Crop

- Agora corta as bordas de qualquer cor, em todos os sistemas. Nos jogos de Master System, Game Gear, SG-1000 e Mega CD, as bordas são pintadas na cor de fundo do jogo e antes não eram cortadas.
- Também corta a coluna vazia de 8 pixels que alguns jogos deixam à esquerda da tela.
- Funciona também nos jogos de 32X.

### Retroflag nos menus

- Com o **Retroflag Classic Controller-M** (ou outro controle USB de 6 botões cujos gatilhos são botões), o **C** confirma e o **B** volta nos menus, como num Mega Drive. O A dele não faz nada nos menus. Dentro do jogo, os botões continuam os mesmos.
- Os botões B e C do Mega Drive aparecem ao lado dos botões do Wiimote: embaixo dos menus e nas janelas de confirmação. Sem o Retroflag conectado, os menus ficam como antes.
- O controle é reconhecido na primeira vez que o C é apertado (esse aperto já confirma).

### Menu

- Ícones novos em **Load Game**: **SEGA 32X** (cartucho de 32X) e **SEGA CD**, com as logos originais.

## Versões anteriores

- **1.1.2**: logo com o **ULTIMATE** um pouco mais baixo.
- **1.1.1**: a música e o mini player ficam só no menu antes de carregar um jogo (no menu do jogo fica em silêncio; **Load Game** toca de novo); a pasta de músicas é procurada no SD e no pendrive; músicas de até 16 MB.
- **1.1.0**: **música no menu**. Coloque arquivos `.ogg` em `genplusultimate/music` (SD ou pendrive) e escolha em **Options > Menu > Menu Music**: OFF, RANDOM (padrão) ou uma música pelo nome. Mini player no alto da tela, com o ponteiro do Wiimote.
- **1.0.0**:
  - **Molduras em PNG** (Options > Video > **Bezel**) nos modos INTERLACED (480i) e PROGRESSIVE (480p): OFF, DEFAULT (moldura de TV, o padrão), AUTO (`<nome do jogo>.png`, senão `default.png`) ou um PNG da pasta `genplusultimate/bezels`. PNG de qualquer tamanho, como os de 1920x1080 do RetroArch.
  - **Overclock automático** da CPU principal e da do Sega CD (Options > System), **configurações por jogo** (botão **Per-Game**), **Auto Crop** (Video > Auto Crop: FIT ou STRETCH) e **Crop Scaling**.
  - **Controles USB do tipo Xbox 360 (XInput)**, com fio ou com receptor 2.4 GHz plugado no USB do Wii, vistos como controle de GameCube (escolha GameCube na porta do jogador). Testado com o Retroflag Classic Controller-M e com um controle sem fio genérico de SNES Classic Mini. Até 4 controles; pode plugar e tirar com o emulador aberto.

Botões já configurados, pensados para o Retroflag de 6 botões:

| Retroflag | Mega Drive |
|---|---|
| A, B, C | A, B, C |
| X, Y, Z | X, Y, Z |
| L e R | C |
| Mode | Mode |
| Start | Start |
| **Mode + direcional para a direita** | abre o menu do emulador |

Em outros controles, os gatilhos e o botão de ombro esquerdo dão C, o ombro direito dá Z e Select dá Mode.

## Instalação

1. Baixe o `GenesisPlusGXUltimate-Wii.zip` e extraia na raiz do cartão SD ou do pendrive. Ele cria `apps/genplusultimate` (o emulador) e a pasta `genplusultimate` com `roms`, `bezels` e `music`. Quem já tem uma versão anterior pode extrair por cima: as configurações e os saves continuam. As outras pastas (saves, snaps, cheats, bios) são criadas quando o emulador abre.
2. É um app separado: pode ficar instalado junto com o Genesis Plus GX, e não mexe na pasta `genplus` dele. Para usar seus saves e BIOS antigos, copie as pastas `saves`, `bios` e `lock-on` de `genplus` para `genplusultimate`.

## Observações

- Plugue o receptor dos controles USB **direto no USB do Wii**. Ligado num adaptador na entrada do Wiimote, ele não funciona e pode travar o Wii.
- Funciona junto com um HD externo plugado na outra porta USB.

## Créditos

- Genesis Plus GX ULTIMATE: VrepliroidV
- Genesis Plus GX: Eke-Eke (e Charles MacDonald, autor do Genesis Plus original); a lista completa está em View Credits, no menu de saída do emulador.
- PicoDrive (emulação do 32X): notaz, irixxxx, fDave e outros; a lista completa está em `PICODRIVE_AUTHORS.txt`, junto com o emulador.

---

## English

**Genesis Plus GX ULTIMATE 1.2.0**: Mega Drive / Genesis, Mega CD, **32X**, Master System, Game Gear and SG-1000 emulator for Wii, based on Genesis Plus GX 1.7.5 (Eke-Eke). The 32X games run on [PicoDrive](https://github.com/irixxxx/picodrive) (notaz, irixxxx and others).

New in 1.2.0:

- **Sega 32X games**, emulated by PicoDrive, built into the emulator. Menus, controllers, bezels, Auto Crop, video filters, save states, backup RAM and per-game settings are the ones of Genesis Plus GX ULTIMATE.
  - **Load Game > SEGA 32X** (the first icon, in place of Recent) lists the 32X games (`.32x` files), starting in `genplusultimate/roms`; the game's screenshot (`genplusultimate/snaps/32x`) is shown in a 32X cartridge frame. Saves go to `genplusultimate/saves/32x`.
  - The 32X BIOS files are optional (PicoDrive has its own): `32X_G_BIOS.BIN`, `32X_M_BIOS.BIN` and `32X_S_BIOS.BIN` in `genplusultimate/bios`, or chosen in System ROM Paths.
  - Cheats are not available for 32X games. The memory of a 32X game (about 11 MB) is freed as soon as it is left.
- **Options > System > 32X Settings** (also per game): Dynamic Recompiler, Renderer (GOOD, ACCURATE, FAST), **Frame Skip** (OFF, AUTO, 1 to 4: AUTO skips drawing a frame only when it would slow the game down or make the sound stutter), Master and Slave SH-2 Speed (25% to 300%), **SH-2 Auto Overclock** (only while the game slows down, never more than the Wii can do; Show FPS shows `SH-2 x% y%`), 68k Overclock, Idle Loop Detection, PWM Sound, PWM IRQ Optimization, No Sprite Limit, FM Chip, FM Filtering and Low-Pass Filter.
- **Auto Crop** removes borders of any colour on all systems (the Master System, Game Gear, SG-1000 and Mega CD games paint them in their background colour) and the blank 8-pixel column some games leave on the left. It works on 32X games too.
- **Retroflag in the menus**: on the Retroflag Classic Controller-M (and other 6-button USB pads whose triggers are buttons), C confirms and B goes back in the menus, as on a Mega Drive (its A does nothing there; the game keeps its own mapping). Its B and C are shown next to the Wii remote buttons at the bottom of the menus and in the confirmation boxes. The pad is recognised the first time C is pressed (that press already confirms).
- New **SEGA 32X** and **SEGA CD** icons in Load Game, with the original logos.

Earlier: menu music from `genplusultimate/music` with a mini player (1.1.0 to 1.1.2); PNG bezels in 480i/480p, automatic overclock of the main and Sega CD 68k, per-game settings, Auto Crop, and Xbox 360 (XInput) USB controllers and 2.4 GHz receivers read as GameCube controllers, with a layout for the 6-button Retroflag where **Mode + D-pad right** opens the menu (1.0.0).

Install: extract `GenesisPlusGXUltimate-Wii.zip` to the root of the SD card or USB drive (over an earlier version too: settings and saves stay). It is a separate app (`apps/genplusultimate`) with its own data folder (`genplusultimate`), so it can sit next to Genesis Plus GX; copy `saves`, `bios` and `lock-on` from `genplus` if you want to keep them.

---

Genesis Plus GX is © Charles MacDonald and Eke-Eke, and PicoDrive is © notaz and the other PicoDrive authors. Both may be used and shared for free, but not for commercial purposes; see LICENSE.txt and PICODRIVE_LICENSE.txt, which are also in the .zip. The source code of this build, PicoDrive's included, is in the "Source code" files of this release.
