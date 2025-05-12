#ifndef PINYIN_IME_H
#define PINYIN_IME_H

#define MAX_PINYIN_LEN 20         // Maximum length of Pinyin input
#define MAX_CANDIDATES_PER_PAGE 5 // Maximum candidates to show
#define PINYIN_IME_KEY_NOT_CONSUMED -1 // Special value: key not consumed by IME
#define PINYIN_IME_COMMIT_CHAR 0 // Example, actual values might differ

#include "../haribote/bootpack.h" // For TASK struct and CONSOLE struct

// Structure to map Pinyin strings to GB2312 character codes
struct PinyinMapping {
    char pinyin[MAX_PINYIN_LEN]; // Pinyin string
    unsigned short gb2312_char;  // GB2312 character code (2 bytes)
};

// Public functions for the Pinyin IME
void pinyin_ime_init_task_fields(struct TASK *task);
void pinyin_ime_toggle_mode(struct TASK *task, struct CONSOLE *cons);

// Handles a character input when IME is active.
// Returns: GB2312 char code if committed, 0 if key consumed by IME, PINYIN_IME_KEY_NOT_CONSUMED if key not consumed
unsigned short pinyin_ime_handle_input(struct TASK *task, unsigned char key_code, struct CONSOLE *cons);

// Displays the current Pinyin buffer and candidate list on the console.
void pinyin_ime_display_ui(struct TASK *task, struct CONSOLE *cons);

// Function to draw a GB2312 character on a sheet (defined in graphic.c)
// Corrected signature to match graphic.c definition
void putfont16_gb2312_sht(struct SHEET *sht, int x, int y, int fg_col, int bg_col, unsigned char b1, unsigned char b2);

void pinyin_ime_reset(struct TASK *task); // New declaration

#endif // PINYIN_IME_H
