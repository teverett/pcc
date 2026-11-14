#include "ti994a.h"

#define VDP_READ_DATA 34816u    /* $8800 */
#define VDP_READ_STATUS 34818u  /* $8802 */
#define VDP_WRITE_DATA 35840u   /* $8C00 */
#define VDP_WRITE_ADDR 35842u   /* $8C02 */

void ti_vdp_write_data(unsigned char value) {
    *(unsigned char *)VDP_WRITE_DATA = value;
}

unsigned char ti_vdp_read_data() {
    return *(unsigned char *)VDP_READ_DATA;
}

unsigned char ti_vdp_read_status() {
    return *(unsigned char *)VDP_READ_STATUS;
}

void ti_vdp_write_addr(unsigned char value) {
    *(unsigned char *)VDP_WRITE_ADDR = value;
}

void ti_vdp_write_reg(unsigned char reg, unsigned char value) {
    ti_vdp_write_addr(value);
    ti_vdp_write_addr((reg & 7) | 128);
}

void ti_vdp_set_write_address(unsigned int addr) {
    ti_vdp_write_addr(addr & 255);
    ti_vdp_write_addr(((addr >> 8) & 63) | 64);
}

void ti_vdp_set_read_address(unsigned int addr) {
    ti_vdp_write_addr(addr & 255);
    ti_vdp_write_addr((addr >> 8) & 63);
}

void ti_vdp_init_text_mode() {
    ti_vdp_write_reg(0, 0);    /* graphics mode I, no external video */
    ti_vdp_write_reg(1, 192);  /* 16K VRAM, enable display, no interrupt, 8x8 sprites */
    ti_vdp_write_reg(2, 3);    /* name table at $1800 (3 * $800) */
    ti_vdp_write_reg(3, 128);  /* color table at $2000 (128 * $20) */
    ti_vdp_write_reg(4, 0);    /* pattern generator table at $0000 */
    ti_vdp_write_reg(5, 118);  /* sprite attribute table at $3B00 */
    ti_vdp_write_reg(6, 3);    /* sprite pattern table at $1800 */
    ti_vdp_write_reg(7, 1);    /* backdrop color */
}

void ti_cls() {
    int i;
    ti_vdp_set_write_address(TI_NAME_TABLE_BASE);
    i = 0;
    while (i < TI_COLS * TI_ROWS) {
        ti_vdp_write_data(' ');
        i = i + 1;
    }
}

void ti_putc(unsigned char row, unsigned char col, char c) {
    ti_vdp_set_write_address(TI_NAME_TABLE_BASE + row * TI_COLS + col);
    ti_vdp_write_data(c);
}

void ti_puts(unsigned char row, unsigned char col, char *s) {
    int i;
    ti_vdp_set_write_address(TI_NAME_TABLE_BASE + row * TI_COLS + col);
    i = 0;
    while (s[i] != 0) {
        ti_vdp_write_data(s[i]);
        i = i + 1;
    }
}
