	.include "iocscall.mac"
    .section .text
	.align	2

# ----- AUX 初期化: 9600bps, 8N1, XON/OFFなし -----
# _SET232C (#0x30)
# d1.w = 設定値
#   bit15-14: Stop(1=1bit)
#   bit13-12: Parity(0=なし)
#   bit11-10: DataLen(3=8bit)
#   bit9    : XON/OFF (0=OFF)
#   bit8    : SI/SO   (0=OFF)
#   bit7-0  : BPS (7=9600)
# → 0b 01 00 11 0 0 0x07 = 0x4C07
    .globl aux_init
aux_init:
    movem.l d1,-(sp)

    # stop=1bit, parity=none, 8bit, 9600bps
    move.w  #0x0C07, d1
    IOCS    _SET232C
    IOCS    _LOF232C
    movem.l (sp)+, d1
    rts

# ----- 1バイト送信 -----
# _OUT232C (#0x35)
    .globl aux_putc
aux_putc:
    movem.l d1,-(sp)
    # 引数を取り出す（intに拡張されている）
    move.l  8(sp), d1
    # 下位8bitだけ送る
    and.b   #0xFF, d1
    IOCS    _OUT232C
    movem.l (sp)+, d1
    rts

# ----- 文字列送信（ASCIIZ） -----
# a0 = 文字列先頭
    .globl aux_puts
aux_puts:
    movem.l d1/a0,-(sp)
    # 第1引数のポインタを取り出す
    move.l  12(sp), a0
.next:
    move.b  (a0)+, d1
    beq.s   .done
    IOCS    _OUT232C
    bra.s   .next
.done:
    movem.l (sp)+, d1/a0
    rts