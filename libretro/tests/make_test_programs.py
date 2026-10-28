#!/usr/bin/env python3
"""Writes the test programs of libretro_test.c into a folder:

test.md: a white picture on a black backdrop (plane A full of tile 1); each
frame VDP register 1 = $FF0006, a SUBQ/BNE loop $FF0002 turns long, wait for
VINT ($FF0000 set by it), and one finished frame counted in $FF000C.

test-blue.md: the same picture on a blue backdrop (colour 0), for the borders
in the backdrop colour that Auto Crop removes as well.

test.sms and test.gg: a white picture (name table full of tile 1) on a blue
backdrop, Mode 4, display on, the leftmost 8 pixels blanked (register 0 bit
5); then a loop.

bios_CD_U.bin and test-cd.iso: a fake Mega CD. The main 68k copies a sub
program into PRG-RAM and starts the sub 68k; at each VINT it passes the work
count from $FF0002 in comm command 0, copies comm status 0 (frames the sub
68k finished) to $FF000E and sends the level 2 interrupt. The sub 68k runs a
SUBQ/BNE loop that many turns, waits for the interrupt, and counts.

Copyright (c) 2026 VrepliroidV, under the same terms as Genesis Plus GX.
"""
import sys, struct, os
out = sys.argv[1]

# --- test.md ---
rom = bytearray(0x20000)
code = []
def w(h): code.append(bytes.fromhex(h.replace(" ", "")))
w("46FC2700")                                  # move.w #$2700,sr
w("41F900C00000 43F900C00004")                 # lea data port,a0 ; lea control port,a1
for r in ("8004", "8174", "8230", "8C81", "8F02"): # mode, display+VINT, plane A at $C000, H40, autoinc 2
    w("32BC" + r)                              # move.w #r,(a1)
w("22BC40200000")                              # move.l #VRAM write $0020,(a1)
w("303C000F 30BC1111 51C8FFFA")                # 16 words of $1111 (tile 1, colour 1)
w("22BC40000003")                              # VRAM write $C000
w("303C07FF 30BC0001 51C8FFFA")                # 2048 cells of tile 1
w("22BCC0000000")                              # CRAM write 0
w("30BC0000 30BC0EEE")                         # colour 0 black, colour 1 white
w("46FC2000")                                  # move.w #$2000,sr
init = b"".join(code)
main = 0x200 + len(init)
code = []
w("303900FF0006 00408100 3280")                # move.w ($FF0006),d0 ; ori #$8100 ; move.w d0,(a1)
w("343900FF0002")                              # move.w ($FF0002),d2
w("5342 66FC")                                 # subq #1,d2 ; bne
w("4A3900FF0000 67F8")                         # tst.b flag ; beq
w("423900FF0000")                              # clr.b flag
w("527900FF000C")                              # addq.w #1,($FF000C)
body = b"".join(code)
disp = main - (main + len(body) + 2)
body += bytes([0x60, disp & 0xff])             # bra.s main
vint = main + len(body)
handler = bytes.fromhex("13FC000100FF0000 4E73 4E73".replace(" ", ""))
rom[0x200:0x200 + len(init)] = init
rom[main:main + len(body)] = body
rom[vint:vint + len(handler)] = handler
struct.pack_into(">I", rom, 0, 0x00FFFE00)
struct.pack_into(">I", rom, 4, 0x00000200)
for v in range(2, 64):
    struct.pack_into(">I", rom, v * 4, vint + 8)
struct.pack_into(">I", rom, 0x78, vint)
rom[0x100:0x110] = b"SEGA MEGA DRIVE "
rom[0x120:0x150] = b"AUTOOC TEST".ljust(48, b" ")
rom[0x150:0x180] = b"AUTOOC TEST".ljust(48, b" ")
rom[0x180:0x18E] = b"GM 00000000-00"
struct.pack_into(">II", rom, 0x1A0, 0, len(rom) - 1)
struct.pack_into(">II", rom, 0x1A8, 0xFF0000, 0xFFFFFF)
rom[0x1F0:0x1F3] = b"JUE"
open(os.path.join(out, "test.md"), "wb").write(rom)
blue = rom.replace(bytes.fromhex("30BC0000 30BC0EEE".replace(" ", "")), bytes.fromhex("30BC0E00 30BC0EEE".replace(" ", "")))
assert blue != rom
open(os.path.join(out, "test-blue.md"), "wb").write(blue)

