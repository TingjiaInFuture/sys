#include "apilib.h"

// Window layout constants (moved earlier for visibility and use in draw_char_16x16)
#define COMPOSED_CHAR_DISPLAY_WIDTH 16
#define COMPOSED_CHAR_GAP 1
#define COMPOSED_CHAR_TOTAL_WIDTH (COMPOSED_CHAR_DISPLAY_WIDTH + COMPOSED_CHAR_GAP)

#define X_COMPOSED_START 8
// Calculate WIN_XSIZ to fit the maximum composed text length, plus some margin
// This makes the window width adaptive at compile-time to MAX_COMPOSED_TEXT_LEN.
// Note: This will increase memory usage for win_buf significantly if MAX_COMPOSED_TEXT_LEN is large.
// Original WIN_XSIZ was 240. For MAX_COMPOSED_TEXT_LEN = 50, this becomes 8 + 50*17 + 8 = 866.
// For MAX_COMPOSED_TEXT_LEN = 20, this becomes 8 + 20*17 + 8 = 356.
#define WIN_XSIZ (X_COMPOSED_START + MAX_COMPOSED_TEXT_LEN * COMPOSED_CHAR_TOTAL_WIDTH + X_COMPOSED_START) //左右留边距
#define WIN_YSIZ 80    // Reduced Y size to potentially fix allocation issues and match earlier comments -> Increased Y size
#define CHAR_COLOR 0   // Black
#define BG_COLOR 7     // White

#define Y_COMPOSED 8
#define X_COMPOSED_START 8

#define Y_PINYIN (Y_COMPOSED + 16 + 3)
#define X_PINYIN 8
#define WIDTH_PINYIN_AREA (WIN_XSIZ - 16)
#define HEIGHT_PINYIN_AREA 12

#define Y_CANDIDATES (Y_PINYIN + 12 + 3)
#define X_CANDIDATES 8
#define WIDTH_CANDIDATES_AREA (WIN_XSIZ - 16)
#define HEIGHT_CANDIDATES_AREA (WIN_YSIZ - Y_CANDIDATES - 8) // Dynamically calculate based on Y_CANDIDATES and a bottom margin

