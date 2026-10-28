# 32X Test

Um teste do Sega 32X no Wii, feito com o núcleo libretro do
[PicoDrive](https://github.com/irixxxx/picodrive) (notaz, irixxxx e outros).
Serve para ver a que velocidade os jogos de 32X rodam no Wii antes de levar o
32X para o Genesis Plus GX ULTIMATE.

A Sega 32X speed test for the Wii, built on the PicoDrive libretro core.

## Instalação

Extraia o ZIP na raiz do SD (ou do pendrive). Ele cria:

- `apps/32xtest`: o app do Homebrew Channel
- `32xtest/roms`: coloque aqui os jogos (`.32x`; também abre `.bin`, `.md`, `.gen`, `.smd` de Mega Drive)
- `32xtest/bios`: opcional, os arquivos `32X_G_BIOS.BIN`, `32X_M_BIOS.BIN` e `32X_S_BIOS.BIN`

## Menu

- **SH-2 CPUs**: *Recompiler (DRC)*, o recompilador do PicoDrive para PowerPC, ou *Interpreter*. Se o jogo travar com o recompilador, teste o interpretador.
- **32X BIOS**: *Built-in*, a BIOS que o PicoDrive monta sozinho (não precisa de arquivos), ou *Files*, os arquivos da pasta `bios`. A tela mostra quais foram achados.
- **Renderer**: Accurate, Good ou Fast.
- **Frameskip**: Off ou Auto (pula quadros quando o som começa a faltar).
- **Sound**, **Speed limit** (Off: roda o mais rápido que conseguir, para medir), **Show speed**, **Pad** (3 ou 6 botões), **TV** (4:3 ou 16:9).

A velocidade aparece no canto da tela: quadros por segundo, a porcentagem da
velocidade normal e DRC ou INT. O app grava as mensagens do núcleo em
`32xtest/log.txt`.

No jogo, o menu abre com **HOME** (Wii Remote, Classic), **Z + direita**
(GameCube) ou **Mode + direita** (controle USB, como o Retroflag).

## Controles

| Mega Drive | Wii Remote (deitado) | Classic | GameCube | USB (XInput) |
|---|---|---|---|---|
| A | 1 | Y | B | A |
| B | 2 | B | A | B |
| C | A | A | X | C (gatilhos, LB) |
| X | - | ZL / L | L | X |
| Y | B | X | Y | Y |
| Z | - | ZR / R | R | Z (RB) |
| Mode | − (menos) | − (menos) | Z | Select |
| Start | + | + | Start | Start |

## Licença

O PicoDrive é distribuído com a licença do arquivo `PICODRIVE_LICENSE.txt`
(uso não comercial; versões modificadas devem vir com o código-fonte). Este
front end segue a mesma licença. O código-fonte está neste repositório
(branch `32x-test`), e o do PicoDrive no endereço acima, no commit indicado em
`.github/workflows/build.yml`.