# --- test.sms, test.gg ---
def z80_program(gg):
    c = bytearray()
    def b(h): c.extend(bytes.fromhex(h.replace(" ", "")))
    def out_ctrl(lo, hi): b("3E%02X D3BF 3E%02X D3BF" % (lo, hi))   # ld a,lo ; out ($BF),a ; ld a,hi ; out ($BF),a
    b("F3 ED56 3100DF")                            # di ; im 1 ; ld sp,$DF00
    # registers: 0 = Mode 4 + left column blank, 1 = display on, name table $3800,
    # sprite table $3F00, backdrop = sprite colour 0
    for reg, value in enumerate((0x24, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB, 0x00, 0x00, 0x00, 0xFF)):
        out_ctrl(value, 0x80 | reg)
    out_ctrl(0x20, 0x40)                           # VRAM $0020: tile 1, colour 1
    b("0608 3EFF D3BE AF D3BE D3BE D3BE 10F3")     # 8 rows: $FF,0,0,0 (djnz back 13 bytes)
    out_ctrl(0x00, 0x7F)                           # VRAM $3F00: no sprites
    b("3ED0 D3BE")
    out_ctrl(0x00, 0x78)                           # VRAM $3800: 32x28 entries of tile 1
    b("018003 3E01 D3BE AF D3BE 0B 78 B1 20F4")  # (jr nz back 12 bytes)
    out_ctrl(0x00, 0xC0)                           # CRAM 0
    if gg:
        # 12-bit colours, 2 bytes: 0 black, 1 white, 16 blue
        b("AF D3BE D3BE 3EFF D3BE 3E0F D3BE")
        b("061C AF D3BE 10FB")                     # colours 2 to 15
        b("AF D3BE 3E0F D3BE")
    else:
        # 6-bit colours: 0 black, 1 white, 16 blue
        b("AF D3BE 3E3F D3BE")
        b("060E AF D3BE 10FB")
        b("3E30 D3BE")
    b("18FE")                                      # jr $
    rom = bytearray(0x8000)
    rom[0:len(c)] = c
    rom[0x7FF0:0x7FF8] = b"TMR SEGA"
    return rom
open(os.path.join(out, "test.sms"), "wb").write(z80_program(False))
open(os.path.join(out, "test.gg"), "wb").write(z80_program(True))

# --- fake Mega CD ---
def put(buf, addr, hexs):
    b = bytes.fromhex(hexs.replace(" ", ""))
    buf[addr:addr + len(b)] = b
    return addr + len(b)
bios = bytearray(0x20000)
a = put(bios, 0x200, "46FC2700 33FC800400C00004 33FC817400C00004 23FCC000000000C00004 33FC0EEE00C00000"
                     " 13FC000200A12001 0839000100A12001 67F6"
                     " 41F900001000 43F900020000 303C00FF 22D8 51C8FFFC"
                     " 13FC000100A12001 46FC2000")
main = a
a = put(bios, a, "4A3900FF0000 67F8 423900FF0000 60F0")
vint = a
a = put(bios, a, "13FC000100FF0000 33F900FF000200A12010 33F900A1202000FF000E 08F9000000A12000 4E73")
rte = a
put(bios, a, "4E73")
assert main == 0x25A and vint == 0x26A
struct.pack_into(">I", bios, 0, 0x00FFFE00)
struct.pack_into(">I", bios, 4, 0x00000200)
for v in range(2, 64):
    struct.pack_into(">I", bios, v * 4, rte)
struct.pack_into(">I", bios, 0x78, vint)
bios[0x100:0x110] = b"SEGA MEGA DRIVE "
sub = bytearray(0x400)
a = put(sub, 0x200, "46FC2700 33FC000400FF8032 46FC2000")
assert a == 0x210
a = put(sub, a, "343900FF8010 5342 66FC 4A3900001000 67F8 423900001000 527900FF8020 60E0")
assert a == 0x230
a = put(sub, a, "13FC000100001000 4E73")
srte = a
put(sub, a, "4E73")
struct.pack_into(">I", sub, 0, 0x00070000)
struct.pack_into(">I", sub, 4, 0x00000200)
for v in range(2, 64):
    struct.pack_into(">I", sub, v * 4, srte)
struct.pack_into(">I", sub, 0x68, 0x230)
bios[0x1000:0x1400] = sub
open(os.path.join(out, "bios_CD_U.bin"), "wb").write(bios)
iso = bytearray(2048 * 16)
iso[0:16] = b"SEGADISCSYSTEM  "
iso[0x100:0x110] = b"SEGA GENESIS    "
open(os.path.join(out, "test-cd.iso"), "wb").write(iso)
