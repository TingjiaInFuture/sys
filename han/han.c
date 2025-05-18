#include "apilib.h"

// Function to draw a 16x16 character from its dot matrix data
void draw_char_16x16(int win, int x_start, int y_start, unsigned char *char_data, int color) {
    int r, c_bit;
    unsigned char byte_val;
    for (r = 0; r < 16; r++) { // 16 rows
        // Left 8 pixels of the row
        byte_val = char_data[r * 2];
        for (c_bit = 0; c_bit < 8; c_bit++) {
            if ((byte_val << c_bit) & 0x80) { // Check MSB first
                api_point(win, x_start + c_bit, y_start + r, color);
            }
        }
        // Right 8 pixels of the row
        byte_val = char_data[r * 2 + 1];
        for (c_bit = 0; c_bit < 8; c_bit++) {
            if ((byte_val << c_bit) & 0x80) { // Check MSB first
                api_point(win, x_start + 8 + c_bit, y_start + r, color);
            }
        }
    }
    // Refresh the area where the character was drawn
    api_refreshwin(win, x_start, y_start, x_start + 16, y_start + 16);
}

// Function to calculate the offset in HZK16 font file for a GB2312 character
unsigned long calculate_hzk_offset(unsigned char gb_high_byte, unsigned char gb_low_byte) {
    unsigned int qm, wm;
    // GB2312 区码和位码从 0xA1 开始
    // 此处计算的 qm, wm 是基于 1 的索引 (1-94)
    if (gb_high_byte < 0xA1 || gb_high_byte > 0xF7 || gb_low_byte < 0xA1 || gb_low_byte > 0xFE) {
        return 0xffffffff; // Invalid GB code, return a large offset as error indicator
    }
    qm = gb_high_byte - 0xA0;
    wm = gb_low_byte - 0xA0;
    return ((qm - 1) * 94 + (wm - 1)) * 32;
}

void HariMain(void) {
    int win, fh;
    unsigned char *font_buf; // Buffer for one character's font data (32 bytes)
    char *win_buf;           // Buffer for the window's content area
    unsigned long offset_ni, offset_hao;
    int char_color = 0;      // Character color: 0 for black

    // Window content area size
    // For "你好": 2 chars (16px wide each) + 3 gaps (e.g., 5px each)
    // Width: 5 + 16 + 5 + 16 + 5 = 47. Let's use 50.
    // Height: 5 + 16 + 5 = 26. Let's use 30.
    int win_xsiz = 50;
    int win_ysiz = 30;
    
    int x_ni, y_char;
    int x_hao;

    api_initmalloc();
    font_buf = api_malloc(32);
    if (font_buf == 0) {
        api_end();
        return;
    }
    win_buf = api_malloc(win_xsiz * win_ysiz);
    if (win_buf == 0) {
        api_free(font_buf, 32);
        api_end();
        return;
    }

    // Open window. Title "HanZi". col_inv = -1 (no transparency)
    win = api_openwin(win_buf, win_xsiz, win_ysiz, -1, "HanZi");
    if (win == 0) { // api_openwin returns 0 on error
        api_free(font_buf, 32);
        api_free(win_buf, win_xsiz * win_ysiz); // win_buf is managed by app if openwin fails early
        api_end();
        return;
    }

    // Fill window background (e.g., white = 7)
    // Coordinates for boxfilwin are relative to window's content area (0,0 to xsiz-1, ysiz-1)
    api_boxfilwin(win, 0, 0, win_xsiz - 1, win_ysiz - 1, 7); // 7 for white

    fh = api_fopen("HZK16"); // Assumes HZK16 is in the root directory
    if (fh == 0) { // api_fopen returns 0 on error
        api_closewin(win); // This should free win_buf
        api_free(font_buf, 32);
        api_end();
        return;
    }

    // Calculate positions for characters
    y_char = (win_ysiz - 16) / 2; // Vertically centered
    x_ni = 5;                     // X position for "你"
    x_hao = x_ni + 16 + 5;        // X position for "好" (after "你" + gap)


    // Character "你" (GB2312: C4 E3)
    offset_ni = calculate_hzk_offset(0xC4, 0xE3);
    if (offset_ni != 0xffffffff) {
        api_fseek(fh, offset_ni, 0); // Mode 0: seek from start of file
        if (api_fread(font_buf, 32, fh) == 32) {
            draw_char_16x16(win, x_ni, y_char, font_buf, char_color);
        }
    }

    // Character "好" (GB2312: BA C3)
    offset_hao = calculate_hzk_offset(0xBA, 0xC3);
     if (offset_hao != 0xffffffff) {
        api_fseek(fh, offset_hao, 0); // Mode 0: seek from start of file
        if (api_fread(font_buf, 32, fh) == 32) {
            draw_char_16x16(win, x_hao, y_char, font_buf, char_color);
        }
    }

    api_fclose(fh);

    // Wait for Enter key to close
    for (;;) {
        if (api_getkey(1) == 0x0a) { // 0x0a is Enter key
            break;
        }
    }
    
    api_closewin(win); // This should free win_buf
    api_free(font_buf, 32);
    api_end();
}
