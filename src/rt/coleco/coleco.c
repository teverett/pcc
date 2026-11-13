#include "coleco.h"

// Staging cell for a port I/O instruction's own byte - see coleco.h's own
// header comment for why this can't just be a local/parameter.
unsigned char rt_a;

void coleco_out_vdp_data(unsigned char value) {
    rt_a = value;
    asm {
        ld a, [g_rt_a]
        out [190], a  ; VDP data port, $BE
    }
}

void coleco_out_vdp_ctrl(unsigned char value) {
    rt_a = value;
    asm {
        ld a, [g_rt_a]
        out [191], a  ; VDP control/status port, $BF
    }
}

unsigned char coleco_in_vdp_data() {
    asm {
        in a, [190]  ; VDP data port, $BE
        ld [g_rt_a], a
    }
    return rt_a;
}

unsigned char coleco_in_vdp_ctrl() {
    asm {
        in a, [191]  ; VDP control/status port, $BF
        ld [g_rt_a], a
    }
    return rt_a;
}

void coleco_out_ctrl_keypad_mode() {
    rt_a = 0;
    asm {
        ld a, [g_rt_a]
        out [128], a  ; $80 - keypad mode (any address in $80-$9F)
    }
}

void coleco_out_ctrl_joystick_mode() {
    rt_a = 0;
    asm {
        ld a, [g_rt_a]
        out [192], a  ; $C0 - joystick mode (any address in $C0-$DF)
    }
}

unsigned char coleco_in_ctrl1() {
    asm {
        in a, [252]  ; $FC - controller 1
        ld [g_rt_a], a
    }
    return rt_a;
}

unsigned char coleco_in_ctrl2() {
    asm {
        in a, [255]  ; $FF - controller 2
        ld [g_rt_a], a
    }
    return rt_a;
}

void coleco_out_sound(unsigned char value) {
    rt_a = value;
    asm {
        ld a, [g_rt_a]
        out [255], a  ; $FF - SN76489 write port
    }
}

void coleco_vdp_write_reg(unsigned char reg, unsigned char value) {
    coleco_out_vdp_ctrl(value);
    coleco_out_vdp_ctrl(reg | 128);
}

void coleco_vdp_set_write_address(unsigned int addr) {
    coleco_out_vdp_ctrl(addr & 255);
    coleco_out_vdp_ctrl(((addr >> 8) & 63) | 64);
}

void coleco_vdp_set_read_address(unsigned int addr) {
    coleco_out_vdp_ctrl(addr & 255);
    coleco_out_vdp_ctrl((addr >> 8) & 63);
}

void coleco_vdp_write_byte(unsigned char value) { coleco_out_vdp_data(value); }

unsigned char coleco_vdp_read_byte() { return coleco_in_vdp_data(); }

void coleco_vdp_init_text_mode() {
    coleco_vdp_write_reg(0, 0);
    coleco_vdp_write_reg(1, 192);
    coleco_vdp_write_reg(2, 6);    // name table at $1800 ($1800 / $400)
    coleco_vdp_write_reg(3, 255);  // color table - Graphics I default
    coleco_vdp_write_reg(4, 0);    // pattern table at $0000
    coleco_vdp_write_reg(5, 54);   // sprite attribute table at $1B00
    coleco_vdp_write_reg(6, 7);    // sprite pattern table at $3800
    coleco_vdp_write_reg(7, 241);  // text color: white on blue
}

void coleco_cls() {
    int i;
    coleco_vdp_set_write_address(COLECO_NAME_TABLE_BASE);
    i = 0;
    while (i < COLECO_COLS * COLECO_ROWS) {
        coleco_vdp_write_byte(' ');
        i = i + 1;
    }
}

void coleco_putc(unsigned char row, unsigned char col, char c) {
    coleco_vdp_set_write_address(COLECO_NAME_TABLE_BASE + row * COLECO_COLS + col);
    coleco_vdp_write_byte(c);
}

void coleco_puts(unsigned char row, unsigned char col, char *s) {
    int i;
    coleco_vdp_set_write_address(COLECO_NAME_TABLE_BASE + row * COLECO_COLS + col);
    i = 0;
    while (s[i] != 0) {
        coleco_vdp_write_byte(s[i]);
        i = i + 1;
    }
}

unsigned char coleco_controller_read(unsigned char player) {
    coleco_out_ctrl_joystick_mode();
    if (player == 0) return coleco_in_ctrl1();
    return coleco_in_ctrl2();
}
