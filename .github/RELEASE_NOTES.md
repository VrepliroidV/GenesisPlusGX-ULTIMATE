**Genesis Plus GX ULTIMATE 1.2.2**: emulador de Mega Drive / Genesis, Mega CD, **32X**, Master System, Game Gear e SG-1000 para Wii, baseado no [Genesis Plus GX 1.7.5](https://github.com/ekeeke/Genesis-Plus-GX) (Eke-Eke). Os jogos de 32X rodam pelo [PicoDrive](https://github.com/irixxxx/picodrive) (notaz, irixxxx e outros). *English below.*

## Novidades da 1.2.2

- **Música do menu com um HD ou pendrive no USB**: as músicas agora vêm só da primeira pasta `genplusultimate/music` que tiver música: a do aparelho de onde o emulador abriu, senão a do SD, senão a do USB. Antes ele lia sempre as três, e a leitura de um HD dormindo ou lento no USB podia deixar o menu parado. Com as músicas no SD, o USB não é lido para isso. (A mesma correção do Snes9x GX ULTIMATE 1.1.6, onde a abertura às vezes ficava parada no fundo com o logo.)

## Novidades da 1.2.1

### Paprium

- O **Paprium** (WaterMelon) agora roda. O chip do cartucho (RAM, descompressão dos gráficos, sprites, escala, efeitos sonoros e músicas) é emulado com o código do [Genesis Plus GX PUNCHiUM](https://github.com/pav1388/Genesis-Plus-GX-PUNCHiUM) (pav1388, "Project Little Man"), com as correções da [versão de PS3](https://github.com/DeepDarkDaddy/Paprium-PS3-Genesis-Plus-GX) (DeepDarkDaddy) e ajustes para o Wii.
- **Músicas**: ficam numa pasta `paprium` ao lado da ROM, em MP3 (ou OGG), com os nomes das faixas, como no Genesis Plus GX PUNCHiUM (por exemplo `24 Intro.mp3`). Elas tocam direto do arquivo, aos poucos, sem carregar a faixa inteira na memória do Wii.
- O Paprium sempre roda como cartucho de Mega Drive, sem ligar o Mega CD, mesmo com o add-on em AUTO ou com um CD montado antes.
- Na primeira vez, sem save, o jogo mostra uma abertura falsa de 8 bits: é uma piada do próprio Paprium, não um defeito.

### Sega 32X

- **Cheats nos jogos de 32X**: o menu de cheats agora funciona neles, com os mesmos arquivos (`genplusultimate/cheats/32x/<jogo>.pat`) e o Auto Cheats. Valem códigos de ROM e de RAM. Um código `AAAAAA:DD` escreve na RAM do 32X (endereço abaixo de `040000`) ou na do 68k (`FFxxxx`), como os códigos "32X RAM" das listas de cheats; os "68K RAM" de 4 dígitos (`AAAA:DD`) valem como `FFAAAA:DD`.
- **32X Settings > 68k Auto Overclock**: o overclock do 68k só entra quando o jogo fica lento, até o valor de **68k Overclock** (+100% se estiver em OFF ao ligar). Com **Show FPS** aparece `68K +x% y%`: o overclock em uso e quanto do quadro o 68k trabalhou.
- **O som do 32X segue o menu Audio**: YM2612 Type (DISCRETE usa o YM2612 do PicoDrive, as outras opções o YM3438), High-Quality FM, os volumes de FM e PSG, o Filtering (passa-baixa ou equalizador) e o Mono. Com tudo desligado, o som sai limpo, sem filtro. Por isso **FM Chip**, **FM Filtering** e **Low-Pass Filter** saíram do 32X Settings.

### Z80 Auto Overclock

- **Options > System > Z80 Auto Overclock**: o overclock automático agora também vale para o Z80 dos jogos de **Master System, Game Gear e SG-1000**. Ele só entra quando o jogo fica lento, até o valor de **Z80 Overclock** (3.0x se estiver em OFF ao ligar). Com **Show FPS** aparece `Z80 1.4x 97%`: a velocidade em uso e quanto do quadro o Z80 trabalhou.
- Nos jogos de Mega Drive, o Z80 toca o som e fica na velocidade normal.

### CD na lista de jogos de Sega CD

- Na lista de jogos de Sega CD, a screenshot aparece **redonda, como a etiqueta de um CD**, dentro de uma moldura de disco (antes era a moldura de cartucho do Mega Drive). Sem screenshot, aparece um CD prateado. As screenshots continuam sendo os mesmos PNG de `genplusultimate/snaps/cd`.

### Retroflag e outros controles USB de 6 botões

- O **C** agora é só o C, e o **L** e o **R** são botões próprios. No jogo eles funcionam como **Mode**, a menos que você coloque outra tecla neles em **Keys Config** (agora dá). Eles não abrem mais o menu: o atalho continua sendo o botão **Mode + direcional para a direita**.
- **Controle sem fio que desliga sozinho**: quando o controle desliga para poupar bateria e é ligado de novo, ele volta a funcionar na hora, sem precisar tirar e recolocar o receptor. Se o Wii recusar abrir o receptor num momento, o emulador tenta de novo alguns segundos depois.

### Correção: Per-Game

- As configurações que você muda **antes** de apertar **Per-Game** agora ficam só para aquele jogo. Antes elas também viravam as configurações globais, e os outros jogos abriam com elas.
- Se isso aconteceu com você, abra uma vez um jogo sem Per-Game e acerte as configurações globais do jeito que você quer. Os arquivos de cada jogo (`genplusultimate/saves/<sistema>/<jogo>.cfg`) estão certos e não precisam ser apagados.

## Versões anteriores

- **1.2.0**: jogos de **32X** pelo PicoDrive (Load Game > SEGA 32X, saves em `genplusultimate/saves/32x`, BIOS opcional), **32X Settings** (Dynamic Recompiler, Renderer, Frame Skip, velocidade e **SH-2 Auto Overclock**, 68k Overclock, Idle Loop Detection, PWM Sound, PWM IRQ Optimization, No Sprite Limit), Auto Crop de bordas de qualquer cor em todos os sistemas, C confirma e B volta nos menus com o Retroflag, ícones novos em Load Game.
- **1.1.0 a 1.1.2**: **música no menu** (arquivos `.ogg` em `genplusultimate/music`, Options > Menu > Menu Music) com mini player.
- **1.0.0**: **molduras em PNG** em 480i/480p, **overclock automático** da CPU principal e da do Sega CD, **configurações por jogo** (Per-Game), **Auto Crop**, e **controles USB do tipo Xbox 360 (XInput)**, com fio ou com receptor 2.4 GHz, vistos como controle de GameCube (até 4, pode plugar e tirar com o emulador aberto).

Botões já configurados, pensados para o Retroflag de 6 botões:

| Retroflag | Mega Drive |
|---|---|
| A, B, C | A, B, C |
| X, Y, Z | X, Y, Z |
| L e R | Mode (ou a tecla que você puser neles em Keys Config) |
| Mode | Mode |
| Start | Start |
| **Mode + direcional para a direita** | abre o menu do emulador |

Nos menus, o C confirma e o B volta. Em outros controles, os gatilhos e o botão de ombro esquerdo dão C, o ombro direito dá Z e Select dá Mode.

## Instalação

1. Baixe o `GenesisPlusGXUltimate-Wii.zip` e extraia na raiz do cartão SD ou do pendrive. Ele cria `apps/genplusultimate` (o emulador) e a pasta `genplusultimate` com `roms`, `bezels` e `music`. Quem já tem uma versão anterior pode extrair por cima: as configurações e os saves continuam. As outras pastas (saves, snaps, cheats, bios) são criadas quando o emulador abre.
2. É um app separado: pode ficar instalado junto com o Genesis Plus GX, e não mexe na pasta `genplus` dele. Para usar seus saves e BIOS antigos, copie as pastas `saves`, `bios` e `lock-on` de `genplus` para `genplusultimate`.

## Observações

- Plugue o receptor dos controles USB **direto no USB do Wii**. Ligado num adaptador na entrada do Wiimote, ele não funciona e pode travar o Wii.
- Funciona junto com um HD externo plugado na outra porta USB.
- O jogo e as músicas do Paprium não vêm com o emulador.

## Créditos

- Genesis Plus GX ULTIMATE: VrepliroidV
- Genesis Plus GX: Eke-Eke (e Charles MacDonald, autor do Genesis Plus original); a lista completa está em View Credits, no menu de saída do emulador.
- PicoDrive (emulação do 32X): notaz, irixxxx, fDave e outros; a lista completa está em `PICODRIVE_AUTHORS.txt`, junto com o emulador.
- Chip do Paprium: Genesis Plus GX PUNCHiUM, de pav1388 ("Project Little Man"), com as correções da versão de PS3 de DeepDarkDaddy.

---

## English

**Genesis Plus GX ULTIMATE 1.2.2**: Mega Drive / Genesis, Mega CD, **32X**, Master System, Game Gear and SG-1000 emulator for Wii, based on Genesis Plus GX 1.7.5 (Eke-Eke). The 32X games run on [PicoDrive](https://github.com/irixxxx/picodrive) (notaz, irixxxx and others).

New in 1.2.2:

- **Menu music with a USB hard drive or pen drive plugged in**: the music now comes from the first `genplusultimate/music` folder that has music only: the one of the device the emulator started from, else the SD card's, else the USB drive's. All three used to be read, and the reading of an asleep or slow USB drive could hold the menu. With the music on the SD card, the USB drive is not read for it. (The same fix as in Snes9x GX ULTIMATE 1.1.6, whose start-up could stay on the background with the logo.)

New in 1.2.1:

- **Paprium** (WaterMelon) runs. Its cartridge chip (RAM, graphics decompression, sprites, scaler, sound effects and music) is emulated with the code of [Genesis Plus GX PUNCHiUM](https://github.com/pav1388/Genesis-Plus-GX-PUNCHiUM) (pav1388, "Project Little Man"), with the fixes of the [PS3 port](https://github.com/DeepDarkDaddy/Paprium-PS3-Genesis-Plus-GX) (DeepDarkDaddy) and changes for the Wii. The music is read from MP3 (or OGG) files in a `paprium` folder next to the ROM, named after the tracks as in Genesis Plus GX PUNCHiUM (e.g. `24 Intro.mp3`), streamed from the file. It always runs as a Mega Drive cartridge, never with the Mega CD. Without a save, the game first shows a fake 8-bit intro: that is the game's own joke.
- **32X**: the **cheat menu** works on 32X games (`genplusultimate/cheats/32x/<game>.pat`, Auto Cheats, ROM and RAM codes; `AAAAAA:DD` writes to the 32X RAM below `040000` or to the 68k RAM at `FFxxxx`, and 4-digit "68K RAM" codes are read as `FFAAAA:DD`). **32X Settings > 68k Auto Overclock**: the 68k overclock only while the game slows down, up to 68k Overclock (+100% when turned on without one); Show FPS shows `68K +x% y%`. The 32X FM and PSG sound **follows the Audio menu** (YM2612 Type, High-Quality FM, FM and PSG volume, Filtering, Mono), so FM Chip, FM Filtering and Low-Pass Filter left 32X Settings.
- **Options > System > Z80 Auto Overclock**: the automatic overclock for the Z80 of Master System, Game Gear and SG-1000 games, up to Z80 Overclock (3.0x when turned on without one); Show FPS shows `Z80 1.4x 97%`. The Z80 of Mega Drive games plays the sound and keeps its speed.
- **Sega CD game list**: the screenshot is shown round, as the label of a CD, in a disc frame (a blank silver disc without one). The screenshots are the same PNG files in `genplusultimate/snaps/cd`.
- **Retroflag and other 6-button USB pads**: C is only C; L and R are buttons of their own, Mode in the game unless Keys Config puts another key on them, and they no longer open the menu (that stays the Mode button + Right).
- **Wireless USB pads that turn themselves off** work again as soon as they are turned back on, without plugging the receiver in again. A receiver that the Wii does not let the emulator open is tried again a few seconds later.
- **Fixed: Per-Game**. Settings changed before pressing Per-Game now stay with that game only; they used to become the global settings too, so every other game got them. If that happened to you, load a game without Per-Game once and set the global settings back.

Earlier: Sega 32X games on PicoDrive with 32X Settings and SH-2 Auto Overclock, Auto Crop on all systems, C and B in the menus on the Retroflag (1.2.0); menu music with a mini player (1.1.0 to 1.1.2); PNG bezels in 480i/480p, automatic overclock of the main and Sega CD 68k, per-game settings, Auto Crop, and Xbox 360 (XInput) USB controllers and 2.4 GHz receivers read as GameCube controllers, with a layout for the 6-button Retroflag where **Mode + D-pad right** opens the menu (1.0.0).

Install: extract `GenesisPlusGXUltimate-Wii.zip` to the root of the SD card or USB drive (over an earlier version too: settings and saves stay). It is a separate app (`apps/genplusultimate`) with its own data folder (`genplusultimate`), so it can sit next to Genesis Plus GX; copy `saves`, `bios` and `lock-on` from `genplus` if you want to keep them. The Paprium game and its music are not included.

---

Genesis Plus GX is © Charles MacDonald and Eke-Eke, and PicoDrive is © notaz and the other PicoDrive authors. Both may be used and shared for free, but not for commercial purposes; see LICENSE.txt and PICODRIVE_LICENSE.txt, which are also in the .zip. The Paprium chip code comes from Genesis Plus GX PUNCHiUM (pav1388), under the same license as Genesis Plus GX. The source code of this build, PicoDrive's included, is in the "Source code" files of this release.