// Simple string length function
int my_strlen(const char *s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

// Simple string n compare function
int my_strncmp(const char *s1, const char *s2, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

// Function to draw a 16x16 character from its dot matrix data
void draw_char_16x16(int win, int x_start, int y_start, unsigned char *char_data, int color) {
    int r, c_bit;
    unsigned char byte_val;
    for (r = 0; r < 16; r++) { // 16 rows
        // Left 8 pixels of the row
        byte_val = char_data[r * 2];
        for (c_bit = 0; c_bit < 8; c_bit++) {
            if ((byte_val << c_bit) & 0x80) { // Check if the (7-c_bit)-th bit is set
                api_point(win, x_start + c_bit, y_start + r, color);
            }
        }
        // Right 8 pixels of the row
        byte_val = char_data[r * 2 + 1];
        for (c_bit = 0; c_bit < 8; c_bit++) {
            if ((byte_val << c_bit) & 0x80) {
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
    if (gb_high_byte < 0xA1 || gb_high_byte > 0xF7 || gb_low_byte < 0xA1 || gb_low_byte > 0xFE) {
        return 0xffffffff; // Invalid GB code
    }
    qm = gb_high_byte - 0xA0;
    wm = gb_low_byte - 0xA0;
    return ((qm - 1) * 94 + (wm - 1)) * 32;
}

// New function to draw a 16x16 character to a memory buffer
void draw_char_16x16_to_buffer(char *win_buf, int win_width, int x_start, int y_start, unsigned char *char_data, int color) {
    int r, c_bit;
    unsigned char byte_val;
    char *p; // Pointer to the current pixel in win_buf

    // Add boundary checks to prevent buffer overflow
    if (x_start < 0 || y_start < 0 || x_start + 16 > win_width || y_start + 16 > WIN_YSIZ) { // Use WIN_YSIZ
        return; // Skip drawing if out of bounds
    }

    for (r = 0; r < 16; r++) { // 16 rows
        int current_y = y_start + r;
        if (current_y >= WIN_YSIZ) break; // Additional safety check using WIN_YSIZ
        
        // Left 8 pixels of the row
        byte_val = char_data[r * 2];
        p = win_buf + current_y * win_width + x_start;
        for (c_bit = 0; c_bit < 8; c_bit++) {
            if (x_start + c_bit < win_width && (byte_val << c_bit) & 0x80) {
                p[c_bit] = color;
            }
        }
        // Right 8 pixels of the row
        byte_val = char_data[r * 2 + 1];
        p = win_buf + current_y * win_width + x_start + 8;
        for (c_bit = 0; c_bit < 8; c_bit++) {
            if (x_start + 8 + c_bit < win_width && (byte_val << c_bit) & 0x80) {
                p[c_bit] = color;
            }
        }
    }
    // No api_refreshwin here; caller will refresh the relevant window area.
}


#define MAX_PINYIN_LEN 10
#define MAX_CANDIDATES 5
#define MAX_COMPOSED_TEXT_LEN 20 // Reduced from 50 to decrease window width and memory usage

typedef struct {
    char pinyin[MAX_PINYIN_LEN];
    char hanzi_display[3]; // For display "你" etc. (not used for drawing, just for reference)
    unsigned char gb_high_byte;
    unsigned char gb_low_byte;
} HanziEntry;

HanziEntry dictionary[] = {
    {"ni", "你", 0xC4, 0xE3},
    {"hao", "好", 0xBA, 0xC3},
    {"ma", "吗", 0xC2, 0xF0},
    {"wo", "我", 0xCE, 0xD2},
    {"ta", "他", 0xCB, 0xFB},
    {"men", "们", 0xC3, 0xC7},
    {"ren", "人", 0xC8, 0xCB},
    {"da", "大", 0xB4, 0xF3},
    {"zhong", "中", 0xD6, 0xD0},
    {"guo", "国", 0xB9, 0xFA}
};
int dictionary_size = sizeof(dictionary) / sizeof(dictionary[0]);

char pinyin_input[MAX_PINYIN_LEN + 1];
int pinyin_len = 0;
HanziEntry candidates[MAX_CANDIDATES];
int num_candidates = 0;

// Buffer for composed Hanzi (GB2312 pairs)
unsigned char composed_text_gb2312[MAX_COMPOSED_TEXT_LEN * 2];
int composed_text_len = 0; // Number of Hanzi characters

void find_matching_pinyin_entries(const char *p_input, int p_len_current, HanziEntry *cand_dest, int *cand_count) {
    *cand_count = 0;
    if (p_len_current == 0) return;
    int i;
    for (i = 0; i < dictionary_size && *cand_count < MAX_CANDIDATES; i++) {
        // Modified condition: Check if p_input is a prefix of dictionary[i].pinyin
        if (my_strncmp(dictionary[i].pinyin, p_input, p_len_current) == 0) {
            cand_dest[*cand_count] = dictionary[i];
            (*cand_count)++;
        }
    }
}

void redraw_dynamic_content(int win, char *win_buf, int win_width, unsigned char *font_buf, int fh) {
    int i;    // Clear and draw Pinyin input string
    api_boxfilwin(win, X_PINYIN, Y_PINYIN, X_PINYIN + WIDTH_PINYIN_AREA -1 , Y_PINYIN + HEIGHT_PINYIN_AREA -1, BG_COLOR);
    if (pinyin_len > 0) {
        api_putstrwin(win, X_PINYIN, Y_PINYIN, CHAR_COLOR, pinyin_len, pinyin_input);
    }
    api_refreshwin(win, X_PINYIN, Y_PINYIN, X_PINYIN + WIDTH_PINYIN_AREA, Y_PINYIN + HEIGHT_PINYIN_AREA);

    // Clear and draw Candidates
    // Calculate the maximum possible y extent for candidates to clear the entire potential area
    int max_cand_y_extent = Y_CANDIDATES + (MAX_CANDIDATES * 16); // 16 is the height of a candidate char
    if (max_cand_y_extent > WIN_YSIZ - 8) { // Ensure it doesn't exceed window bounds (with a small margin)
        max_cand_y_extent = WIN_YSIZ - 8;
    }
    api_boxfilwin(win, X_CANDIDATES, Y_CANDIDATES, X_CANDIDATES + WIDTH_CANDIDATES_AREA -1, max_cand_y_extent -1 , BG_COLOR);

    int y_curr_cand = Y_CANDIDATES;
    for (i = 0; i < num_candidates; i++) {
        char num_label[4] = {' ', '.', ' ', '\0'}; // 修正：使用正确的空终止符 '\0'
        num_label[0] = (i + 1) + '0';
        api_putstrwin(win, X_CANDIDATES, y_curr_cand, CHAR_COLOR, my_strlen(num_label), num_label);

        unsigned long offset = calculate_hzk_offset(candidates[i].gb_high_byte, candidates[i].gb_low_byte);
        if (fh != 0 && offset != 0xffffffff) {
            api_fseek(fh, offset, 0); // 0 for SEEK_SET
            if (api_fread(font_buf, 32, fh) == 32) {
                draw_char_16x16_to_buffer(win_buf, win_width, X_CANDIDATES + 50, y_curr_cand, font_buf, CHAR_COLOR);
            }
        }
        y_curr_cand += 16; // Spacing for candidate characters
    }
    // Refresh the candidates area, ensuring the refresh height doesn't exceed the window
    int refresh_height_cand = Y_CANDIDATES + (num_candidates * 16);
    if (refresh_height_cand > WIN_YSIZ -8) {
        refresh_height_cand = WIN_YSIZ - 8;
    }
    if (num_candidates > 0) { // Only refresh if there are candidates
      api_refreshwin(win, X_CANDIDATES, Y_CANDIDATES, X_CANDIDATES + WIDTH_CANDIDATES_AREA, refresh_height_cand);
    } else { // If no candidates, still refresh the base candidate area to clear previous ones
      api_refreshwin(win, X_CANDIDATES, Y_CANDIDATES, X_CANDIDATES + WIDTH_CANDIDATES_AREA, Y_CANDIDATES + 16); // Refresh at least one line height
    }
}

void draw_composed_char(char *win_buf, int win_width, unsigned char* font_buf, int fh, unsigned char gb_high, unsigned char gb_low, int x, int y) {
    unsigned long offset = calculate_hzk_offset(gb_high, gb_low);
    if (fh != 0 && offset != 0xffffffff) {
        api_fseek(fh, offset, 0);
        if (api_fread(font_buf, 32, fh) == 32) {
            draw_char_16x16_to_buffer(win_buf, win_width, x, y, font_buf, CHAR_COLOR);
        }
    }
}


void HariMain(void) {
    int win, fh;
    unsigned char *font_buf; // Buffer for one character's font data (32 bytes)
    char *win_buf;           // Buffer for the window's content area
    
    pinyin_input[0] = '\0';
    pinyin_len = 0;
    num_candidates = 0;
    composed_text_len = 0;

    api_initmalloc();
    font_buf = api_malloc(32);
    if (font_buf == 0) {
        api_putstr0("Error: failed to allocate font_buf\\n"); // 添加错误输出
        api_end();
        return;
    }
    win_buf = api_malloc(WIN_XSIZ * WIN_YSIZ);
    if (win_buf == 0) {
        api_putstr0("Error: failed to allocate win_buf\\n"); // 添加错误输出
        api_free(font_buf, 32);
        api_end();
        return;
    }

    win = api_openwin(win_buf, WIN_XSIZ, WIN_YSIZ, -1, "Simple IME");
    if (win == 0) {
        api_putstr0("Error: failed to open window\\n"); // 添加错误输出
        api_free(font_buf, 32);
        api_free(win_buf, WIN_XSIZ * WIN_YSIZ);
        api_end();
        return;
    }

    api_boxfilwin(win, 0, 0, WIN_XSIZ - 1, WIN_YSIZ - 1, BG_COLOR); // Initial clear
    api_refreshwin(win, 0, 0, WIN_XSIZ, WIN_YSIZ);
    fh = api_fopen("HZK16"); // Assumes HZK16 is in the root directory
    // No error check for fh here, draw_char functions will handle fh == 0
    if (fh == 0) { // 虽然原代码注释说draw_char会处理，但这里也可以加个提示
        api_putstr0("Warning: failed to open HZK16. Chinese characters may not display.\\n");
    }


    redraw_dynamic_content(win, win_buf, WIN_XSIZ, font_buf, fh);

    int current_composed_x = X_COMPOSED_START;

    for (;;) {
        int key = api_getkey(1);
        int needs_redraw = 0;

        if (key >= 'a' && key <= 'z') {
            if (pinyin_len < MAX_PINYIN_LEN) {
                pinyin_input[pinyin_len++] = key;
                pinyin_input[pinyin_len] = '\0';
                find_matching_pinyin_entries(pinyin_input, pinyin_len, candidates, &num_candidates);
                needs_redraw = 1;
            }
        } else if (key >= '1' && key <= ('0' + num_candidates)) {
            int selected_idx = key - '1';
            if (selected_idx < num_candidates && composed_text_len < MAX_COMPOSED_TEXT_LEN) {
                HanziEntry selected_char = candidates[selected_idx];
                
                // Add to composed text buffer
                composed_text_gb2312[composed_text_len * 2] = selected_char.gb_high_byte;
                composed_text_gb2312[composed_text_len * 2 + 1] = selected_char.gb_low_byte;
                composed_text_len++;
                // Draw the selected character in composed area
                draw_composed_char(win_buf, WIN_XSIZ, font_buf, fh, selected_char.gb_high_byte, selected_char.gb_low_byte, current_composed_x, Y_COMPOSED);
                api_refreshwin(win, current_composed_x, Y_COMPOSED, current_composed_x + COMPOSED_CHAR_DISPLAY_WIDTH, Y_COMPOSED + 16); // Refresh the newly drawn char (use COMPOSED_CHAR_DISPLAY_WIDTH for actual char width)
                current_composed_x += COMPOSED_CHAR_TOTAL_WIDTH; // Use macro for consistency

                // Clear pinyin input and candidates
                pinyin_len = 0;
                pinyin_input[0] = '\0';
                num_candidates = 0;
                needs_redraw = 1;
            }
        } else if (key == 8) { // Backspace key (ASCII code)
            if (pinyin_len > 0) {
                pinyin_len--;
                pinyin_input[pinyin_len] = '\0';
                // 重新查找匹配的拼音条目
                if (pinyin_len > 0) {
                    find_matching_pinyin_entries(pinyin_input, pinyin_len, candidates, &num_candidates);
                } else {
                    // 如果拼音输入为空，清空候选项
                    num_candidates = 0;
                }
                needs_redraw = 1;
            } else if (composed_text_len > 0) { // Backspace for composed text
                composed_text_len--;
                current_composed_x -= COMPOSED_CHAR_TOTAL_WIDTH; // Move cursor back

                // Clear the area of the deleted character in the window buffer
                int x_clear_start = current_composed_x;
                int y_clear_start = Y_COMPOSED;
                int clear_width = COMPOSED_CHAR_TOTAL_WIDTH; // Clear character + gap
                int clear_height = 16; // Character height
                int r, c; // Declare r and c outside the loops for C89 compatibility

                for (r = 0; r < clear_height; ++r) {                    for (c = 0; c < clear_width; ++c) {
                        int win_x = x_clear_start + c;
                        int win_y = y_clear_start + r;
                        if (win_x >= 0 && win_x < WIN_XSIZ && win_y >= 0 && win_y < WIN_YSIZ) {
                             win_buf[win_y * WIN_XSIZ + win_x] = BG_COLOR;
                        }
                    }
                }
                // Refresh the cleared area on screen
                api_refreshwin(win, x_clear_start, y_clear_start, x_clear_start + clear_width, y_clear_start + clear_height);
                // No explicit needs_redraw = 1 here, as this part is self-contained.
            }
        } else if (key == 10) { // Enter key (ASCII code)
            if (pinyin_len == 0 && num_candidates == 0) { // Exit if input is empty
                break;
            }
            // Optional: if pinyin input is not empty, could try to select first candidate or clear
            // For now, just clears if not empty, or exits if empty
             pinyin_len = 0;
             pinyin_input[0] = '\0';
             num_candidates = 0;
             needs_redraw = 1;
        }
        if (needs_redraw) {
            redraw_dynamic_content(win, win_buf, WIN_XSIZ, font_buf, fh);
        }
    }
    if (fh != 0) {
        api_fclose(fh);
    }
    api_closewin(win);
    api_free(font_buf, 32);
    api_free(win_buf, WIN_XSIZ * WIN_YSIZ);
    api_end();
}
